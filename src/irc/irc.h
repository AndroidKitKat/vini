#ifndef IRC_H
#define IRC_H
#include <signal.h>

#define IRC_MSG_MAX (512 + 1)	/* add 1 for the null byte */
#define IRC_CMD_TABLE_MAX 1024

struct irc_info {
	/* Initial settings */
	char *name;		/* bot's nickname and username */
	char *botrn;		/* bot's real name */
	char *network;		/* network name */
	char *host;		/* server's name */
	char *port;		/* port we're connecting through */
	char *pass;		/* IRC server's password */
	char *channels;		/* channels to join */
	int use_ssl;		/* whether we're using SSL */

	/* NickServ information */
	int do_login;		/* whether to attempt login */
	char *ns_username;	/* username to send to nickserv */
	char *ns_password;	/* password to send to nickserv */
};

struct irc_module {
	int (*init)(void);
	void (*free)(void);
	int (*reload)(void);
	void (*sigchld)(pid_t);
};

int irc_verbose;	/* whether to print raw IRC messages to stdout */

/*
 * irc.c functions
 */

/* main IRC command: setup IRC, get messages and distribute them */
void irc_main(int);

/* send an RAW irc message */
int send_irc_message(int, const char *, int, int);

struct irc_info *get_ircinfo(int);

void set_network_name(int, const char *);
const char *get_network_name(int);
const char *get_userdef_network_name(int);
int get_cid_from_network_name(const char *);
int irc_get_network_count(void);

char *get_bot_name(int, int);
void set_bot_name(int, int, char *);


/*
 * Hooks interface (hooks.c)
 * The structure for the hooks will be a hash table pointing to a linked
 * list provided by queue.h macros.
 */

struct hook_msg {
	const char *raw;
	int fid;
	int sn;
	int cid;
};

int add_irc_hook(const char *, void (*)(const struct hook_msg *), void (*)(int), int);
int rm_irc_hook(const char *, void (*)(const struct hook_msg *), int);
int rm_irc_hooks(const char *, void (*)(const struct hook_msg *));
void run_hooks(int, const char *key, const char *msg, int);


/*
 * Core hooks (core.c)
 */
void setup_core_hooks();



/*
 * ircparser.c functions
 */
char *get_sender(const char *msg, char *buf, int size);
char *get_command(const char *msg, char *buf, int size);
char *get_param(const char *msg, char *buf, int size, int paramno);
char *get_tail(const char *msg, char *buf, int size);



/*
 * utils.c functions
 */
void irc_join(int cid, const char *target);			/* join channel(s) */
void irc_part(int cid, const char *msg, const char *target);	/* part from channel(s) */
void irc_msg(int cid, const char *msg, const char *target);		/* send a PRIVMSG */
void irc_action(int cid, const char *msg, const char *target);	/* send an ACTION */
void irc_notice(int cid, const char *msg, const char *target);	/* send a NOTICE */
#endif
