#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <irc/irc.h>
#include "vinny.h"
#include "ops.h"
#include "parser.h"

#define BUFSIZE 8192

int parse_op(char *op, char *errstr);
char *parse_cmd(char *op, char *errstr);
int get_cmd_index(char *op, char *errstr);
int is_op_end(char *op, int i);
int is_valid_cmd(char *cmd);

struct parsercmd {
	char *name;
	int (*handler)(struct parser_info *);
};
static struct parsercmd commands[] = {
	{ "REPLY",	&parse_reply },
	{ "ACTION",	&parse_action },
	{ "JOIN",	&parse_join },
	{ "PART",	&parse_part },
	{ "SEND",	&parse_send },
	{ "NOTICE",	&parse_notice },
	{ "ACTTO",	&parse_actto },
	{ "RAW",	&parse_raw },
	{ "GETLOG",	&parse_getlog },
	{ "DEF",	&parse_def },
	{ "HOOK",	&parse_hook },
	{ "MATCH",	&parse_match },
	{ NULL,		NULL }
};

/*
 * Parse an operation and do the action it asks
 * Return 1 if it failed because no command could be found,
 * -1 if it failed because a subcommand failed
 * 0 if it succeeded.
 * op is the operation to parse
 * name is the name of the module we're called from or for
 * chan is the channel we're called for, or NULL
 * rsp is where any necessary response to stdin is stored in
 * errstr is where any debugging information to a parsing error is stored in
 */
int
process_op(struct parser_info *pinfo)
{
	int ret = 0;

	if (pinfo == NULL || pinfo->op == NULL) {
		set_errstr(pinfo->errstr, "parser info is NULL");
		return -1;
	}

	/*
	 * Use a local op to hold our promise of not changing the string,
	 * confine to a length of IRC_MSG_MAX for safety, and make sure there's
	 * room to add a last \r, \n and \0 if necessary.
	 */
	int oplen = strlen(pinfo->op);

	char *lop = malloc(oplen + 3);
	memset(lop, 0, oplen + 3);
	strncpy(lop, pinfo->op, oplen);

	if (lop == NULL) {
		set_errstr(pinfo->errstr, "Failed to allocate memory");
		return -1;
	}

	char *op_orig = pinfo->op;
	pinfo->op = lop;

	int i;
	/* Get rid of any \r and \n */
	for (i = 0; i < strlen(lop); i++)
		if (lop[i] == '\r' || lop[i] == '\n')
			lop[i] = '\0';

	int cmdindex = get_cmd_index(lop, pinfo->errstr);
	if (cmdindex < 0 || commands[cmdindex].name == NULL) {
		if (pinfo->channel != NULL) {
			/* default to REPLY */
			irc_msg(pinfo->cid, op_orig, pinfo->channel);
			goto exit_process_op;
		} else {
			set_errstr(pinfo->errstr, "invalid command");
			ret = -1;
			goto exit_process_op;
		}
	}

	/* Get rid of the command so that handlers don't have to strip it out */
	int argindex = index_to_arg(lop, 1, pinfo->errstr);
	if (argindex == -1) {
		set_errstr(pinfo->errstr, "invalid amount of args");
		ret = -1;
		goto exit_process_op;
	}

	pinfo->op += argindex;

	ret = commands[cmdindex].handler(pinfo);

exit_process_op:
	free(lop);
	pinfo->op = op_orig;

	return ret;
}

int
get_cmd_index(char *op, char *errstr)
{
	char buf[512];
	int i;
	for (i = 0; i < 512 && i < BUFSIZE && op[i] != ' ' && op[i] != '\0'; i++)
		buf[i] = op[i];

	if (i == BUFSIZE) {
		set_errstr(errstr, "invalid command");
		return -1;
	}

	buf[i] = '\0';

	for (i = 0;
	    i < 512 &&
	    commands[i].name != NULL &&
	    strcmp(buf, commands[i].name);
	    i++)
		;

	return i;
}

int
index_to_arg(char *op, int argn, char *errstr)
{
	int i;
	for (i = 0; op[i] != '\0' && argn > 0; i++) {
		if (op[i] == ' ')
			argn--;
	}

	while (op[i] == ' ')
		i++;

	if (op[i] == '\0') {
		set_errstr(errstr, "bad amount of args for command");
		return -1;
	}

	return i;
}

/*
 * Get the next word starting at the value of index,
 * and store the start of the next word in index.
 */
char *
get_next_word(char *op, int *index, char *errstr)
{
	int i = 0;
	int wsize = 0;

	if (index != NULL)
		i = *index;

	for (; op[i] != ' ' && op[i] != '\0'; i++)
		;

	wsize = i;

	while (op[i] == ' ')
		i++;

	if (i == 0) {
		set_errstr(errstr, "bad amount of args for command");
		return NULL;
	}

	if (index != NULL)
		*index = i;

	char *rop = malloc(sizeof(op));
	strncpy(rop, op, wsize);
	rop[i + 1] = '\0';
	return rop;
}

/* Check if the command exists */
int
is_valid_cmd(char *cmd)
{
	int i;
	for (i = 0; cmd[i] != '\0'; i++)
		if (strcmp(cmd, commands[i].name) == 0)
			return 1;

	return 0;
}

/*
 * Take a string in the format:
 * something, something, some:thing, \:something:
 *   arg1, "string for arg2", arg3 | #channel1, #channel2
 * And split it in 3, each in their own new string:
 * *headp = "something,something,some:thing,\:something\0"
 * *tailp = "arg1,"string for arg2",arg3\0"
 * *channelp = "#channel1,#channel2\0"
 * op should not be modified.
 */
