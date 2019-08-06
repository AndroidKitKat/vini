#include <sys/types.h>
#include <irc/irc.h>

struct ircmsg {
	int cid;
	char *raw;
	char *sender;
	char *channel;
	const char *network;
	char *text;
	int spawning_done;
	pthread_mutex_t *holder_mtx;
	struct holder_head *holders;
};

void register_msg_holder(struct ircmsg *msg, pthread_t thread);
void unregister_msg_holder(struct ircmsg *msg, pthread_t thread);
void free_msg_if_not_used(struct ircmsg *msg);
void to_vinny(const struct hook_msg *);
int setup_vinny(void);
int reload_vinny(void);

/* constants */
#define MAX_PATH_SIZE 256

/*
 * Settings related to the bot's runtime
 */
char *vinny_headdir;
char *vinny_bindir;
char *vinny_libdir;
char *vinny_confdir;
char *vinny_netdir;

#ifdef DEBUG
#include <sys/errno.h>
#include <stdio.h>
#include <string.h>
#define VDEBUG(text) fprintf(stderr, "DEBUG: %s\n", text);
#define ERRNOCHECK(failure) {							\
	if (errno) {								\
		fprintf(stderr, "DEBUG: %s\n", failure);			\
		return errno;							\
	}									\
}
#define DBGINIT(f) {								\
	fprintf(stderr, "DEBUG: running %s\n", #f);				\
	ret = f();								\
	if (ret || errno) {							\
		fprintf(stderr, "DEBUG: %s failed: %s\n", #f, strerror(errno));	\
		return ret;							\
	}									\
}
#else
#define VDEBUG(text)
#define DBGINIT(f) f();
#define ERRNOCHECK(failure)
#endif
