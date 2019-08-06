/*
 * General IRC protocol: maintain a connection socket and make it available
 * Provide a command to send raw IRC messages back
 */

#include <sys/socket.h>
#include <netdb.h>
#include <openssl/ssl.h>

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <err.h>

#include "irc.h"
#include <config.h>

struct irc_con {
	/* Connection handling */
	char *name;			/* Network name as by ISUPPORT */
	char *botname[NSOCKS];		/* Reported bot's name by socket */
	int cid;			/* Connection's ID */
	int socket[NSOCKS];	    	/* Connection's socket */
	int wsocket;			/* Socket we're currently writing from */
	SSL *ssl[NSOCKS];     		/* Connection's SSL context */
	pthread_mutex_t conmtx;		/* Connection's socket mutex */
	struct irc_info *ircinfo;	/* IRC info tied to the connection */
};

struct handler_data {
	struct irc_con *ircc;
	int sn;
};

void *irc_handler(void *);
void register_irc_client(struct irc_con *);
void setup_irc_connection(struct irc_con *, int n);
void handle_message(int, char *, int);
ssize_t read_line(struct irc_con *, char *, int);
ssize_t get_irc_message(struct irc_con *, char *, int);

static struct irc_con *irc_connections[32];
pthread_mutex_t empty_mtx = PTHREAD_MUTEX_INITIALIZER;

extern struct irc_info ircinfo[];
extern char *irc_suffixlist[];

/*
 * setup a socket and start a thread to the irc_watcher
 * return 0 on success
 */
void
irc_main(int forever)
{
	struct irc_con *ircc;
	pthread_t thread;
	setup_core_hooks();

	int i, j;
	for (i = 0; ircinfo[i].host != NULL; i++) {
		ircc = malloc(sizeof(struct irc_con));
		if (ircc == NULL)
			err(1, "Failed to allocate memory");

		memset(ircc, 0, sizeof(struct irc_con));

		irc_connections[i] = ircc;
		ircc->cid = i;
		ircc->wsocket = 0;

		ircc->ircinfo = ircinfo + i;
		ircc->conmtx = empty_mtx;

		for (j = 0; j < NSOCKS; j++) {
			setup_irc_connection(ircc, j);
			if (ircc->socket[j] == -1)
				err(1, "Failed to setup the socket for %s", ircinfo[i].host);
		}

		register_irc_client(ircc);

		for (j = 0; j < NSOCKS; j++) {
			struct handler_data *hdata = malloc(sizeof(struct handler_data));
			if (hdata == NULL)
				err(1, "Failed to allocate memory");

			hdata->ircc = ircc;
			hdata->sn = j;

			if (forever && ircinfo[i + 1].host == NULL && j == NSOCKS - 1)
				irc_handler(hdata);
			else
				pthread_create(&thread, NULL, irc_handler, (void *)hdata);
		}
	}

	return;
}

void *
irc_handler(void *hdata)
{
	struct irc_con *ircc = ((struct handler_data *)hdata)->ircc;
	int sn = ((struct handler_data *)hdata)->sn;

	ssize_t msgsize;
	char msg[IRC_MSG_MAX];

	while (1) {
		msgsize = get_irc_message(ircc, msg, sn);

		if (msgsize <= 0) {
			close(ircc->socket[sn]);
			if (ircc->ircinfo->use_ssl)
				SSL_shutdown(ircc->ssl[sn]);
			setup_irc_connection(ircc, sn);
		} else {
			handle_message(ircc->cid, msg, sn);
		}
	}
}

/*
 * setup the tcp IRC socket
 * Inspired by tcpopen by the ii devs, credits to them
 * https://git.suckless.org/ii/tree/ii.c#n363
 *
 * Return an IRC socket, or -1 if there's an error.
 */
