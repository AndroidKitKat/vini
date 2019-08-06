#include <sys/queue.h>

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <search.h>
#include <string.h>

#include <config.h>
#include "vinny.h"
#include "mods.h"
#include "ops.h"

struct cmdentry {
	char *cmd;
	struct mod_op *mop;
	SLIST_ENTRY(cmdentry) next;
};

void free_cmd(struct cmdentry *);
char *get_cmdname(struct ircmsg *);
struct mod_op *get_mop(char *);

char *cmd_prefix[] = CMD_PREFIX;

static SLIST_HEAD(cmdlist, cmdentry) cmdl = SLIST_HEAD_INITIALIZER(cmdl);

void
spawn_cmd(struct ircmsg *msg)
{
	char *cmd = get_cmdname(msg);
	struct mod_op *mop = get_mop(cmd);
	if (mop == NULL)
		return;

	 if ((mop->cid != -1 && mop->cid != msg->cid) ||
	    !chan_allowed(mop, msg->channel))
		return;

	char **argv = mkargv(mop, msg);
	spawn_mod(mop->name, mop->path, msg, argv);

	freeargv(mop, argv);
	return;
}

int
mk_cmd(char *cmd, struct mod_op *mop)
{
	if (mop == NULL)
		return 1;

	struct cmdentry *cmde = malloc(sizeof(struct cmdentry));
	cmde->cmd = cmd;
	cmde->mop = mop;
	SLIST_INSERT_HEAD(&cmdl, cmde, next);

	int i;
	char *prfcmd;
	ENTRY item;
	ENTRY *found;
	for (i = 0; cmd_prefix[i] != NULL; i++) {
		prfcmd = malloc(strlen(cmd) + strlen(cmd_prefix[i]) + 1);
		strcpy(prfcmd, cmd_prefix[i]);
		strcat(prfcmd, cmd);

		item.key = prfcmd;
		item.data = mop;

		found = hsearch(item, FIND);
		if (found != NULL) {
			free(prfcmd);
			return 1;
		}

		/* This set errno */
		errno = 0;

		item.key = prfcmd;
		item.data = mop;
		found = hsearch(item, ENTER);
		if (found == NULL) {
			free(prfcmd);
			return 1;
		}
	}

	return 0;
}

void
rm_cmd(struct mod_op *mop)
{
	struct cmdentry *cmde, *tcmde;
	SLIST_FOREACH_SAFE(cmde, &cmdl, next, tcmde) {
		if (mop == cmde->mop) {
			free_cmd(cmde);
		}
	}
}

void
free_cmd(struct cmdentry *cmde)
{
	ENTRY item;
	ENTRY *found;

	item.key = cmde->cmd;
	if ((found = hsearch(item, FIND)) == (void *)cmde->mop) {
		SLIST_REMOVE(&cmdl, cmde, cmdentry, next);
		found->data = NULL;
		free_mop(cmde->mop);
		free(cmde->cmd);
		free(cmde);
	}
}

void
destroy_cmds()
{
	struct cmdentry *cmde, *tcmde;
	SLIST_FOREACH_SAFE(cmde, &cmdl, next, tcmde) {
		free_cmd(cmde);
	}
}

struct mod_op *
get_mop(char *cmd)
{
	ENTRY item;
	ENTRY *found;

	if (cmd == NULL)
		return NULL;

	item.key = cmd;
	found = hsearch(item, FIND);
	if (found == NULL || found->data == NULL) {
		return NULL;
	}

	struct mod_op *mop = (struct mod_op *)found->data;

	free(cmd);
	return mop;
}

char *
get_cmdname(struct ircmsg *msg)
{
	char *cmd, *text = msg->text;
	if (text == NULL)
		return NULL;

	int i, cmdlen, prefixlen, success = 0;
	for (i = 0; cmd_prefix[i] != NULL; i++) {
		prefixlen = strlen(cmd_prefix[i]);
		if (strncmp(text, cmd_prefix[i], prefixlen) == 0) {
			success = 1;
			break;
		}
	}

	if (!success)
		return NULL;

	cmdlen = strlen(text) + strlen(cmd_prefix[i]) + 1;
	cmd = malloc(cmdlen);
	if (cmd == NULL)
		return NULL;

	for (i = 0; text[i] != ' ' && text[i] != '\0'; i++)
		cmd[i] = text[i];
	cmd[i] = '\0';

	return cmd;
}
