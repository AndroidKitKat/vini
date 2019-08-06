#include <sys/queue.h>

/* modes */
#define IRC_OP		0x01
#define CHAN_OP		0x02
#define CHAN_HOP	0x04
#define CHAN_VOP	0x08

struct user_info {
	char *network;
	char *channel;
	char *nick;
	char *user;
	char *host;
	char *realname;
	int modes;
	int away;
	int oper;
};

struct channel_info {
	char *network;
	char *channel;
	char *topic;
	char *modes;
};

struct user_entry {
	struct user_info *uinfo;
	SLIST_ENTRY(user_entry) next;
};

struct channel_entry {
	struct channel_info *cinfo;
	struct user_list *users;
	SLIST_ENTRY(channel_entry) next;
};

SLIST_HEAD(user_list, user_entry);
SLIST_HEAD(channel_list, channel_entry);

struct channel_list *get_channel_list(int cid);
struct user_list *get_user_list(const char *channel, int cid);

int setup_info(void);
void free_user_info(struct user_info *);
void free_channel_info(struct channel_info *);

struct user_info *get_user_info(const char *, const char *, const char *);
struct channel_info *get_channel_info(const char *, int);
int add_user_info(struct user_info *, int, int);
int add_channel_info(struct channel_info *, int);
struct user_info *make_user_info(const char *, const char *, const char *);
struct channel_info *make_channel_info(const char *, const char *);
void rm_user_info(const char *, const char *, const char *, int, int);
void rm_channel_info(const char *, const char *, int);
