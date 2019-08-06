#include <pcre.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <irc/irc.h>
#include <config.h>
#include "vinny.h"
#include "ops.h"


#ifdef WITH_PG
#include <libpq-fe.h>

char *pgmatch(PGresult *, char **, struct ircmsg *);
int strip_action(char *);

static PGconn *pgconn;

static const char *pgkeywords[] = {
	"dbname", "user", "password", "host", "port", NULL
};
static const char *pgvalues[] = {
	PG_DB, PG_USER, PG_PASS, PG_HOST, PG_PORT, NULL
};

static const char *create_query = "CREATE TABLE IF NOT EXISTS irc_logs (network VARCHAR(32), channel VARCHAR(128), nick VARCHAR(64), text VARCHAR(512), action BOOLEAN, time TIMESTAMP DEFAULT NOW());";

static pthread_mutex_t dbmtx = PTHREAD_MUTEX_INITIALIZER;
#endif /* WITH_PG */

int
setup_logs()
{
#ifdef WITH_PG
	/* Connect to pgsql */
	pg_connected = 0;
	pgconn = PQconnectdbParams(pgkeywords, pgvalues, 0);
	if (pgconn != NULL) {
		pg_connected = 1;
		PQexec(pgconn, create_query);
	}
#endif /* WITH_PG */

	return 0;
}

void *
log_msg(void *msgp)
{
	struct ircmsg *msg = (struct ircmsg *)msgp;

#ifdef WITH_PG
	if (msg == NULL || msg->channel == NULL || !pg_connected)
		goto log_msg_exit;

	const char *network, *channel, *nick;
	char *text;
	int action;

	/* pgsql parameters */
	network = get_network_name(msg->cid);
	channel = msg->channel;
	nick = msg->sender;
	text = strdup(msg->text);
	action = strip_action(text);
	char *acttext = action ? "true" : "false";

	/* Add whether the message is an action as a bool */
	char *command = "INSERT INTO irc_logs VALUES ($1, $2, $3, $4, $5);";
	const int nParams = 5;
	const Oid ids[5] = { 1043, 1043, 1043, 1043, 16 };
	const char *values[5] = { network, channel, nick, text, acttext };
	const int lengths[5] = { strlen(network), strlen(channel), strlen(nick),
	    strlen(text), strlen(acttext) };
	const int formats[5] = { 0, 0, 0, 0, 0 };
	int resultFormat = 1;

	pthread_mutex_lock(&dbmtx);
	PGresult *pgres = PQexecParams(pgconn, command, nParams, ids, values, lengths, formats, resultFormat);
	pthread_mutex_unlock(&dbmtx);

	PQclear(pgres);
	free(text);

log_msg_exit:
#endif /* WITH_PG */

	pthread_detach(pthread_self());
	unregister_msg_holder(msg, pthread_self());

	return NULL;
}

char *
access_logs(char **regexes, struct ircmsg *msg)
{
#ifdef WITH_PG

	if (msg == NULL || msg->channel == NULL || regexes == NULL || regexes[0] == NULL || !pg_connected)
		return NULL;

	const char *network = get_network_name(msg->cid);
	const char *channel = msg->channel;

	const char *command = "SELECT nick, text, action FROM irc_logs WHERE network = $1 AND channel = $2 ORDER BY time DESC LIMIT 250;";
	const int nParams = 2;
	const Oid ids[2] = { 1043, 1043 };
	const char *values[2] = { network, channel };
	const int lengths[2] = { strlen(network), strlen(channel) };
	const int formats[2] = { 0, 0 };
	int resultFormat = 0;

	pthread_mutex_lock(&dbmtx);
	PGresult *pgres = PQexecParams(pgconn, command, nParams, ids, values, lengths, formats, resultFormat);
	pthread_mutex_unlock(&dbmtx);

	if (pgres == NULL)
		return NULL;

	char *log = pgmatch(pgres, regexes, msg);

	PQclear(pgres);
	return log;

#else
	return 0;
#endif /* WITH_PG */
}

#ifdef WITH_PG

char *
pgmatch(PGresult *pgres, char **regexes, struct ircmsg *msg)
{
	int i, j;
	char *text;
	char *nick;
	char *action;
	char *log = NULL;

	int reslen = PQntuples(pgres);
	if (reslen == 0)
		return NULL;

	pcre *rx;
	pcre_extra *rxx;
	const char *errstr = NULL;
	int erroffset = 0;
	int ovec[30];

	for (i = 0; regexes[i] != NULL && log == NULL; i++) {
		rx = pcre_compile(regexes[i], 0, &errstr, &erroffset, NULL);
		if (rx == NULL)
			continue;

		rxx = pcre_study(rx, 0, &errstr);

		for (j = 0; j < reslen; j++) {
			/* text is stored in column 1 */
			text = PQgetvalue(pgres, j, 1);
			if (strcmp(text, msg->text) == 0 ||
			    strncmp(text, "s/", 2) == 0)
				continue;

			if (pcre_exec(rx, rxx, text, strlen(text), 0, 0, ovec, 30) >= 1) {
				nick = PQgetvalue(pgres, j, 0);
				action = PQgetvalue(pgres, j, 2);
				log = malloc(512);

				/* Check if the message is an action and change the formatting if it is */
				if (*action == 't') {
					snprintf(log, 512, "%s * %s\n", nick, text);
				} else if (*action == 'f') {
					snprintf(log, 512, "<%s> %s\n", nick, text);
				}

				break;
			}
		}

		free(rx);
		pcre_free_study(rxx);
	}

	return log;
}

int
strip_action(char *text)
{
	int i, n = 0, len = strlen(text);

	if (text[0] == '\001' && strncmp(text + 1, "ACTION", 6) == 0 && text[len - 1] == '\001') {
		n = 8;
		text[len - 1] = '\0';
		for (i = 0; i < len; i++) {
			text[i] = text[i + n];
		}

		return 1;
	} else {
		return 0;
	}
}

#endif /* WITH_PG */
