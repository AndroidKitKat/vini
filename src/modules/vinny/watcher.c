#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "vinny.h"
#include "mods.h"
#include "ops.h"

void *
mod_watcher(void *vmodp)
{
	struct vinny_module vmod = *(struct vinny_module *)vmodp;
	int i = 0;

	struct ircmsg *msg = vmod.msg;
	int so = vmod.socket;

	int buflen = 2<<16;
	char *buf = malloc(buflen);
	char rsp[RSPLEN_MAX];
	pid_t cpid = vmod.child_pid;
	memset(rsp, 0, RSPLEN_MAX);

	struct parser_info pinfo;
	memset(&pinfo, 0, sizeof(struct parser_info));

	VDEBUG("in watcher thread");

#ifdef MOD_DEBUG
	char errstr[ERRSTR_MAX];
	errstr[0] = '\0';
#else
	char *errstr = NULL;
#endif

	pinfo.name = vmod.name;
	pinfo.op = buf;
	pinfo.cid = msg->cid;
	pinfo.channel = msg->channel;
	pinfo.msg = msg;
	pinfo.rsp = rsp;
	pinfo.errstr = errstr;

	FILE *cf = fdopen(so, "r+");

	char c;
	while((c = getc(cf)) != EOF) {
		switch(c) {
		case '\r':
		case '\n':
		case '\0':
			buf[i] = '\0';
			if (buf[0] == '\0')
				continue;

			VDEBUG("Got a command to process");
			process_op(&pinfo);
			if (kill(cpid, 0) == 0 && rsp[0] != '\0') {
				fputs(rsp, cf);
				rsp[0] = '\0';
			}

			i = 0;
			break;
		default:
			if (i == buflen) {
				if (buflen == 2<<14)
					goto exit_watcher;

				buflen *= 2;
				buf = realloc(buf, buflen);
			}

			buf[i++] = c;
			break;
		}
	}

exit_watcher:
	pthread_detach(pthread_self());

	unregister_msg_holder(msg, pthread_self());

	fclose(cf);
	close(so);
	free(vmodp);
	free(buf);
	return NULL;
}
