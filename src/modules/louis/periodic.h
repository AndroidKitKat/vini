void register_periodic(lua_State *, pthread_mutex_t *, const char *, int);
void *periodic_main(void *);
void destroy_periodics(void);
