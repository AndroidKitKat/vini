#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <fts.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <irc/irc.h>
#include "vinny.h"
#include "ops.h"

#define NAME_MAXLEN 256

struct modinfo {
	char *name;
	char *path;
	int size;
};

struct modinfo **find_modules(char *);
int read_configs(struct modinfo **, int);
int process_module_conf(struct modinfo *, int);

/* this is the function trusted to initiate modules */
int
setup_modules()
{
	int i, ncount = irc_get_network_count();

	char cdir[256];
	const char *network;

	memset(cdir, 0, 256);
	snprintf(cdir, 256, "%s/%s", vinny_headdir, vinny_bindir);

	for (i = -1; i < ncount; i++) {
		if (i >= 0) {
			if (vinny_netdir == NULL)
				break;

			network = get_userdef_network_name(i);
			snprintf(cdir, 256, "%s/%s/%s/%s", vinny_headdir,
			    vinny_netdir, network, vinny_bindir);
		}

		struct modinfo **modules = find_modules(cdir);
		if (modules == NULL) {
			continue;
		}

		/* read_configs frees modules entirely */
		read_configs(modules, i);
	}

	return 0;
}

/* return a NULL terminated array containing each module's name */
struct modinfo **
find_modules(char *dirname)
{
	if (access(dirname, X_OK) == -1)
		return NULL;

	char *dir[] = {dirname, NULL};
	FTS *ftsp = fts_open(dir, FTS_PHYSICAL, NULL);
	FTSENT *fe;

	int bufsz = 256;
	int modc = 0;

	struct modinfo **modules = malloc(bufsz * sizeof(struct modinfo *));
	if (modules == NULL)
		return NULL;

	while ((fe = fts_read(ftsp)) != NULL) {
		/*
		 * Ignore if there was an error, fts hasn't provided a statp,
		 * it's not a file or the file isn't executable by all.
		 */
		if (fe->fts_info == FTS_ERR || fe->fts_info == FTS_NS ||
		    fe->fts_info != FTS_F ||
		    !(fe->fts_statp->st_mode & S_IXOTH))
			continue;

		/* reallocate more memory if we're gone past thebuffer */
		if (modc == bufsz) {
			bufsz += 256;
			modules = realloc(modules,
			    bufsz * sizeof(struct modinfo *));
			if (modules == NULL)
				return NULL;
		}

		modules[modc] = malloc(sizeof(struct modinfo));
		if (modules[modc] == NULL)
			return NULL;
		modules[modc]->name = strdup(fe->fts_name);
		modules[modc]->path = malloc(512);
		snprintf(modules[modc]->path, 512, "%s/%s",
		    dirname, fe->fts_name);
		modules[modc]->size = fe->fts_statp->st_size;
		if (modules[modc]->name != NULL) {
			modc++;
		}
	}

	/* make sure the array can handle one more null pointer */
	if (modc == bufsz) {
		bufsz += 1;
		modules = realloc(modules, bufsz * sizeof(struct modinfo *));
	}

	modules[modc] = NULL;

	fts_close(ftsp);
	return modules;
}

/*
 * grab the config of each module if there is one
 * and make sure it's processed
 */
int
read_configs(struct modinfo **modules, int cid)
{
	int i, mkop;

	int oplen = 512;
	char op[oplen];
	memset(op, 0, oplen);

	struct ircmsg *msg = malloc(sizeof(struct ircmsg));
	memset(msg, 0, sizeof(struct ircmsg));

	struct parser_info pinfo;
	memset(&pinfo, 0, sizeof(struct parser_info));

	for (i = 0; modules[i] != NULL; i++) {
		mkop = process_module_conf(modules[i], cid);

		if (mkop) {
			pinfo.op = op;
			pinfo.name = modules[i]->name;
			pinfo.path = modules[i]->path;
			pinfo.cid = cid;
			process_op(&pinfo);
		}

		mkop = 0;
		free(modules[i]->name);
		free(modules[i]);
	}

	free(modules);
	return 0;
}

/* split the rules in the config file in a null-terminated array */
int
process_module_conf(struct modinfo *module, int cid)
{
	/* Make sure to have enough room for the path, plus extra */
	char confpath[512];
	if (cid == -1)
		snprintf(confpath, 512, "%s/%s/%s.conf",
		    vinny_headdir, vinny_confdir, module->name);
	else
		snprintf(confpath, 512, "%s/%s/%s/%s/%s.conf",
		    vinny_headdir, vinny_netdir, get_userdef_network_name(cid),
		    vinny_confdir, module->name);

	if (access(confpath, R_OK) != 0)
		return 1;

	int fd = open(confpath, O_RDWR);
	if (fd == -1)
		return 1;

	char *conf = mmap(NULL, module->size,
	    PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

	struct ircmsg *msg = malloc(sizeof(struct ircmsg));
	memset(msg, 0, sizeof(struct ircmsg));
	msg->cid = cid;

	struct parser_info pinfo;
	memset(&pinfo, 0, sizeof(struct parser_info));

	int cl = 1;
	char errstr[ERRSTR_MAX], *op, *conf_orig = conf;
	pinfo.errstr = errstr;
	errstr[0] = '\0';
	while ((op = strsep(&conf, "\n")) != NULL && strlen(op) > 1) {
		if (op[0] == '#')
			continue;

		pinfo.op = op;
		pinfo.name = module->name;
		pinfo.path = module->path;
		pinfo.cid = cid;
		if (process_op(&pinfo) != 0)
			printf("error in %s (line %d): %s\n",
			    confpath, cl, errstr);

		cl++;
	}

	munmap(conf_orig, module->size);
	close(fd);
	return 0;
}
