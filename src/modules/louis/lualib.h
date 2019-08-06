#include <config.h>

int l_register_hook(lua_State *L);
int l_periodic(lua_State *L);
int l_getraw(lua_State *L);
int l_network(lua_State *L);
int l_params(lua_State *L);
int l_tail(lua_State *L);
int l_command(lua_State *L);
int l_sender(lua_State *L);
int l_msg(lua_State *L);
int l_notice(lua_State *L);
int l_action(lua_State *L);
int l_sendraw(lua_State *L);

#ifdef WITH_IRC_INFO
int l_ismod(lua_State *L);
int l_modes(lua_State *L);
#endif

const struct luaL_Reg l_irclib[] = {
	{"reghook", l_register_hook},
	{"periodic", l_periodic},
	{"getraw", l_getraw},
	{"network", l_network},
	{"params", l_params},
	{"tail", l_tail},
	{"command", l_command},
	{"sender", l_sender},
	{"msg", l_msg},
	{"notice", l_notice},
	{"action", l_action},
	{"sendraw", l_sendraw},

#ifdef WITH_IRC_INFO
	{"ismod", l_ismod},
	{"modes", l_modes},
#endif

	{NULL, NULL}
};

