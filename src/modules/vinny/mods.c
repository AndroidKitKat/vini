#include <sys/queue.h>
#include <sys/socket.h>

#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <err.h>

#include "vinny.h"
#include "mods.h"

struct mod_entry {
	struct vinny_module *vmod;
	pthread_t thread;
	SLIST_ENTRY(mod_entry) next;
};

static pthread_mutex_t mod_mtx = PTHREAD_MUTEX_INITIALIZER;

int setup_mods_handler(void);
void insert_mod(struct mod_entry *mod);
void remove_mod(struct mod_entry *mod);

SLIST_HEAD(mod_head, mod_entry);

static struct mod_head mhead;
extern char **environ;

int
setup_mods_handler()
{
	int ret = 0;
	ret = pthread_mutex_init(&mod_mtx, NULL);
	return ret;
}

int
spawn_mod(const char *name, const char *path, struct ircmsg *msg, char **argv)
{
	pthread_mutex_lock(&mod_mtx);

	int ret = 0;

	pthread_t thread;
	pid_t pid;

	/* Hold the new process's file descriptors */
	int sp[2];
	int cso, pso;

	struct mod_entry *mod = malloc(sizeof(struct mod_entry));
	struct vinny_module *vmod = malloc(sizeof(struct vinny_module));

	ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sp);
	cso = sp[0];	/* child socket */
	pso = sp[1];	/* parent socket */

	ret = access(path, X_OK);
	if (ret) {
		VDEBUG("spawn_mod: bad process");
		goto spawn_mod_fail;
	}

	if (ret) {
		VDEBUG("spawn_mod: failed to open the log file");
		goto spawn_mod_fail;
	}

	pid = fork();
	if (pid == 0) { /* we're in the new process: setup and exec */
		close(pso);

		VDEBUG("spawn_mod: forked, about to exec.");
		ret = dup2(cso, 0);
		if (ret == -1)
			err(1, "Failed to setup stdin in child process");
		ret = dup2(cso, 1);
		if (ret == -1)
			err(1, "Failed to setup stdout in child process");

		/* Bring this child process on a ride */
		execve(path, argv, environ);

		/* Exit with an error if we're still in this address space */
		err(1, "Failed to exec into new process");
	} /* Not reached by child */

	close(cso);

	/* Setup the struct vinny_module to send to the watcher thread */
	vmod->name = name;
	vmod->msg = msg;
	vmod->child_pid = pid;
	vmod->socket = pso;

	ret = pthread_create(&thread, NULL, mod_watcher, vmod);
	if (ret != 0) {
		VDEBUG("spawn_mod: failed to create watcher thread");
		goto spawn_mod_fail;
	}

	/* Setup the struct mod_entry and place in the list */
	mod->vmod = vmod;
	mod->thread = thread;
	register_msg_holder(msg, thread);
	insert_mod(mod);
	goto spawn_mod_success;

spawn_mod_fail:
	close(cso);
	close(pso);
	free(mod);
	free(vmod);
spawn_mod_success:

	pthread_mutex_unlock(&mod_mtx);
	return ret;
}

struct vinny_module *
get_matching_module(char *name, struct ircmsg *msg)
{
	struct mod_entry *tmpmod;
	SLIST_FOREACH(tmpmod, &mhead, next) {
		if (strcmp(name, tmpmod->vmod->name) == 0 &&
		    ((msg == NULL && tmpmod->vmod->msg == NULL) ||
		    msg->cid == tmpmod->vmod->msg->cid ||
		    (msg->channel != NULL &&
		    tmpmod->vmod->msg->channel != NULL &&
		    strcmp(msg->channel, tmpmod->vmod->msg->channel) == 0))) {
			return tmpmod->vmod;
		}
	}

	return NULL;
}

void
insert_mod(struct mod_entry *mod)
{
	SLIST_INSERT_HEAD(&mhead, mod, next);
}

void
remove_mod(struct mod_entry *mod)
{
	struct vinny_module *vmod = mod->vmod;

	/*
	 * Don't check for error: we don't care wether or not
	 * it was still alive.
	 */
	(void) kill(vmod->child_pid, 9);
	SLIST_REMOVE(&mhead, mod, mod_entry, next);

	free(vmod);
	free(mod);
	return;
}

/*
 * If there's a process without a watching thread,
 * or a thread without a running child process,
 * something is wrong, kill them.
 */
void
cleanup_mods()
{
	pthread_mutex_lock(&mod_mtx);

	struct mod_entry *tmpmod, *bakmod;
	SLIST_FOREACH_SAFE(tmpmod, &mhead, next, bakmod) {
		if (kill(tmpmod->vmod->child_pid, 0) != 0)
			remove_mod(tmpmod);
	}

	pthread_mutex_unlock(&mod_mtx);
	return;
}

/*
 * Cleanup mod watching pid
 * Will be called on sigchld
 */
void
cleanup_mod(pid_t pid)
{
	pthread_mutex_lock(&mod_mtx);

	struct mod_entry *tmpmod, *bakmod;
	SLIST_FOREACH_SAFE(tmpmod, &mhead, next, bakmod) {
		if (tmpmod->vmod->child_pid == pid) {
			remove_mod(tmpmod);
			break;
		}
	}

	pthread_mutex_unlock(&mod_mtx);
	return;
}

void
destroy_mods()
{
	pthread_mutex_lock(&mod_mtx);

	struct mod_entry *tmpmod, *bakmod;
	SLIST_FOREACH_SAFE(tmpmod, &mhead, next, bakmod) {
		remove_mod(tmpmod);
	}

	pthread_mutex_unlock(&mod_mtx);
}
