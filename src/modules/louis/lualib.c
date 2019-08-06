#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <irc/irc.h>
#include "louis.h"
#include "periodic.h"

#ifdef WITH_IRC_INFO
#include <info/info.h>
#endif

static struct ext *get_ext(lua_State *);
static int irc_parse(lua_State *L, char *(*f)(const char *, char *, int));
static int to_irc(lua_State *, void (*)(int, const char *, const char *));

#ifdef WITH_IRC_INFO
static int get_modes(lua_State *);
#endif

int
l_register_hook(lua_State *L)
{
	if (hookc == hksize) {
		hksize *= 2;
		hooks = realloc(hooks, hksize);
	}

	struct ext *e = get_ext(L);
	struct hook *hk = malloc(sizeof(struct hook));
	if (hk == NULL)
		return 0;

	const char *type = lua_tostring(L, 1);
	const char *fname = lua_tostring(L, 2);
	if (fname == NULL || type == NULL) {
		lua_pushstring(L, "incorrect arguments to reghook");
		lua_error(L);
		return 1;
	}

	int ccount, i;
	for (ccount = 1; lua_tostring(L, ccount + 2) != NULL; ccount++)
		;
	ccount--;

	if (ccount == 0) {
		hk->cmds = NULL;
	} else {
		hk->cmds = malloc((ccount + 1) * sizeof(char *));
		memset(hk->cmds, 0, (ccount + 1) * sizeof(char *));
		for (i = 0; i < ccount; i++)
			hk->cmds[i] = strdup(lua_tostring(L, i + 3));

		hk->cmds[i] = NULL;
	}

	hk->ext = e;
	hk->irccmd = strdup(type);
	hk->fname = strdup(fname);
	hk->fid = hookc++;
	add_irc_hook(type, to_louis, free_hook_fid, hk->fid);
	hooks[hk->fid] = hk;

	return 0;
}

int
l_periodic(lua_State *L)
{
	struct ext *e = get_ext(L);
	const char *fname = lua_tostring(L, 1);
	int period = lua_tointeger(L, 2);
	if (fname == NULL) {
		lua_pushstring(L, "Received a null string in place of a function name in irc.periodic()");
		lua_error(L);
		return 1;
	}

	lua_State *thread = lua_newthread(L);
	lua_gc(thread, LUA_GCSTOP, 0);
	register_periodic(thread, &e->S->mtx, fname, period);
	return 0;
}

int
l_getraw(lua_State *L)
{
	struct ext *e = get_ext(L);
	lua_pushstring(L, e->S->msg);
	return 1;
}

int
l_network(lua_State *L)
{
	struct ext *e = get_ext(L);
	const char *network = get_network_name(e->S->cid);
	lua_pushstring(L, network);
	return 1;
}

int
l_params(lua_State *L)
{
	int i, n = lua_gettop(L);
	if (n < 1) {
		lua_error(L);
		return 0;
	}

	struct ext *e = get_ext(L);
	char result[512];
	char *ret;

	for (i = 1; i <= n; i++) {
		if (lua_isnumber(L, i)) {
			ret = get_param(e->S->msg, result, 512, lua_tonumber(L, i));
			lua_pushstring(L, ret);
		}
	}

	return i - 1;
}

int
l_tail(lua_State *L)
{
	return irc_parse(L, get_tail);
}

int
l_sender(lua_State *L)
{
	return irc_parse(L, get_sender);
}

int
l_command(lua_State *L)
{
	return irc_parse(L, get_command);
}

int
l_msg(lua_State *L)
{
	return to_irc(L, irc_msg);
}

int
l_notice(lua_State *L)
{
	return to_irc(L, irc_notice);
}

int
l_action(lua_State *L)
{
	return to_irc(L, irc_action);
}

int
l_sendraw(lua_State *L)
{
	struct ext *e = get_ext(L);
	int cid;

	const char *rawmsg = lua_tostring(L, 1);
	if (rawmsg == NULL)
		return 0;

	const char *network = lua_tostring(L, 2);
	if (network != NULL) {
		cid = get_cid_from_network_name(network);
		if (cid == -1) {
			lua_pushfstring(L,
			    "Provided network \"%s\" not found", network);
			lua_error(L);
			return 1;
		}
	} else if (e != NULL) {
		cid = e->S->cid;
	} else {
		lua_pushstring(L, "Cannot figure out which connection to send the message to");
		lua_error(L);
		return 1;
	}

	send_irc_message(cid, rawmsg, 0, -1);
	return 0;
}

#ifdef WITH_IRC_INFO
int
l_ismod(lua_State *L)
{
	int modes = get_modes(L);
	lua_pushboolean(L, (modes & 0x7));
	return 1;
}

int
l_modes(lua_State *L)
{
	int modes = get_modes(L);
	lua_pushinteger(L, modes);
	return 1;
}

static int
get_modes(lua_State *L)
{
	struct ext *e = get_ext(L);
	const char *network = get_network_name(e->S->cid);
	const char *channel = lua_tostring(L, 1);
	const char *user = lua_tostring(L, 2);
	struct user_info *uinfo = get_user_info(network, channel, user);
	if (uinfo == NULL)
		return 0x10; /* user */

	return uinfo->modes;
}
#endif

static int
irc_parse(lua_State *L, char *(*f)(const char *, char *, int))
{
	struct ext *e = get_ext(L);
	if (e == NULL) {
		lua_pushnil(L);
		return 1;
	}

	char result[512];
	char *ret = f(e->S->msg, result, 512);
	lua_pushstring(L, ret);

	return 1;
}


static int
to_irc(lua_State *L, void (*f)(int, const char *, const char *))
{
	struct ext *e = get_ext(L);
	int cid;
	const char *target = lua_tostring(L, 1);
	const char *message = lua_tostring(L, 2);
	if (message == NULL || target == NULL) {
		lua_pushstring(L, "Incorrect arguments");
		lua_error(L);
		return 1;
	}

	const char *network = lua_tostring(L, 3);
	if (network != NULL) {
		cid = get_cid_from_network_name(network);
		if (cid == -1) {
			lua_pushfstring(L,
			    "Provided network \"%s\" not found", network);
			lua_error(L);
			return 1;
		}
	} else if (e != NULL) {
		cid = e->S->cid;
	} else {
		lua_pushstring(L, "Cannot figure out which connection to send the message to");
		lua_error(L);
		return 1;
	}

	f(cid, message, target);
	return 0;
}

static struct ext *
get_ext(lua_State *L)
{
	int i;
	for (i = 0; i < extc; i++) {
		if (exts[i]->lua == L)
			return exts[i];
	}

	return NULL;
}
