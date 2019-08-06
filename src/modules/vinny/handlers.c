#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>

#include <irc/irc.h>
#include <config.h>
#include "vinny.h"
#include "ops.h"
#include "parser.h"
#include "mods.h"

int sendto(struct parser_info *pinfo, void (*)(int, const char *, const char *));
int cmd_maker(struct parser_info *pinfo, int (*)(char *, struct mod_op *));

/* REPLY message */
int
parse_reply(struct parser_info *pinfo)
{
	if (pinfo->channel != NULL) {
		irc_msg(pinfo->cid, pinfo->op, pinfo->channel);
	}

	return 0;
}

/* ACTION message */
int
parse_action(struct parser_info *pinfo)
{
	if (pinfo->channel != NULL) {
		irc_action(pinfo->cid, pinfo->op, pinfo->channel);
	}

	return 0;
}

int
parse_join(struct parser_info *pinfo)
{
	irc_join(pinfo->cid, pinfo->op);
	return 0;
}

int
parse_part(struct parser_info *pinfo)
{
	irc_part(pinfo->cid, "im out", pinfo->op);
	return 0;
}

/*
 * Get rid of the first command, assume the target is in
 * the second word, and use the argument func to send to the target
 */
int
sendto(struct parser_info *pinfo, void (*func)(int, const char *, const char *))
{
	int index = 0;

	/* Grab the target */
	char *target = get_next_word(pinfo->op, &index, pinfo->errstr);
	if (target == NULL)
		return -1;

	func(pinfo->cid, pinfo->op + index, target);
	free(target);
	return 0;
}


int
parse_send(struct parser_info *pinfo)
{
	return sendto(pinfo, irc_msg);
}

int
parse_notice(struct parser_info *pinfo)
{
	return sendto(pinfo, irc_notice);
}

int
parse_actto(struct parser_info *pinfo)
{
	return sendto(pinfo, irc_action);
}

int
parse_raw(struct parser_info *pinfo)
{
	send_irc_message(pinfo->cid, pinfo->op, 0, -1);
	return 0;
}

int
parse_getlog(struct parser_info *pinfo)
{
#ifdef WITH_PG
	if (pinfo->rsp == NULL)
		return 0;

	char **regexps = delistify(pinfo->op, pinfo->errstr);

	if (regexps == NULL || pinfo->msg == NULL) {
		strcpy(pinfo->rsp, "%%ERROR%%\n");
		return -1;
	}

	char *log = access_logs(regexps, pinfo->msg);
	if (log == NULL)
		log = strdup("%%ERROR%%\n");

	int i;
	for (i = 0; regexps[i] != NULL; i++)
		free(regexps[i]);
	free(regexps);

	strcpy(pinfo->rsp, log);
	free(log);
#endif
	return 0;
}

int
cmd_maker(struct parser_info *pinfo, int (*maker)(char *, struct mod_op *))
{
	struct mod_op *mop;

	int cid = pinfo->cid;
	char *op = pinfo->op;
	char *name = strdup(pinfo->name);
	char *path = strdup(pinfo->path);
	char *errstr = pinfo->errstr;
	if (path == NULL)
		return -1;

	char *head, *tail, *channelstr;
	head = tail = channelstr = NULL;

	char **keys, **argv, **channels;
	keys = argv = channels = NULL;

	int i, ret;

	ret = split_maker_parts(op, &head, &tail, &channelstr, errstr);
	if (head == NULL || ret == -1) {
		/* implies all the others are null and errstr is set */
		ret = -1;
		goto cmd_maker_err;
	}

	keys = delistify(head, errstr);
	if (keys == NULL || keys[0] == NULL) {
		ret = -1;
		goto cmd_maker_err;
	}

	argv = parse_argv(name, tail, errstr);

	/* An empty argv should at least contain the module name and the NULL */
	if (argv == NULL || argv[0] == NULL) {
		for (i = 0; keys[i] != NULL; i++)
			free(keys[i]);
		free(keys);
		ret = -1;

		set_errstr(pinfo->errstr, "argv is null");
		goto cmd_maker_err;
	}

	if (channelstr != NULL) {
		channels = delistify(channelstr, errstr);
	}

	for (i = 0; keys[i] != NULL; i++) {
		mop = malloc(sizeof(struct mod_op));
		mop->name = name;
		mop->path = path;
		mop->cid = cid;
		mop->chans = channels;
		mop->argv = argv;
		maker(keys[i], mop);
	}

	free(keys);
cmd_maker_err:
	free(tail);
	free(channelstr);
	free(head);
	return ret;
}

int
parse_def(struct parser_info *pinfo)
{
	return cmd_maker(pinfo, mk_cmd);
}

int
parse_hook(struct parser_info *pinfo)
{
	return cmd_maker(pinfo, mk_hook);
}

int
parse_match(struct parser_info *pinfo)
{
	return cmd_maker(pinfo, mk_rgx);
}
