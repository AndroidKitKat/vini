#include <sys/queue.h>

#include <stdlib.h>
#include <string.h>

#include <irc/irc.h>
#include <info/info.h>
#include "vinny.h"
#include "ops.h"
#include "parser.h"

struct dynargs {
	char *arg;
	char *(*func)(struct ircmsg *);
};

static struct dynargs args[] = {
	{ .arg = "$raw$", .func = get_raw },
	{ .arg = "$nick$", .func = get_nick },
	{ .arg = "$host$", .func = get_host },
	{ .arg = "$chan$", .func = get_chan },
	{ .arg = "$text$", .func = get_text },
	{ .arg = "$args$", .func = get_args },
	{ .arg = "$nuh$", .func = get_nuh },
	{ .arg = "$mode$", .func = get_mode },
	{ .arg = "$network$", .func = get_network },
	{ NULL, NULL }
};

void
free_mop(struct mod_op *mop)
{
	int i;

	free(mop->name);
	free(mop->path);

	if (mop->chans != NULL) {
		for (i = 0; mop->chans[i] != NULL; i++)
			free(mop->chans[i]);

		free(mop->chans);
	}

	if (mop->argv != NULL) {
		for (i = 0; mop->argv[i] != NULL; i++)
			free(mop->argv[i]);

		free(mop->argv);
	}
}

/* Replace argv flags for data from the mesage */
char **
mkargv(struct mod_op *mop, struct ircmsg *msg)
{
	int sz, i, j;
	for (sz = 0; mop->argv[sz] != NULL; sz++)
		;

	char **argv = malloc((sz + 2) * sizeof(char *));
	for (i = 0; i <= sz; i++)
		argv[i] = mop->argv[i];

	for (i = 0; args[i].arg != NULL; i++) {
		for (j = 0; argv[j] != NULL; j++) {
			if (strcmp(argv[j], args[i].arg) == 0)
				argv[j] = args[i].func(msg);
		}
	}

	return argv;
}

/* Free changed strings from a mod's argv */
void
freeargv(struct mod_op *mop, char **argv)
{
	int i;
	for (i = 0; argv[i] != NULL; i++) {
		if (argv[i] != mop->argv[i])
			free(argv[i]);
	}

	free(argv);
	return;
}

int
chan_allowed(struct mod_op *mop, char *chan)
{
	int i;
	char **allowed_chans = mop->chans;
	if (allowed_chans == NULL || allowed_chans[0] == NULL)
		return 1;

	for (i = 0; allowed_chans[i] != NULL; i++)
		if (strcmp(allowed_chans[i], chan) == 0)
			return 1;

	return 0;
}

char *
get_raw(struct ircmsg *msg)
{
	return strdup(msg->raw);
}

char *
get_text(struct ircmsg *msg)
{
	char *tail = malloc(512);
	return get_tail(msg->raw, tail, 512);
}

char *
get_args(struct ircmsg *msg)
{
	char text[512];
	get_tail(msg->raw, text, 512);
	char *args;
	int index = index_to_arg(text, 1, NULL);
	if (index == -1) {
		args = malloc(1);
		*args = '\0';
	} else {
		 args = strdup(text + index);
	}

	return args;
}

char *
get_chan(struct ircmsg *msg)
{
	char p[256];
	get_param(msg->raw, p, 256, 1);
	if (p[0] != '#')
		return get_nick(msg);

	return strdup(p);
}

char *
get_host(struct ircmsg *msg)
{
	char *sender = malloc(256);
	return get_sender(msg->raw, sender, 256);
}

char *
get_nick(struct ircmsg *msg)
{
	char sender[256];
	get_sender(msg->raw, sender, 256);
	char *nick = malloc(strlen(sender) + 2);
	int i;

	for (i = 0; sender[i] != '!'; i++)
		nick[i] = sender[i];
	nick[i] = '\0';
	return nick;
}

char *
get_nuh(struct ircmsg *msg)
{
	char sender[256];
	get_sender(msg->raw, sender, 256);
	char *host = malloc(strlen(sender) + 2);
	int i;

	for (i = 0; sender[i] != '@'; i++)
		;

	for (; sender[i] != '\0'; i++) {
		host[i] = sender[i];
	}

	host[i] = '\0';
	return host;
}

char *
get_network(struct ircmsg *msg)
{
	char *network = strdup(get_network_name(msg->cid));
	return network;
}

char *
get_mode(struct ircmsg *msg)
{
	struct user_info *uinfo = get_user_info(msg->network, msg->channel, msg->sender);
	char *modes;
	char m[2];

	if (uinfo == NULL)
		m[0] = 'u';
	else if (uinfo->modes & IRC_OP)
		m[0] = 'n';
	else if (uinfo->modes & CHAN_OP)
		m[0] = 'o';
	else if (uinfo->modes & CHAN_HOP)
		m[0] = 'h';
	else if (uinfo->modes & CHAN_VOP)
		m[0] = 'v';
	else
		m[0] = 'u';

	m[1] = '\0';
	modes = strdup(m);
	return modes;
}
