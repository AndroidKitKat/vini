/* core responses to IRC commands */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include "irc.h"

void on_ping(const struct hook_msg *);
void on_client_registered(const struct hook_msg *);
void on_isupport(const struct hook_msg *);
void on_logged_in(const struct hook_msg *);
void on_invite(const struct hook_msg *);
void on_nick_in_use(const struct hook_msg *);
void on_unique_id(const struct hook_msg *);
void on_bot_nick_change(const struct hook_msg *);

static int nick_attempts;

void
setup_core_hooks()
{
	add_irc_hook("042", on_unique_id, NULL, 0);
	add_irc_hook("NICK", on_bot_nick_change, NULL, 0);
	add_irc_hook("PING", on_ping, NULL, 0);
	add_irc_hook("INVITE", on_invite, NULL, 0);
	add_irc_hook("001", on_client_registered, NULL, 0);
	add_irc_hook("005", on_isupport, NULL, 0);
	add_irc_hook("900", on_logged_in, NULL, 0);
	add_irc_hook("433", on_nick_in_use, NULL, 0);

	return;
}

void
on_ping(const struct hook_msg *hmsg)
{
	char response[IRC_MSG_MAX];
	char pingval[IRC_MSG_MAX];
	get_tail(hmsg->raw, pingval, 256);
	snprintf(response, IRC_MSG_MAX, "PONG :%s", pingval);
	send_irc_message(hmsg->cid, response, 0, hmsg->sn);

	return;
}

/* This is where we try to register with nickserv */
void
on_client_registered(const struct hook_msg *hmsg)
{
	struct irc_info *info = get_ircinfo(hmsg->cid);

	if (info->do_login && info->ns_username != NULL && info->ns_password != NULL) {
		char identify_cmd[IRC_MSG_MAX];
		snprintf(identify_cmd, IRC_MSG_MAX, "PRIVMSG NickServ :IDENTIFY %s %s",
		    info->ns_username, info->ns_password);
		send_irc_message(hmsg->cid, identify_cmd, 0, hmsg->sn);
	} else if (info->channels != NULL) {
		irc_join(hmsg->cid, info->channels);
	}

	return;
}

void
on_isupport(const struct hook_msg *hmsg)
{
	char netname[IRC_MSG_MAX];
	memset(netname, 0, IRC_MSG_MAX);
	char *neteq = "NETWORK=";
	const char *msg = hmsg->raw;

	int i, j = 0, record = 0;
	for (i = 0; msg[i] != '\0'; i++) {
		if (record) {
			if (msg[i] == ' ')
				break;
			netname[j++] = msg[i];
		}

		if (msg[i] == neteq[j] && !record) {
			j++;
			if (neteq[j] == '=') {
				record = 1;
				j = 0;
				i++;
			}
		}
	}

	if (netname[0] != '\0')
		set_network_name(hmsg->cid, netname);
	return;
}

/* This is where we decide to join channels */
void
on_logged_in(const struct hook_msg *hmsg)
{
	struct irc_info *info = get_ircinfo(hmsg->cid);
	if (info->channels != NULL)
		irc_join(hmsg->cid, info->channels);

	return;
}

/* Join a channel when invited */
void
on_invite(const struct hook_msg *hmsg)
{
	struct irc_info *info = get_ircinfo(hmsg->cid);
	char target[256];
	get_param(hmsg->raw, target, 256, 1);
	char channel[256];
	get_tail(hmsg->raw, channel, 256);

	if (strncmp(target, info->name, strlen(info->name)) == 0)
		irc_join(hmsg->cid, channel);

	return;
}

void
on_nick_in_use(const struct hook_msg *hmsg)
{
	extern char *irc_suffixlist[];
	if (irc_suffixlist[nick_attempts] == NULL)
		err(1, "No available nicks");

	struct irc_info *ircinfo = get_ircinfo(hmsg->cid);
	char command[IRC_MSG_MAX];

	// TODO: update ircinfo->name
	snprintf(command, IRC_MSG_MAX, "NICK %s%s", ircinfo->name, irc_suffixlist[nick_attempts]);
	send_irc_message(hmsg->cid, command, 0, hmsg->sn);
	nick_attempts++;
	return;
}

/* We're connected, take the opportunity to make sure we know our nick */
void
on_unique_id(const struct hook_msg *hmsg)
{
	char nick[64];
	get_param(hmsg->raw, nick, 64, 1);
	set_bot_name(hmsg->cid, hmsg->sn, nick);
	return;
}

void
on_bot_nick_change(const struct hook_msg *hmsg)
{
	if (hmsg->raw == NULL)
		return;

	char *oldnick = get_bot_name(hmsg->cid, hmsg->sn);
	if (oldnick == NULL)
		return;

	char newnick[64];
	get_param(hmsg->raw, newnick, 64, 1);
	char sender[256];
	get_sender(hmsg->raw, sender, 256);
	char msgnick[256];
	int i;

	for (i = 0; i < 255 && sender[i] != '!' && sender[i] != '\0'; i++)
		msgnick[i] = sender[i];
	msgnick[i] = '\0';

	if (strcmp(msgnick, oldnick) == 0)
		/* our nick changed */
		set_bot_name(hmsg->cid, hmsg->sn, newnick);

	return;
}
