#include <pthread.h>

#define ERRSTR_MAX 256
#define RSPLEN_MAX 1024

/* Data structure for a module operation */
struct mod_op {
	/* Name of the process to execute */
	char *name;

	/* Path of the process to execute */
	char *path;

	/* cid it's restricted to, or -1 if it's not restricted */
	int cid;

	/* Channels it's restricted to, or NULL if there's none */
	char **chans;

	/* argv to send to the process */
	char **argv;
};

struct parser_info {
	char *op;
	const char *name;
	char *path;
	char *channel;
	int cid;

	struct ircmsg *msg;

	char *rsp;
	char *errstr;
};

/* Send an operation to the parser */
int process_op(struct parser_info *);
void free_mop(struct mod_op *);

/* ops */
char **mkargv(struct mod_op *mop, struct ircmsg *msg);
void freeargv(struct mod_op *mop, char **argv);
int chan_allowed(struct mod_op *mop, char *chan);
char *get_chan(struct ircmsg *msg);
char *get_nick(struct ircmsg *msg);
char *get_host(struct ircmsg *msg);
char *get_text(struct ircmsg *msg);
char *get_args(struct ircmsg *msg);
char *get_nuh(struct ircmsg *msg);
char *get_raw(struct ircmsg *msg);
char *get_mode(struct ircmsg *msg);
char *get_network(struct ircmsg *msg);

/* logs */
int pg_connected;
void *log_msg(void *msg);
int setup_logs(void);
char *access_logs(char **regexes, struct ircmsg *msg);

/* cmds */
void spawn_cmd(struct ircmsg *msg);
int mk_cmd(char *cmd, struct mod_op *mop);
void destroy_cmds(void);

/* regex */
void spawn_rgx(struct ircmsg *msg);
void destroy_rgx(void);
int mk_rgx(char *rgx, struct mod_op *mop);

/* hooks */
void spawn_hook(struct ircmsg *msg, int fid);
int setup_hooks(void);
void destroy_hooks(void);
int mk_hook(char *hook, struct mod_op *mop);
