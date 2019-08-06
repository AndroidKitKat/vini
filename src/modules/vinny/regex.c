#include <sys/queue.h>

#include <pcre.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "vinny.h"
#include "mods.h"
#include "ops.h"

SLIST_HEAD(rx_head, rx_entry);
static struct rx_head rxhead;

struct rx_entry {
	pcre *rx;
	pcre_extra *rxx;
	struct mod_op *mop;
	SLIST_ENTRY(rx_entry) next;
};

void
spawn_rgx(struct ircmsg *msg)
{
	struct mod_op *mop;
	char *name;
	char *path;
	char *chan = msg->channel;
	char *text = msg->text;

	char **argv;

	struct rx_entry *re;
	int ovec[30];

	SLIST_FOREACH(re, &rxhead, next) {
		if (pcre_exec(re->rx, re->rxx, text, strlen(text), 0, 0, ovec, 30) == 1) {
			mop = re->mop;
			name = mop->name;
			path = mop->path;
			if ((mop->cid != -1 && mop->cid != msg->cid) ||
			    !chan_allowed(mop, chan))
				continue;

			argv = mkargv(mop, msg);

			spawn_mod(name, path, msg, argv);

			freeargv(mop, argv);
		}
	}

	return;
}

void
destroy_rgx(void)
{
	struct rx_entry *re, *tre;
	SLIST_FOREACH_SAFE(re, &rxhead, next, tre) {
		SLIST_REMOVE(&rxhead, re, rx_entry, next);
		free(re->rx);
		pcre_free_study(re->rxx);
		free_mop(re->mop);
		free(re);
	}

	return;
}

int
mk_rgx(char *rgx, struct mod_op *mop)
{
	struct rx_entry *re;
	pcre *rx;
	pcre_extra *rxx;

	const char *errstr = NULL;
	int erroffset = 0;

	rx = pcre_compile(rgx, 0, &errstr, &erroffset, NULL);
	if (rx == NULL) {
		printf("Failed to compile regex: %s (%s)\n", rgx, errstr);
		return 1;
	}

	rxx = pcre_study(rx, 0, &errstr);
	re = malloc(sizeof(struct rx_entry));

	if (re == NULL) {
		free(rx);
		if (rxx != NULL)
			free(rxx);

		return 1;
	}

	re->rx = rx;
	re->rxx = rxx;
	re->mop = mop;
	SLIST_INSERT_HEAD(&rxhead, re, next);
	return 0;
}