int
split_maker_parts(char *op, char **headp, char **tailp,
    char **chansp, char *errstr)
{
	int i, j = 0, pos = 0, len = strlen(op);
	char quotec;

	if (op[0] == ':') {
		set_errstr(errstr, "head empty");
		return -1;
	}

	char lhead[len + 1];
	char ltail[len + 1];
	char lchans[len + 1];

	memset(lhead, 0, len + 1);
	memset(ltail, 0, len + 1);
	memset(lchans, 0, len + 1);

	/*
	 * pos = 0: append to head
	 * pos = 1: append to tail
	 * pos = 2: append to chans
	 */
	for (i = 0; i < len; i++) {
		switch(pos) {
		case 0:
			/* Skip all spaces */
			while (op[i] == ' ')
				i++;

			if ((op[i] == ':' && op[i - 1] != '\\' &&
			    op[i + 1] == ' ') || op[i] == '\0') {
				pos++;
				j = 0;
			} else {
				lhead[j++] = op[i];
			}

			break;
		case 1:
			while (op[i] == ' ')
				i++;

			/* Handle quotes */
			if (op[i - 1] != '\\' &&
			    (op[i] == '\'' || op[i] == '"')) {
				quotec = op[i++];
				while (op[i] != quotec && op[i - 1] != '\\'
				    && op[i] != '\0')
					ltail[j++] = op[i++];

				if (op[i] == '\0') {
					set_errstr(errstr, "unclosed quote");
					return -1;
				}

				/* get rid of the closing quote */
				i++;
			}


			if (op[i] == '|' && op[i - 1] != '\\') {
				pos++;
				j = 0;
			} else if (op[i] != ' ') {
				ltail[j++] = op[i];
			}

			break;
		case 2:
			while (op[i] == ' ')
				i++;

			lchans[j++] = op[i];
			break;
		default:
			set_errstr(errstr,
			    "[bug] pos contains an invalid value");
			return -1;
		}
	}

	*headp = (lhead[0] == '\0') ? NULL : strdup(lhead);
	*tailp = (ltail[0] == '\0') ? NULL : strdup(ltail);
	*chansp = (lchans[0] == '\0') ? NULL : strdup(lchans);

	return 0;
}

/* Separate a list of words or quoted strings into an array */
char **
delistify(char *s, char *errstr)
{
	int i, j = 0, k = 0, seps = 0, qcount = 0, len = strlen(s);
	char qc;

	/* Initial scan */
	for (i = 0; i < len; i++) {
		if (s[i] == ',')
			seps++;

		if ((s[i] == '"' || s[i] == '\'') &&
		    i != 0 && s[i - 1] != '\\') {
			qc = s[i++];
			while(s[i] != qc && s[i - 1] != '\\' && s[i] != '\0')
				i++;

			if (s[i] == '\0' && s[i - 1] != qc) {
				set_errstr(errstr, "invalid amount of quotes");
				return NULL;
			}

			qcount++;
			i++;
		}
	}

	if (qcount % 2 != 0) {
		set_errstr(errstr, "invalid amount of quotes");
		return NULL;
	}

	char **a = malloc((seps + 2) * sizeof(char *));
	if (a == NULL) {
		set_errstr(errstr, "failed to allocate memory");
		return NULL;
	}

	a[0] = malloc(len + 1);
	for (i = 0; i < len; i++) {
		/* Handle quotes */
		if ((s[i] == '"' || s[i] == '\'') &&
		    i != 0 && s[i - 1] != '\\') {

			qc = s[i++];
			while (s[i] != qc && s[i - 1] != '\\' && s[i] != '\0')
				a[k][j++] = s[i++];

			/*
			 * This should never happen because
			 * we checked for it already..
			 */
			if (s[i] == '\0' && s[i - 1] != qc) {
				for (; k >= 0; k--)
					free(a[k]);
				free(a);
				set_errstr(errstr, "bad amount of quotes");
				return NULL;
			}

			/* Get rid of the terminating quote */
			i++;
		}

		a[k][j++] = s[i];
		if (s[i] == ',' && i != 0 && s[i - 1] != '\\') {
			a[k][j - 1] = '\0';
			j = 0;
			a[++k] = malloc(len);
			if (k == seps - 2) {
				seps *= 2;
				a = realloc(a, (seps + 2) * sizeof(char *));
			}

			/* get rid of any spaces after the comma */
			while (s[i + 1] == ' ')
				i++;
		}
	}

	a[k++][j] = '\0';
	a[k] = NULL;
	return a;
}

char **
parse_argv(char *name, char *s, char *errstr)
{
	int i;

	if (s == NULL) {
		char **argv = malloc(2 * sizeof(char *));
		argv[0] = name;
		argv[1] = NULL;
		return argv;
	}

	char **args = delistify(s, errstr);
	if (args == NULL)
		return NULL;

	for (i = 0; args[i] != NULL; i++)
		;

	int len = i;
	char **argv = malloc((len + 2) * sizeof(char *));
	argv[0] = name;
	for (i = 1; i <= len; i++)
		argv[i] = args[i - 1];

	argv[i] = NULL;
	free(args);
	return argv;
}

void
set_errstr(char *errstr, char *str)
{
	if (errstr == NULL)
		return;

	int i;
	for (i = 0; i < strlen(str); i++)
		errstr[i] = str[i];
	errstr[i] = '\0';

	return;
}
