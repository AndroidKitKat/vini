#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>

#include "info.h"

static void free_user_entry(struct user_list *, struct user_entry *);
static void free_user_list(struct user_list *);
static void free_channel_entry(struct channel_list *, struct channel_entry *);

static struct channel_list clist[32];

struct channel_list *
get_channel_list(int cid)
{
	return clist + cid;
}

struct user_list *
get_user_list(const char *channel, int cid)
{
	struct channel_list *cclist = get_channel_list(cid);
	struct channel_entry *centry;
	struct user_list *ulist = NULL;
	SLIST_FOREACH(centry, cclist, next) {
		if(centry->cinfo == NULL || centry->cinfo->channel == NULL) {
			/* it's broken */
			free_channel_entry(cclist, centry);
			continue;
		}

		if (strcmp(channel, centry->cinfo->channel) == 0) {
			ulist = centry->users;
			break;
		}
	}

	return ulist;
}

struct user_info *
make_user_info(const char *network, const char *channel, const char *nick)
{
	struct user_info *uinfo = malloc(sizeof(struct user_info));
	if (uinfo == NULL)
		return NULL;

	memset(uinfo, 0, sizeof(struct user_info));
	uinfo->network = strdup(network);
	uinfo->channel = strdup(channel);
	uinfo->nick = strdup(nick);
	return uinfo;
}

struct channel_info *
make_channel_info(const char *network, const char *channel)
{
	struct channel_info *cinfo = malloc(sizeof(struct channel_info));
	if (cinfo == NULL)
		return NULL;

	memset(cinfo, 0, sizeof(struct channel_info));
	cinfo->network = strdup(network);
	cinfo->channel = strdup(channel);
	return cinfo;
}

int
add_user_info(struct user_info *uinfo, int cid, int append)
{
	ENTRY item;
	ENTRY *found;

	char buf[512];
	memset(buf, 0, 512);
	snprintf(buf, 512, "%s.%s.%s", uinfo->network, uinfo->channel, uinfo->nick);
	item.key = buf;
	item.data = uinfo;

	struct user_list *ulist = get_user_list(uinfo->channel, cid);
	struct user_entry *uentry;

	if ((found = hsearch(item, FIND)) != NULL && found->data != NULL) {
		struct user_info *found_uinfo = found->data;
		if (uinfo != found_uinfo) {
			found->data = uinfo;

			if (ulist != NULL) {
				SLIST_FOREACH(uentry, ulist, next) {
					if (uentry->uinfo == found_uinfo) {
						uentry->uinfo = uinfo;
						break;
					}
				}
			}

			free_user_info(found_uinfo);
		}

		return 0;
	}

	if (ulist != NULL && append) {
		struct user_entry *uentry = malloc(sizeof(struct user_entry));
		uentry->uinfo = uinfo;
		SLIST_INSERT_HEAD(ulist, uentry, next);
	}

	item.key = strdup(buf);
	found = hsearch(item, ENTER);
	if (found == NULL) {
		free(item.key);
		return 1;
	}

	errno = 0;
	return 0;
}

void
rm_user_info(const char *network, const char *channel, const char *nick, int cid, int free)
{
	ENTRY item;
	ENTRY *found;

	char buf[512];
	snprintf(buf, 512, "%s.%s.%s", network, channel, nick);
	item.key = buf;
	item.data = NULL;

	if ((found = hsearch(item, FIND)) == NULL) {
		errno = 0;
		return;
	} else {
		found->data = NULL;
	}

	if (free == 0)
		return;

	struct user_list *ulist = get_user_list(channel, cid);
	struct user_entry *uentry, *uentry_bak;
	SLIST_FOREACH_SAFE(uentry, ulist, next, uentry_bak) {
		if (uentry->uinfo == NULL || uentry->uinfo->nick == NULL) {
			/* it's broken */
			free_user_entry(ulist, uentry);
			continue;
		}

		if (strcmp(nick, uentry->uinfo->nick) == 0) {
			free_user_entry(ulist, uentry);
			break;
		}
	}

	return;
}

struct user_info *
get_user_info(const char *network, const char *channel, const char *nick)
{
	ENTRY item;
	ENTRY *found;

	char buf[512];
	snprintf(buf, 512, "%s.%s.%s", network, channel, nick);
	item.key = buf;

	found = hsearch(item, FIND);

	struct user_info *uinfo = NULL;
	if (found != NULL)
		uinfo = (struct user_info *)found->data;

	if (errno || found == NULL || uinfo == NULL || uinfo->nick == NULL ||
	    strcmp(uinfo->nick, nick) != 0) {
		return NULL;
	} else {
		return uinfo;
	}
}

int
add_channel_info(struct channel_info *cinfo, int cid)
{
	struct channel_entry *centry = malloc(sizeof(struct channel_entry));
	struct user_list *ulist = malloc(sizeof(struct user_list));
	if (centry == NULL || ulist == NULL)
		return 1;

	SLIST_INIT(ulist);

	centry->cinfo = cinfo;
	centry->users = ulist;
	SLIST_INSERT_HEAD(clist + cid, centry, next);
	return 0;
}

void
rm_channel_info(const char *network, const char *channel, int cid)
{
	struct channel_list *cclist = clist + cid;
	struct channel_entry *centry, *centry_bak;

	SLIST_FOREACH_SAFE(centry, cclist, next, centry_bak) {
		if (strcmp(centry->cinfo->channel, channel) == 0) {
			free_channel_entry(cclist, centry);
			break;
		}
	}

	return;
}

struct channel_info *
get_channel_info(const char *channel, int cid)
{
	struct channel_list *cclist = clist + cid;
	struct channel_entry *centry;

	SLIST_FOREACH(centry, cclist, next) {
		if (strcmp(centry->cinfo->channel, channel) == 0)
			return centry->cinfo;
	}

	return NULL;
}

void
free_user_info(struct user_info *uinfo)
{
	free(uinfo->channel);
	free(uinfo->nick);
	free(uinfo->user);
	free(uinfo->host);
	free(uinfo->realname);
	free(uinfo);
	return;
}

static void
free_user_entry(struct user_list *ulist, struct user_entry *uentry)
{
	SLIST_REMOVE(ulist, uentry, user_entry, next);
	free_user_info(uentry->uinfo);
	free(uentry);
	return;
}

static void
free_user_list(struct user_list *ulist)
{
	struct user_entry *uentry;
	while (!SLIST_EMPTY(ulist)) {
		uentry = SLIST_FIRST(ulist);
		SLIST_REMOVE_HEAD(ulist, next);
		free_user_entry(ulist, uentry);
	}

	free(ulist);
	return;
}

void
free_channel_info(struct channel_info *cinfo)
{
	free(cinfo->channel);
	free(cinfo->topic);
	free(cinfo->modes);
	free(cinfo);
	return;
}

static void
free_channel_entry(struct channel_list *cclist, struct channel_entry *centry)
{
	SLIST_REMOVE(cclist, centry, channel_entry, next);
	free_channel_info(centry->cinfo);
	free_user_list(centry->users);
	free(centry);
	return;
}