void
setup_irc_connection(struct irc_con *ircc, int n)
{
	struct addrinfo hints, *orig_ainfo, *ainfo = NULL;
	int ret, sock = -1;
	SSL *ssl_struct = NULL;

	memset(&hints, 0, sizeof(struct addrinfo));

	/* setup hints */
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_NUMERICSERV;
	hints.ai_socktype = SOCK_STREAM;

	ret = getaddrinfo(ircc->ircinfo->host, ircc->ircinfo->port, &hints, &ainfo);
	if (ret) {
		ircc->socket[n] = -1;
	}

	if (ircc->ircinfo->use_ssl) {
		SSL_load_error_strings();
		(void) SSL_library_init();
		OpenSSL_add_all_algorithms();
		SSL_CTX *sslctx = SSL_CTX_new(TLSv1_client_method());
		ssl_struct = SSL_new(sslctx);

		if (ssl_struct == NULL)
			err(1, "Failed to setup SSL");
	}

	orig_ainfo = ainfo;

	while (ainfo != NULL && sock == -1) {
		sock = socket(ainfo->ai_family, ainfo->ai_socktype,
			ainfo->ai_protocol);
		ainfo = ainfo->ai_next;
		if (sock == -1)
			continue;

		ret = connect(sock, ainfo->ai_addr, ainfo->ai_addrlen);
		if (ret) {
			close(sock);
			sock = -1;
			continue;
		}

		if (ircc->ircinfo->use_ssl) {
			ret = SSL_set_fd(ssl_struct, sock);
			if (ret != 1) {
				close(sock);
				ret = SSL_get_error(ssl_struct, ret);
				err(ret, "Failed to link the socket to SSL");
			}

			ret = SSL_connect(ssl_struct);
			if (ret != 1) {
				close(sock);
				ret = SSL_get_error(ssl_struct, ret);
				err(ret, "Failed to do handshake");
			}
		}
	}

	if (ircc->ircinfo->use_ssl)
		ircc->ssl[n] = ssl_struct;
	ircc->socket[n] = sock;

	freeaddrinfo(orig_ainfo);
}

/*
 * send NICK then USER with the correct parameters
 * as by RFC
 */
void
register_irc_client(struct irc_con *ircc)
{
	char nick_cmd[IRC_MSG_MAX];
	char user_cmd[IRC_MSG_MAX];
	char suffix[32];
	suffix[0] = '\0';
	int i;
	int suflist_len;

	for (i = 0; irc_suffixlist[i] != NULL; i++);
	suflist_len = i;

	for (i = 0; i < NSOCKS; i++) {
		if (NSOCKS != 1) {
			if (suflist_len < NSOCKS)
				snprintf(suffix, 32, "%d", i);
			else
				strncpy(suffix, irc_suffixlist[i], 32);
		}

		if (ircc->ircinfo->pass != NULL) {
			char pass_cmd[IRC_MSG_MAX];
			snprintf(pass_cmd, IRC_MSG_MAX, "PASS %s",
			    ircc->ircinfo->pass);
			send_irc_message(ircc->cid, pass_cmd, 0, i);
		}

		snprintf(nick_cmd, IRC_MSG_MAX, "NICK %s%s",
		    ircc->ircinfo->name, suffix);
		snprintf(user_cmd, IRC_MSG_MAX, "USER %s%s * * :%s",
		    ircc->ircinfo->name, suffix, ircc->ircinfo->botrn);
		send_irc_message(ircc->cid, nick_cmd, 0, i);
		send_irc_message(ircc->cid, user_cmd, 0, i);
	}

	return;
}

ssize_t
get_irc_message(struct irc_con *ircc, char *buf, int sn)
{
	ssize_t msgsize;

	msgsize = read_line(ircc, buf, sn);

	if (irc_verbose) {
		write(1, "-> ", 3);
		write(1, buf, msgsize);
	}

	return msgsize;
}

