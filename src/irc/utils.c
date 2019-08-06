/* Provide functions to quickly form common raw messages to send to IRC */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "irc.h"

void
irc_join(int cid, const char *target)
{
	char cmd[IRC_MSG_MAX];
	snprintf(cmd, IRC_MSG_MAX, "JOIN :%s", target);
	send_irc_message(cid, cmd, 1, -1);
	return;
}

void
irc_part(int cid, const char *msg, const char *target)
{
	char cmd[IRC_MSG_MAX];
	snprintf(cmd, IRC_MSG_MAX, "PART %s :%s", target, msg);
	send_irc_message(cid, cmd, 1, -1);
	return;
}

void
irc_msg(int cid, const char *msg, const char *target)
{
	char cmd[IRC_MSG_MAX];
	snprintf(cmd, IRC_MSG_MAX, "PRIVMSG %s :%s", target, msg);
	send_irc_message(cid, cmd, 0, -1);
	return;
}

void
irc_action(int cid, const char *msg, const char *target)
{
	char cmd[IRC_MSG_MAX];
	snprintf(cmd, IRC_MSG_MAX, "PRIVMSG %s \001ACTION :%s", target, msg);
	send_irc_message(cid, cmd, 0, -1);
	return;
}

void
irc_notice(int cid, const char *msg, const char *target)
{
	char cmd[IRC_MSG_MAX];
	snprintf(cmd, IRC_MSG_MAX, "NOTICE %s :%s", target, msg);
	send_irc_message(cid, cmd, 0, -1);
	return;
}
