#include <sys/types.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <pthread.h>
#include <irc/irc.h>

struct state {
	char *msg;
	pthread_mutex_t mtx;
	int cid;
	int sn;
	int fid;
};

struct ext {
	lua_State *lua;
	struct state *S;
};

struct hook {
	struct ext *ext;
	char *irccmd;
	char *fname;
	int fid;
	char **cmds;
};

uint64_t extsize;
uint64_t hksize;
struct ext **exts;
struct hook **hooks;
uint64_t extc;
uint64_t hookc;

void to_louis(const struct hook_msg *);
void free_hook_fid(int);
int init_louis();
