/* Provide functions to parse raw IRC messages into smaller blocks */
#include <string.h>
#include "irc.h"

/* (:sender )?COMMAND( params)?( :tail)? */

/* return the sender */
char *
get_sender(const char *msg, char *buf, int size)
{
	if (msg == NULL || buf == NULL || msg[0] != ':')
		return NULL;

	char sender[512];

	int j = 0, i = (msg[0] == ':') ? 1 : 0;
	for (; msg[i] != ' '; i++)
		sender[j++] = msg[i];

	sender[j] = '\0';
	strncpy(buf, sender, size);
	buf[size - 1] = '\0';
	return buf;
}

/* return the COMMAND */
char *
get_command(const char *msg, char *buf, int size)
{
	if (msg == NULL || buf == NULL)
		return NULL;

	char command[32];

	int i;
	if (msg[0] == ':') {
		for (; *msg != ' '; msg++);
		msg++;
	}

	for (i = 0; msg[i] != ' ' && msg[i] != '\r' &&
	    msg[i] != '\n' && msg[i] != '\0'; i++)
		command[i] = msg[i];

	command[i] = '\0';
	strncpy(buf, command, size);
	buf[size - 1] = '\0';
	return buf;
}

/*
 * return param no. n, or NULL if there aren't that many before the tail
 */
char *
get_param(const char *msg, char *buf, int size, int n)
{
	if (msg == NULL || buf == NULL || n < 1)
		return NULL;

	char param[strlen(msg)];

	/* there's a sender part, get rid of it */
	if (msg[0] == ':') {
		while (*msg != ' ')
			msg++;
		msg++;
	}

	int i, j, nspaces = 0;
	for (i = 0; nspaces < n; i++) {
		switch(msg[i]) {
		case '\0':		/* we're at the end */
		case ':':		/* we're past the tail */
			return NULL;
		case ' ':
			nspaces++;
			/* FALLTHROUGH */
		default:
			continue;
		}
	}

	if (msg[i] == ':') {
		/*
		 * we're asked for the tail, which is a valid parameter
		 * according to RFC
		 */
		return get_tail(msg, buf, size);
	}

	j = 0;
	while (msg[i] != ' ' && msg[i] != '\r' &&
	    msg[i] != '\n' && msg[i] != '\0')
		param[j++] = msg[i++];

	param[j] = '\0';
	strncpy(buf, param, size);
	buf[size - 1] = '\0';
	return buf;
}

/*
 * return the tail
 */
char *
get_tail(const char *msg, char *buf, int size)
{
	if (msg == NULL || buf == NULL)
		return NULL;

	char tail[IRC_MSG_MAX];

	/* set i to the index of first instance of " :", or the null byte */
	int i, j;
	for (i = 0; msg[i] != '\0' &&
	    !(msg[i] == ' ' && msg[i + 1] == ':'); i++)
		;

	if (msg[i] == '\0')
		return NULL;

	i += 2;
	j = 0;
	while(msg[i] != '\r' && msg[i] != '\n' && msg[i] != '\0')
		tail[j++] = msg[i++];

	tail[j] = '\0';
	strncpy(buf, tail, size);
	buf[size - 1] = '\0';
	return buf;
}
