/* tools for the handlers */
void set_errstr(char *errstr, char *string);
int index_to_arg(char *op, int argn, char *errstr);
char *get_next_word(char *op, int *index, char *errstr);
int split_maker_parts(char *op, char **headp, char **tailp,
    char **channelsp, char *errstr);
char **delistify(char *s, char *errstr);
char **parse_argv(char *name, char *s, char *errstr);

/* cmd_handlers for the parser */
int parse_reply(struct parser_info *);
int parse_action(struct parser_info *);
int parse_join(struct parser_info *);
int parse_part(struct parser_info *);
int parse_send(struct parser_info *);
int parse_notice(struct parser_info *);
int parse_actto(struct parser_info *);
int parse_raw(struct parser_info *);
int parse_getlog(struct parser_info *);
int parse_def(struct parser_info *);
int parse_hook(struct parser_info *);
int parse_match(struct parser_info *);
