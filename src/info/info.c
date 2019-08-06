#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <irc/irc.h>
#include "info.h"

static void parse_mode_prefix(struct user_info *, const char *);
static void get_nick_from_head(char *, char *);

/* IRC hooks */
void on_join(const struct hook_msg *);
void on_part(const struct hook_msg *);
void on_quit(const struct hook_msg *);
void on_nick_change(const struct hook_msg *);
void on_mode(const struct hook_msg *);
void on_who(const struct hook_msg *);
void on_names(const struct hook_msg *);
void on_topic(const struct hook_msg *);

static int setup;

int
setup_info(void)
{
	if (setup)
		return 0;

	add_irc_hook("JOIN", on_join, NULL, 0);
	add_irc_hook("PART", on_part, NULL, 0);
	add_irc_hook("QUIT", on_quit, NULL, 0);
	add_irc_hook("NICK", on_nick_change, NULL, 0);
	add_irc_hook("MODE", on_mode, NULL, 0);

	add_irc_hook("352", on_who, NULL, 0);
	add_irc_hook("353", on_names, NULL, 0);

	add_irc_hook("TOPIC", on_topic, NULL, 0);
	/* TOPIC sent on join */
	add_irc_hook("332", on_topic, NULL, 1);

	setup = 1;
	return 0;
}

/* :head JOIN :channel */
void
on_join(const struct hook_msg *hmsg)
{
	if (hmsg->raw == NULL)
		return;

	char head[256];
	get_sender(hmsg->raw, head, 256);
	char channel[256];
	get_tail(hmsg->raw, channel, 256);
	const char *network = get_network_name(hmsg->cid);
	if (network == NULL)
		return;

	char nick[256];
	memset(nick, 0, 256);
	get_nick_from_head(head, nick);
	char *botnick = get_bot_name(hmsg->cid, hmsg->sn);

	if (botnick != NULL && strcmp(nick, botnick) == 0) {
		struct channel_info *cinfo = make_channel_info(network, channel);
		if (cinfo == NULL)
			return;


		add_channel_info(cinfo, hmsg->cid);

		/* call WHO on the channel while we're here */
		char cmd[512];
		snprintf(cmd, 512, "WHO %s\r\n", channel);
		send_irc_message(hmsg->cid, cmd, 1, -1);
	} else {
		struct user_info *uinfo = make_user_info(network, channel, nick);
		if (uinfo == NULL)
			return;

		add_user_info(uinfo, hmsg->cid, 1);
	}

	return;
}

/* :head PART channel :reason */
void
on_part(const struct hook_msg *hmsg)
{
	if (hmsg->raw == NULL)
		return;

	char head[256];
	get_sender(hmsg->raw, head, 256);
	char channel[256];
	get_param(hmsg->raw, channel, 256, 1);
	const char *network = get_network_name(hmsg->cid);
	if (network == NULL)
		return;

	char nick[256];
	memset(nick, 0, 256);
	get_nick_from_head(head, nick);
	char *botnick = get_bot_name(hmsg->cid, hmsg->sn);

	if (botnick != NULL && strcmp(nick, botnick) == 0) {
		rm_channel_info(network, channel, hmsg->cid);
	} else {
		rm_user_info(network, channel, nick, hmsg->cid, 1);
	}

	return;
}

/* :head QUIT :reason */
void
on_quit(const struct hook_msg *hmsg)
{
	if (hmsg->raw == NULL)
		return;

	char head[256];
	get_sender(hmsg->raw, head, 256);

	const char *network = get_network_name(hmsg->cid);
	char nick[256];
	memset(nick, 0, 256);
	get_nick_from_head(head, nick);

	struct channel_list *cclist = get_channel_list(hmsg->cid);
	struct channel_entry *centry, *centry_bak;
	SLIST_FOREACH_SAFE(centry, cclist, next, centry_bak) {
		rm_user_info(network, centry->cinfo->channel, nick, hmsg->cid, 1);
	}

	return;
}

/* :head NICK newnick */
void
on_nick_change(const struct hook_msg *hmsg)
{
	if (hmsg->raw == NULL)
		return;

	char head[256];
	get_sender(hmsg->raw, head, 256);
	char newnick[64];
	get_param(hmsg->raw, newnick, 64, 1);
	const char *network = get_network_name(hmsg->cid);
	if (network == NULL)
		return;

	char oldnick[256];
	memset(oldnick, 0, 256);
	get_nick_from_head(head, oldnick);

	struct channel_list *cclist = get_channel_list(hmsg->cid);
	struct channel_entry *centry;
	SLIST_FOREACH(centry, cclist, next) {
		struct user_info *uinfo = get_user_info(network, centry->cinfo->channel, oldnick);
		if (uinfo == NULL) {
			continue;
		} else {
			rm_user_info(network, centry->cinfo->channel, oldnick, hmsg->cid, 0);
			memset(uinfo->nick, 0, 256);
			strncpy(uinfo->nick, newnick, 255);
			add_user_info(uinfo, hmsg->cid, 0);
		}
	}

	return;
}

