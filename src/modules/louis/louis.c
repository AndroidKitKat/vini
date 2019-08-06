#include <fts.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <config.h>
#include <irc/irc.h>
#include "louis.h"
#include "lualib.h"
#include "periodic.h"

static void extdir_walk(char *);
static void load_ext(char *);
void *exec_hook(void *);
static bool match_cmds(const char *, char **);

static void free_ext(struct ext *);
static void free_hook(struct hook *);

void free_louis(void);
int reload_louis(void);

struct irc_module louis = {
	.init = init_louis,
	.free = free_louis,
	.reload = reload_louis,
	.sigchld = NULL
};

void
to_louis(const struct hook_msg *hmsg)
{
	if (hooks[hmsg->fid] == NULL || hooks[hmsg->fid]->ext == NULL) {
		return;
	}

	if (hooks[hmsg->fid]->cmds != NULL && !match_cmds(hmsg->raw, hooks[hmsg->fid]->cmds))
		return;

	struct state *S = malloc(sizeof(struct state));
	if (S == NULL)
		return;

	S->msg = strdup(hmsg->raw);
	S->cid = hmsg->cid;
	S->sn = hmsg->sn;
	S->fid = hmsg->fid;
	if (S->msg == NULL) {
		free(S);
		free(S->msg);
		return;
	}

	pthread_t thread;
	pthread_create(&thread, NULL, exec_hook, S);
	return;
}

int
init_louis()
{
	char luadir[256];
	char cluadir[256];
	snprintf(luadir, 256, "%s/%s/lua/?.lua", HEADDIR, LIBDIR);
	snprintf(cluadir, 256, "%s/%s/lua/?.so", HEADDIR, LIBDIR);

	setenv("LUA_PATH", luadir, 1);
	setenv("LUA_CPATH", cluadir, 1);

	extsize = 1024;
	hksize = 1024;
	exts = malloc(extsize * sizeof(struct ext *));
	hooks = malloc(hksize * sizeof(struct hook *));
	if (exts == NULL || hooks == NULL)
		return 1;

	memset(exts, 0, extsize * sizeof(struct ext *));
	memset(hooks, 0, hksize * sizeof(struct hook *));

	char extdir[1024];
	snprintf(extdir, 1024, "%s/%s", HEADDIR, EXTDIR);
	extdir_walk(extdir);

	pthread_t thread;
	pthread_create(&thread, NULL, periodic_main, NULL);

	return 0;
}

void
free_louis()
{
	int i;

	destroy_periodics();
	struct ext *e;
	for (i = 0; i < extc; i++) {
		e = exts[i];
		exts[i] = NULL;

		free_ext(e);
	}

	extc = 0;
	hookc = 0;
}

int
reload_louis()
{
	free_louis();
	return init_louis();
}

static void
free_ext(struct ext *e)
{
	struct hook *h;
	int i;

	pthread_mutex_lock(&e->S->mtx);
	lua_close(e->lua);
	free(e->S->msg);

	for (i = 0; i < hookc; i++) {
		if (hooks[i] != NULL && hooks[i]->ext == e) {
			h = hooks[i];
			hooks[i] = NULL;

			/* rm_irc_hook will call free_hook_fid */
			rm_irc_hook(h->irccmd, to_louis, h->fid);
		}
	}

	pthread_mutex_unlock(&e->S->mtx);
	free(e->S);
	free(e);
}

void
free_hook_fid(int fid)
{
	free_hook(hooks[fid]);
}

static void
free_hook(struct hook *h)
{
	int i;

	if (h == NULL)
		return;

	free(h->fname);
	free(h->irccmd);
	if (h->cmds != NULL)
		for (i = 0; h->cmds[i] != NULL; i++)
			free(h->cmds[i]);

	free(h);
}

static void
extdir_walk(char *path)
{
	int buflen = 1024;
	char buf[buflen];

	char *dir[] = {path, NULL};
	FTS *f = fts_open(dir, FTS_LOGICAL, NULL);
	FTSENT *fe;
	if (f == NULL)
		return;

	fe = fts_read(f);
	if (fe == NULL)
		return;

	char *file_ext;
	for (fe = fts_children(f, 0); fe != NULL; fe = fe->fts_link) {
		if (fe->fts_info == FTS_F && fe->fts_namelen > 4) {
			file_ext = fe->fts_name + (fe->fts_namelen - 4);
			if (strncmp(file_ext, ".lua", 4) == 0) {
				snprintf(buf, buflen, "%s/%s",
				    fe->fts_path, fe->fts_name);
				load_ext(buf);
			}
		}
	}
}

static void
load_ext(char *path)
{
	printf("loading %s... ", path);
	if (access(path, R_OK) != 0) {
		return;
	}

	if (extc == extsize) {
		extsize *= 2;
		exts = realloc(exts, extsize);
	}

	struct ext *e = malloc(sizeof(struct ext));
	lua_State *L = luaL_newstate();
	e->lua = L;
	e->S = malloc(sizeof(struct state));
	memset(e->S, 0, sizeof(struct state));
	pthread_mutex_init(&e->S->mtx, NULL);
	exts[extc++] = e;

	luaL_openlibs(L);
	luaL_newlib(L, l_irclib);
	lua_setglobal(L, "irc");

	int err = luaL_loadfile(L, path);
	if (err) {
		puts(lua_tostring(L, lua_gettop(L)));
		return;
	}

	err = lua_pcall(L, 0, 0, 0);
	if (err) {
		puts(lua_tostring(L, lua_gettop(L)));
		return;
	}

	printf("loaded.\n");
}

void *
exec_hook(void *vs)
{
	struct state *tmpS = (struct state *)vs;
	struct state *S = hooks[tmpS->fid]->ext->S;

	pthread_mutex_lock(&S->mtx);

	free(S->msg);
	S->msg = tmpS->msg;
	S->sn = tmpS->sn;
	S->cid = tmpS->cid;
	S->fid = tmpS->fid;
	struct hook *hk = hooks[S->fid];
	struct ext *e = hk->ext;

	lua_getglobal(e->lua, hk->fname);
	int err = lua_pcall(e->lua, 0, 0, 0);
	if (err)
		printf("lua function \"%s\" returned with error: %s\n",
		    hk->fname, lua_tostring(e->lua, lua_gettop(e->lua)));

	pthread_mutex_unlock(&e->S->mtx);

	pthread_detach(pthread_self());
	free(tmpS);

	return NULL;
}

static bool
match_cmds(const char *msg, char **cmds)
{
	if (msg == NULL || cmds == NULL || cmds[0] == NULL)
		return false;

	int i, preflen;
	char *prefixes[] = CMD_PREFIX;
	char tail[512];
	if (get_tail(msg, tail, 512) == NULL)
		return false;

	/* make sure the command starts with a valid prefix */
	for (i = 0; prefixes[i] != NULL; i++) {
		preflen = strlen(prefixes[i]);
		if (strncmp(tail, prefixes[i], preflen) == 0)
			break;
	}

	if (prefixes[i] == NULL)
		return false;

	int cmdlen, taillen = strlen(tail);
	char c;
	for (i = 0; cmds[i] != NULL; i++) {
		cmdlen = strlen(cmds[i]);
		if (cmdlen + preflen > taillen)
			continue;

		c = tail[cmdlen + preflen];
		if (strncmp(tail + preflen, cmds[i], cmdlen) == 0 &&
		    (c == ' ' || c == '\0' || c == '\n' || c == '\r'))
			return true;
	}

	return false;
}
