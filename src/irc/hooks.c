/* interface to hook C functions to IRC commands */

#include <sys/errno.h>
#include <stdlib.h>	/* malloc */
#include <string.h>
#include <sys/queue.h>	/* linked lists */
#include <search.h>	/* hash table */

#include "irc.h"

/*
 * Table entries.
 * Linked list provided by queue.h,
 * fid will be passed to every hooked function.
 * This gives the ability for a single function to branch out
 * depending on the hook itself.
 */
struct hooks {
	int fid;
	void (*function)(const struct hook_msg *);
	void (*free)(int);
	SLIST_ENTRY(hooks) next;
};

SLIST_HEAD(hooks_head, hooks);

/*
 * Add an entry to the table, initiate the key if it * doesn't exist.
 * Return 0 on success and non-zero on failure.
 */
int
add_irc_hook(const char *key, void (*function)(const struct hook_msg *), void (*free)(int), int fid)
{
	struct hooks *new_entry = malloc(sizeof(struct hooks));
	if (new_entry == NULL)
		return 1;

	struct hooks_head *hhead;
	ENTRY item;
	ENTRY *found;

	item.key = strdup(key);
	new_entry->fid = fid;
	new_entry->function = function;
	new_entry->free = free;

	if ((found = hsearch(item, FIND)) == NULL) {
		errno = 0;
		hhead = malloc(sizeof(struct hooks_head));
		if (hhead == NULL)
			return 1;
		memset(hhead, 0, sizeof(struct hooks_head));
		SLIST_INIT(hhead);

		item.data = hhead;
		hsearch(item, ENTER);
	} else {
		hhead = found->data;
	}

	SLIST_INSERT_HEAD(hhead, new_entry, next);

	return 0;
}

/*
 * Remove a function from the linked list in the table.
 * Follow the linked list until both fid and the function pointer match.
 */
int
rm_irc_hook(const char *key, void (*function)(const struct hook_msg *), int fid)
{
	ENTRY item;
	ENTRY *found;
	item.key = strdup(key);

	if ((found = hsearch(item, FIND)) == 0 ||
	    SLIST_EMPTY((struct hooks_head *)found->data)) {
		free(item.key);
		return 0; /* Job well done! */
	}

	struct hooks_head *hhead = found->data;
	struct hooks *tmphook, *bakhook;
	SLIST_FOREACH_SAFE(tmphook, hhead, next, bakhook) {
		if (tmphook->fid == fid && tmphook->function == function) {
			SLIST_REMOVE(hhead, tmphook, hooks, next);

			if (tmphook->free != NULL)
				tmphook->free(fid);
			free(tmphook);
		}
	}

	free(item.key);
	return 0;
}

int
rm_irc_hooks(const char *key, void (*function)(const struct hook_msg *))
{
	ENTRY item;
	ENTRY *found;
	item.key = strdup(key);

	if ((found = hsearch(item, FIND)) == 0 || SLIST_EMPTY((struct hooks_head *)found->data)) {
		free(item.key);
		return 0;
	}

	struct hooks_head *hhead = found->data;
	struct hooks *h, *th;
	SLIST_FOREACH_SAFE(h, hhead, next, th) {
		if (h->function == function) {
			SLIST_REMOVE(hhead, h, hooks, next);

			if (h->free != NULL)
				h->free(h->fid);
			free(h);
		}
	}

	free(item.key);
	return 0;
}

void
run_hooks(int cid, const char *key, const char *msg, int sn)
{
	ENTRY item;
	ENTRY *found;
	item.key = strdup(key);

	if (msg == NULL || key == NULL) {
		return;
	}

	if ((found = hsearch(item, FIND)) == 0 ||
	    SLIST_EMPTY((struct hooks_head *)found->data)) {
		free(item.key);
		return; /* Job well done! */
	}

	struct hooks_head *hhead = found->data;
	struct hooks *tmphook;

	struct hook_msg hmsg;
	hmsg.raw = msg;
	hmsg.cid = cid;
	hmsg.sn = sn;

	SLIST_FOREACH(tmphook, hhead, next) {
		hmsg.fid = tmphook->fid;
		tmphook->function(&hmsg);
	}

	free(item.key);
	return;
}
