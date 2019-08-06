#define WITH_IRC_INFO

#define NSOCKS		1
#define HTAB_MAX	32768

/* Vinny configuration */
#define CMD_PREFIX	{ "%", ";", "@", NULL }
#define CONFFILE	"vini.conf"
#define BINDIR		"bin"
#define CONFDIR		"etc"
#define EXTDIR		"ext"
#define LIBDIR		"lib"
#define NETDIR		"networks"
#define MODDIR		"mods"
#define HEADDIR		"."

/* pgsql config */
// #define WITH_PG
#define PG_HOST		"192.168.90.255"
#define PG_PORT		"5432"
#define PG_DB		"ircbot"
#define PG_USER		"ircbot"
#define PG_PASS		"canttouchthis"