/* :head MODE channel modes nick */
void
on_mode(const struct hook_msg *hmsg)
{
	if (hmsg->raw == NULL)
		return;

	char channel[256];
	get_param(hmsg->raw, channel, 256, 1);	
	char modestring[32];
	get_param(hmsg->raw, modestring, 32, 2);
	char nick[64];
	get_param(hmsg->raw, nick, 64, 3);
	const char *network = get_network_name(hmsg->cid);
	if (network == NULL)
		return;

	struct user_info *uinfo = get_user_info(network, channel, nick);
	if (uinfo == NULL) {
		uinfo = make_user_info(network, channel, nick);
		add_user_info(uinfo, hmsg->cid, 1);
	}

	int modes = uinfo->modes;

	int i, append = 1;
	char c;
	for (i = 0; modestring[i] != '\0'; i++) {
		c = modestring[i];
		switch (c) {
		case '+':
			append = 1;
			continue;
		case '-':
			append = 0;
			continue;
		case 'Y':
			if (append)
				modes |= IRC_OP;
			else
				modes &= (modes ^ IRC_OP);
			continue;
		case 'o':
			if (append)
				modes |= CHAN_OP;
			else
				modes &= (modes ^ CHAN_OP);
			continue;
		case 'h':
			if (append)
				modes |= CHAN_HOP;
			else
				modes &= (modes ^ CHAN_HOP);
			continue;
		case 'v':
			if (append)
				modes |= CHAN_VOP;
			else
				modes &= (modes ^ CHAN_VOP);
			continue;
		default:
			continue;
		}
	}

	uinfo->modes = modes;
	return;
}

/* :head 352 botnick channel user host server nick status :hops realname */
void
on_who(const struct hook_msg *hmsg)
{
	if (hmsg->raw == NULL)
		return;

	char channel[256];
	get_param(hmsg->raw, channel, 256, 2);
	char user[64];
	get_param(hmsg->raw, user, 64, 3);
	char host[128];
	get_param(hmsg->raw, host, 128, 4);
	char nick[64];
	get_param(hmsg->raw, nick, 64, 6);
	char status[64];
	get_param(hmsg->raw, status, 64, 7);
	char tail[128];
	get_tail(hmsg->raw, tail, 128);
	const char *network = get_network_name(hmsg->cid);
	char *realname;

	int i;
	for (i = 0; tail[i] != ' ' && tail[i] != '\0'; i++)
		;
	i--;

	if (tail[i] != '\0')
		realname = strdup(tail + i);
	else
		realname = NULL;

	struct user_info *uinfo = get_user_info(network, channel, nick);
	if (uinfo == NULL) {
		uinfo = make_user_info(network, channel, nick);
		add_user_info(uinfo, hmsg->cid, 1);
	}

	free(uinfo->user);
	uinfo->user = strdup(user);

	free(uinfo->host);
	uinfo->host = strdup(host);

	free(uinfo->realname);
	uinfo->realname = realname;

	parse_mode_prefix(uinfo, status);
	return;
}

/* :head 353 channel botnick ? channel :<prefix><name> ... */
void
on_names(const struct hook_msg *hmsg)
{
	if (hmsg->raw == NULL)
		return;

	char channel[256];
	get_param(hmsg->raw, channel, 256, 3);
	char names[512];
	get_tail(hmsg->raw, names, 512);
	const char *network = get_network_name(hmsg->cid);

	struct user_info *uinfo;

	int i, j;
	char modestr[64], nick[256];
	for (i = 0; names[i] != '\0'; i++) {
		while (names[i] == ' ')
			i++;

		j = 0;
		while (names[i] == '+' || names[i] == '@' ||
		    names[i] == '%' || names[i] == '~' ||
		    names[i] == '&' || names[i] == '!')
			modestr[j++] = names[i++];
		modestr[j] = '\0';

		j = 0;
		while (names[i] != ' ' && names[i] != '\0')
			nick[j++] = names[i++];
		nick[j] = '\0';

		uinfo = get_user_info(network, channel, nick);
		if (uinfo == NULL) {
			uinfo = make_user_info(network, channel, nick);
			add_user_info(uinfo, hmsg->cid, 1);
		}

		parse_mode_prefix(uinfo, modestr);

		uinfo = NULL;
		i += j;
	}
}

/*
 * 332: :head 332 botnick channel :topic
 * TOPIC: :head TOPIC channel :topic
 */
void
on_topic(const struct hook_msg *hmsg)
{
	if (hmsg->raw == NULL)
		return;

	char channel[256];
	get_param(hmsg->raw, channel, 256, 1 + hmsg->fid);
	char topic[512];
	get_tail(hmsg->raw, topic, 512);
	const char *network = get_network_name(hmsg->cid);
	if (network == NULL)
		return;

	struct channel_info *cinfo = get_channel_info(channel, hmsg->cid);
	if (cinfo == NULL) {
		cinfo = make_channel_info(network, channel);
		add_channel_info(cinfo, hmsg->cid);
	}

	free(cinfo->topic);
	cinfo->topic = strdup(topic);

	return;
}

static void
parse_mode_prefix(struct user_info *uinfo, const char *modestr)
{
	int i;
	char c;

	if (modestr == NULL)
		return;

	for (i = 0; (c = modestr[i]) != '\0'; i++) {
		switch (c) {
		case 'G':
			uinfo->away = 1;
			continue;
		case 'H':
			uinfo->away = 0;
			continue;
		case '@':
		case '&':
		case '~':
			uinfo->modes |= CHAN_OP;
			continue;
		case '%':
			uinfo->modes |= CHAN_HOP;
			continue;
		case '+':
			uinfo->modes |= CHAN_VOP;
			continue;
		case '!':
			uinfo->modes |= IRC_OP;
			continue;
		case '*':
			uinfo->oper = 1;
			continue;
		}
	}

	return;
}

static void
get_nick_from_head(char *head, char *nick)
{
	int i;
	for (i = 0; head[i] != '!' && head[i] != '\0'; i++)
		nick[i] = head[i];

	if (nick[i - 1] != '\0')
		nick[i] = '\0';

	return;
}
