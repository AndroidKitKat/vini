/* default IRC configuration */
#define BOTNAME			"vini"
#define BOTREALNAME		"Vini Pu"
#define IRC_NETWORK_NAME	"Snoonet"
#define IRC_SERVER		"irc.snoonet.org"
#define IRC_PORT		"6697"
#define IRC_PASSWORD		NULL
//#define IRC_CHANNELS		"#bsdmasterrace,#linux_master_bait,#0,##uno"
#define IRC_CHANNELS		"#bsdmasterrace"
#define IRC_USE_SSL		1

#define NS_LOGIN		1
#define NS_USERNAME		"vini"
#define NS_PASSWORD		"honeybear"

char *irc_suffixlist[] = { "", "i", "ii", "iii", "iiii", NULL };

struct irc_info ircinfo[] = {
	/* Default connection */
	{ BOTNAME, BOTREALNAME, IRC_NETWORK_NAME, IRC_SERVER, IRC_PORT,
	  IRC_PASSWORD, IRC_CHANNELS, IRC_USE_SSL,
	  NS_LOGIN, NS_USERNAME, NS_PASSWORD },
//	{ "vini", "Vini Pu", "Rizon", "irc.rizon.net", "6697", NULL,
//	  "#geb", 1, 1, "vini", "honeybear" },
	{NULL}
};
