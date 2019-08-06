#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include <irc/irc.h>
#include "vinny.h"
#include "mods.h"
#include "ops.h"

struct irc_hook {
	char *cmd;
	int fid;
	struct mod_op *mop;
};

static int ccount = 1;
static struct irc_hook hooks[8192];

void free_vinny_hook(int);

void
spawn_hook(struct ircmsg *msg, int fid)
{
	struct mod_op *mop = hooks[fid].mop;
	char *name = mop->name;
	char *path = mop->path;
	char **argv = mkargv(mop, msg);

	if (mop->cid != -1 && msg != NULL && mop->cid != msg->cid)
		return;

	char channel[256];
	if (mop->chans != NULL && get_param(msg->raw, channel, 256, 1) != NULL && channel[0] == '#') {
		int i;
		for (i = 0; mop->chans[i] != NULL; i++)
		if (strcmp(channel, mop->chans[i]) == 0)
			break;

		if (mop->chans[i] == NULL)
			return;
	}

	spawn_mod(name, path, msg, argv);

	freeargv(mop, argv);
	return;
}

int
setup_hooks()
{
	/* Setup a NULL entry for later use */
	hooks[0].cmd = NULL;
	hooks[0].fid = 0;
	hooks[0].mop = NULL;

	return 0;
}

void
destroy_hooks()
{
	int i;
	struct irc_hook hook;
	for (i = 1; i < ccount; i++) {
		hook = hooks[i];
		rm_irc_hook(hook.cmd, to_vinny, hook.fid);
	}

	ccount = 1;
	for (i = 1; hooks[i].mop != NULL; i++) {
		free(hook.cmd);
		free_mop(hook.mop);
		hooks[i] = hooks[0];
	}

	return;
}

void
free_vinny_hook(int fid)
{
	free(hooks[fid].cmd);
	free_mop(hooks[fid].mop);
	hooks[fid] = hooks[0];
}

int
mk_hook(char *cmd, struct mod_op *mop)
{
	if (cmd == NULL || mop == NULL || ccount >= 8192)
		return 1;

	struct irc_hook hook;

	hook.cmd = cmd;
	hook.mop = mop;
	hook.fid = ccount++;
	hooks[hook.fid] = hook;

	add_irc_hook(hook.cmd, to_vinny, free_vinny_hook, hook.fid);
	return 0;
}
