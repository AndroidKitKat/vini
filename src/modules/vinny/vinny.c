#include <sys/queue.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include <irc/irc.h>
#include <config.h>
#include "vinny.h"
#include "mods.h"
#include "ops.h"

static pthread_mutex_t op_mtx = PTHREAD_MUTEX_INITIALIZER;
void destroy_vinny(void);
void free_vinny(int);

/* msg tracking list data structures */
SLIST_HEAD(holder_head, msgholder);
struct msgholder {
	pthread_t thread;
	SLIST_ENTRY(msgholder) next;
};

const struct irc_module vinny = {
	.init = setup_vinny,
	.free = destroy_vinny,
	.reload = reload_vinny,
	.sigchld = cleanup_mod
};

/* Receive private messages from the IRC client and process it */
void
to_vinny(const struct hook_msg *hmsg)
{
	struct ircmsg *msg = malloc(sizeof(struct ircmsg));
	if (msg == NULL)
		return;

	msg->cid = hmsg->cid;
	msg->raw = strdup(hmsg->raw);
	msg->channel = get_chan(msg);
	msg->network = get_network_name(hmsg->cid);
	msg->spawning_done = 0;
	msg->holder_mtx = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(msg->holder_mtx, NULL);
	msg->holders = malloc(sizeof(struct holder_head));
	memset(msg->holders, 0, sizeof(struct holder_head));
	SLIST_INIT(msg->holders);

	if (hmsg->fid != 0) {
		msg->sender = NULL;
		msg->text = NULL;
		spawn_hook(msg, hmsg->fid);
	} else {
		msg->sender = get_nick(msg);
		msg->text = get_text(msg);

#ifdef WITH_PG
		if (hmsg->sn == 0 && pg_connected) {
			pthread_t logthread;
			pthread_create(&logthread, NULL, log_msg, msg);
			register_msg_holder(msg, logthread);
		}
#endif

		spawn_cmd(msg);
		spawn_rgx(msg);	
	}

	msg->spawning_done = 1;
	free_msg_if_not_used(msg);
	return;
}

/* Setup all components, trust the caller to handle failure */
int
setup_vinny()
{
	VDEBUG("initializing vinny");
	int ret = 0;

	vinny_headdir = HEADDIR;
	vinny_bindir = BINDIR;
	vinny_libdir = LIBDIR;
	vinny_confdir = CONFDIR;
	vinny_netdir = NETDIR;

	char libdir[1024];
	snprintf(libdir, 1024, "%s/%s", vinny_headdir, vinny_libdir);
	setenv("PYTHONPATH", libdir, 1);
	setenv("LIBDIR", libdir, 1);

	pthread_mutex_lock(&op_mtx);
	ERRNOCHECK("failed to lock the mutex");

	add_irc_hook("PRIVMSG", to_vinny, free_vinny, 0);
	ERRNOCHECK("failed to add our hook");

	DBGINIT(setup_mods_handler);
	DBGINIT(setup_hooks);

#ifdef WITH_PG
	setup_logs();
#endif

	if (ret == 0)
		DBGINIT(setup_modules);

	pthread_mutex_unlock(&op_mtx);
	return ret;
}

void
free_vinny(int fid)
{
	destroy_vinny();
}

/* Destroy all components */
void
destroy_vinny()
{
	VDEBUG("deinitializing vinny");

	VDEBUG("destroying all hooks...");
	destroy_hooks();
	VDEBUG("destroying all regexes...");
	destroy_rgx();
	VDEBUG("destroying all modules...");
	destroy_cmds();

	rm_irc_hooks("PRIVMSG", to_vinny);
}

/* deinitialize all vinny modules and reinitialize them */
int
reload_vinny()
{
	int ret = 0;

	destroy_vinny();
	ret = setup_vinny();
	return ret;
}

void
register_msg_holder(struct ircmsg *msg, pthread_t thread)
{
	if (msg != NULL && msg->holder_mtx != NULL) {
		pthread_mutex_lock(msg->holder_mtx);

		struct msgholder *holder = malloc(sizeof(struct msgholder));
		holder->thread = thread;
		SLIST_INSERT_HEAD(msg->holders, holder, next);
		pthread_mutex_unlock(msg->holder_mtx);
	}
}

void
unregister_msg_holder(struct ircmsg *msg, pthread_t thread)
{
	if (msg != NULL && msg->holder_mtx != NULL) {
		pthread_mutex_lock(msg->holder_mtx);

		struct msgholder *tmpholder, *bakholder;
		SLIST_FOREACH_SAFE(tmpholder, msg->holders, next, bakholder) {
			if (tmpholder->thread == thread)
				SLIST_REMOVE(msg->holders, tmpholder,
				    msgholder, next);
		}

		pthread_mutex_unlock(msg->holder_mtx);
	}

	free_msg_if_not_used(msg);
	return;
}

void
free_msg_if_not_used(struct ircmsg *msg)
{
	if (msg->spawning_done == 0)
		return;

	pthread_mutex_lock(msg->holder_mtx);

	if (SLIST_EMPTY(msg->holders)) {
		pthread_mutex_unlock(msg->holder_mtx);
		free(msg->raw);
		free(msg->sender);
		free(msg->channel);
		free(msg->text);
		pthread_mutex_destroy(msg->holder_mtx);
		free(msg->holder_mtx);
		free(msg->holders);
		free(msg);
	} else {
		pthread_mutex_unlock(msg->holder_mtx);
	}

	return;
}