int
send_irc_message(int cid, const char *msg, int allsocks, int sn)
{
	struct irc_con *ircc = irc_connections[cid];

	/* strip out any \r \n and add our own properly */
	int i, ret = 0;
	int len = strlen(msg);
	char lmsg[IRC_MSG_MAX];
	strcpy(lmsg, msg);

	for (i = 0; i < len; i++)
		if (lmsg[i] == '\r' || lmsg[i] == '\n')
			lmsg[i] = '\0';

	len = strlen(lmsg);
	lmsg[len] = '\r';
	lmsg[len + 1] = '\n';
	lmsg[len + 2] = '\0';

	len += 2;
	if (irc_verbose) {
		write(1, "<- ", 3);
		write(1, lmsg, len);
		//printf("Hex: ");
		//for (i = 0; i < len; i++)
		//	printf("0x%x", lmsg[i]);
		//putchar('\n');
	}

	pthread_mutex_lock(&ircc->conmtx);

	if (!allsocks) {
		int target;
		if (sn != -1)
			target = sn;
		else
			target = ircc->wsocket;

		if (ircc->ircinfo->use_ssl)
			ret = (SSL_write(ircc->ssl[target], lmsg, len) <= 0);
		else
			ret = (write(ircc->socket[target], lmsg, len) == -1);

		if (sn == -1)
			ircc->wsocket = (ircc->wsocket + 1) % NSOCKS;
	} else {
		for (i = 0; i < NSOCKS; i++) {
			if (ircc->ircinfo->use_ssl)
				ret = (SSL_write(ircc->ssl[i], lmsg, len) <= 0);
			else
				ret = (write(ircc->socket[i], lmsg, len) == -1);
		}
	}

	pthread_mutex_unlock(&ircc->conmtx);

	return ret;
}

struct irc_info *
get_ircinfo(int cid)
{
	if (irc_connections[cid] == NULL)
		return NULL;

	return irc_connections[cid]->ircinfo;
}

void
set_network_name(int cid, const char *name)
{
	free(irc_connections[cid]->name);
	irc_connections[cid]->name = strdup(name);
	return;
}

const char *
get_network_name(int cid)
{
	return irc_connections[cid]->name;
}

const char *
get_userdef_network_name(int cid)
{
	return irc_connections[cid]->ircinfo->network;
}

int
get_cid_from_network_name(const char *network)
{
	int i;

	if (network == NULL)
		return -1;

	for (i = 0; irc_connections[i] != NULL; i++) {
		if (irc_connections[i]->ircinfo != NULL &&
		    irc_connections[i]->ircinfo->network != NULL &&
		    strcmp(irc_connections[i]->ircinfo->network, network) == 0)
			return i;
		else if (irc_connections[i]->name != NULL &&
		    strcmp(irc_connections[i]->name, network) == 0)
			return i;
	}

	return -1;
}

void
set_bot_name(int cid, int sn, char *name)
{
	if (irc_connections[cid]->botname[sn] != NULL)
		free(irc_connections[cid]->botname[sn]);
	irc_connections[cid]->botname[sn] = strdup(name);
	return;
}

char *
get_bot_name(int cid, int sn)
{
	return irc_connections[cid]->botname[sn];
}

int
irc_get_network_count()
{
	int i;
	for (i = 0; ircinfo[i].host != NULL; i++)
		;

	return i;
}

ssize_t
read_line(struct irc_con *ircc, char *buf, int sn)
{
	char c = '\0';
	int i = 0;
	int ret = 0;

	while (c != '\n' && i < IRC_MSG_MAX) {
		/* Socket 0 is the lead for now */
		if (ircc->ircinfo->use_ssl)
			ret = SSL_read(ircc->ssl[sn], &c, 1);
		else
			ret = read(ircc->socket[sn], &c, 1);

		if (ret <= 0)
			/* the connection is probably closed */
			return -1;
		buf[i++] = c;
	}


	buf[i] = '\0';
	return i;
}

/*
 * parse out the IRC command and hook it to its proper hooks
 */ 
void
handle_message(int cid, char *msg, int sn)
{
	char command[64];
	get_command(msg, command, 64);
	if (sn == 0 || strcmp(command, "PRIVMSG") != 0)
		run_hooks(cid, command, msg, sn);

	return;
}
