#include <sys/queue.h>
#include <sys/wait.h>

#include <dlfcn.h>
#include <fts.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include <errno.h>
#include <err.h>

#include <irc/irc.h>
#include <config.h>
#include <irc_config.h>
#ifdef WITH_IRC_INFO
#include <info/info.h>
#endif

struct vini_mod {
	char *name;
	char *path;
	void *handle;
	struct irc_module *mod;
	SLIST_ENTRY(vini_mod) next;
};

void cli_load(char **);
void cli_unload(char **);
void cli_reload(char **);
void cli_reloadlib(char **);
void cli_listmods(char **);

const static struct clicmd {
	char *name;
	void (*function)(char **);
} clicmds[] = {
	{"load", cli_load},
	{"unload", cli_unload},
	{"reload", cli_reload},
	{"reloadlib", cli_reloadlib},
	{"listmods", cli_listmods},
	{NULL}
};

void read_cli();
void parse_cli();

static SLIST_HEAD(vini_mod_head, vini_mod) mods;
void open_mods(void);
void load_mod(const char *, const char *);
void free_mod(struct vini_mod *);
void set_initial_settings(void);
void setup_env(void);
void print_usage(void);
void sigchld_handler(int);
const char *cmd_prefix[] = CMD_PREFIX;

char *optstring = "hv";
char *usage = "usage: vini [-h] [-v]";

int
main(int argc, char **argv)
{
	char c;
	int ret = 0;

	set_initial_settings();
	hcreate(0);

	/* getopt */
	while ((c = getopt(argc, argv, optstring)) != -1) {
		switch(c) {
			case 'v': /* verbose (print raw IRC messages) */
				irc_verbose = 1;
				break;
			case 'h': /* show help */
			default:
				print_usage();
		}
	}
	errno = 0;

	/* setup SIGCHLD catcher */
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigchld_handler;
	sa.sa_flags = SA_RESTART;
	ret = sigaction(SIGCHLD, &sa, NULL);
	if (ret)
		err(ret, "sigaction failed for SIGCHLD");

	/* setup the environment */
	setup_env();
	if (errno)
		err(ret, "setup_env failed");

#ifdef WITH_IRC_INFO
	ret = setup_info();
	if (ret)
		err(ret, "setup_info failed");
#endif

	irc_main(0);
	open_mods();
	read_cli();

	return 0;
}

void
read_cli()
{
	char c;
	char buf[8096];
	memset(buf, 0, sizeof(buf));
	int i = 0;
	write(1, "> ", 2);

	while (read(0, &c, 1) > 0) {
		if (c == '\n') {
			buf[i] = '\0';
			parse_cli(buf);
			i = 0;
			write(1, "> ", 2);
		} else {
			buf[i++] = c;
		}

		if (i == 8096)
			i = 0;
	}
}

void
parse_cli(char *buf)
{
	int i, n = 1;
	char *args[32];
	args[0] = buf;

	for (i = 0; buf[i] != '\0' && n < sizeof(args) - 1; i++) {
		if (buf[i] == ' ' || buf[i] == '\n') {
			buf[i] = '\0';
			i++;
			args[n++] = buf + i;
		}
	}

	args[n] = NULL;

	for (i = 0; clicmds[i].name != NULL; i++)
		if (strcmp(clicmds[i].name, args[0]) == 0)
			return clicmds[i].function(args + 1);

	printf("Unknown command: %s\n", args[0]);
}

void
cli_load(char **args)
{
	int i;
	for (i = 0; args[i] != NULL && args[i + 1] != NULL; i++) {
		load_mod(args[i], args[i + 1]);
	}
}

void
cli_unload(char **args)
{
	int i;
	struct vini_mod *mod, *tmod;
	for (i = 0; args[i] != NULL; i++) {
		SLIST_FOREACH_SAFE(mod, &mods, next, tmod) {
			if (strcmp(mod->name, args[i]) == 0) {
				printf("Unloading %s\n", mod->name);
				mod->mod->free();
				SLIST_REMOVE(&mods, mod, vini_mod, next);
				free_mod(mod);
				break;
			}
		}
	}
}

void
cli_reload(char **args)
{
	int i;
	struct vini_mod *mod;
	for (i = 0; args[i] != NULL; i++) {
		SLIST_FOREACH(mod, &mods, next) {
			if (strcmp(mod->name, args[i]) == 0) {
				printf("Reloading %s\n", mod->name);
				mod->mod->reload();
			}
		}
	}
}

void
cli_reloadlib(char **args)
{
	int i;
	struct vini_mod *mod;
	for (i = 0; args[i] != NULL; i++) {
		SLIST_FOREACH(mod, &mods, next) {
			if (strcmp(mod->name, args[i]) == 0) {
				printf("Reloading module %s (%s)\n", mod->name, mod->path);
				mod->mod->free();
				dlclose(mod->handle);
				mod->handle = dlopen(mod->path, RTLD_NOW);
				mod->mod = (struct irc_module *)dlsym(mod->handle, mod->name);
				mod->mod->init();
			}
		}
	}
}

void
cli_listmods(char **args)
{
	struct vini_mod *mod;
	SLIST_FOREACH(mod, &mods, next)
		printf("%s (%s)\n", mod->name, mod->path);
}

void
open_mods()
{
	char path[1024];
	char name[512];
	snprintf(path, 1024, "%s/%s", HEADDIR, MODDIR);
	char *dirs[] = {path, NULL};
	FTS *f = fts_open(dirs, FTS_LOGICAL, NULL);
	FTSENT *fe;
	if (f == NULL)
		return;

	fe = fts_read(f);
	if (fe == NULL)
		return;

	char *file_ext;
	for (fe = fts_children(f, 0); fe != NULL; fe = fe->fts_link) {
		if (fe->fts_info == FTS_F && fe->fts_namelen > 4 &&
		    strncmp(fe->fts_name, "lib", 3) == 0 &&
		    strcmp(fe->fts_name + (fe->fts_namelen - 3), ".so") == 0) {
			snprintf(path, sizeof(path), "%s/%s", fe->fts_path, fe->fts_name);
			memset(name, 0, sizeof(name));
			strncpy(name, fe->fts_name + 3, fe->fts_namelen - 6);
			load_mod(path, name);
		}
	}
}

void
load_mod(const char *path, const char *name)
{
	printf("Loading module %s (%s)\n", name, path);
	struct vini_mod *mod = malloc(sizeof(struct vini_mod));
	mod->name = strdup(name);
	mod->path = strdup(path);
	mod->handle = dlopen(path, RTLD_NOW);
	mod->mod = (struct irc_module *)dlsym(mod->handle, name);
	if (mod->mod == NULL) {
		puts(dlerror());
		free_mod(mod);
	} else {
		SLIST_INSERT_HEAD(&mods, mod, next);
		mod->mod->init();
	}
}

void
free_mod(struct vini_mod *mod)
{
	free(mod->name);
	dlclose(mod->handle);
}

void
sigchld_handler(int sig)
{
	int stat;
	pid_t pid = wait(&stat);

	struct vini_mod *mod;
	SLIST_FOREACH(mod, &mods, next) {
		if (mod->mod->sigchld != NULL)
			mod->mod->sigchld(pid);
	}
}

void
set_initial_settings()
{
	irc_verbose = 0;
}

void
setup_env()
{
	setenv("LC_ALL", "C", 1);
	setenv("BOTNAME", BOTNAME, 1);
}

void
print_usage()
{
	puts(usage);
	exit(0);
}
