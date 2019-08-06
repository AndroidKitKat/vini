#include <unistd.h>

/*
 * Available information about a running module.
 * stdout should only be read by the watching thread.
 * This struct is sent to the watcher thread, so it's
 * always read-only.
 */
struct vinny_module {
	const char *name;	/* Name of the module spawned for */
	struct ircmsg *msg;	/* message it was requested from */
	pid_t child_pid;	/* PID of the module's process */
	int socket;		/* stdin/out of the process */
};

int spawn_mod(const char *name, const char *path, struct ircmsg *msg, char **argv);
void *mod_watcher(void *);
int setup_modules(void);
int setup_mods_handler(void);
struct vinny_module *get_matching_module(char *name, struct ircmsg *msg);
void cleanup_mods(void);
void cleanup_mod(pid_t pid);
void destroy_mods(void);
