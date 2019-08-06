#include <sys/types.h>

#include <stdlib.h>
#include <lua52/lua.h>
#include <lua52/lualib.h>
#include <lua52/lauxlib.h>

static int l_random(lua_State *);

static const struct luaL_Reg osrandom[] = {
	{"random", l_random},
	{NULL, NULL}
};

static int
l_random(lua_State *L)
{
	srandomdev();
	long n = random();
	lua_pushinteger(L, n);
	return 1;
}

int
luaopen_osrandom(lua_State *L)
{
	luaL_newlib(L, osrandom);
	return 1;
}
