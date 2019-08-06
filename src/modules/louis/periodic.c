#include <sys/queue.h>
#include <sys/time.h>

#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "louis.h"
#include "periodic.h"

struct periodic {
	lua_State *lua;
	char *fname;
	time_t deadline;
	int period;
	pthread_mutex_t *mtx;
	LIST_ENTRY(periodic) entry;
};

LIST_HEAD(phead, periodic) head = LIST_HEAD_INITIALIZER(head);

void insert_periodic(struct periodic *);
void run_periodic(struct periodic *);
void free_periodic(struct periodic *);
void *call_periodic(void *);

static pthread_t periodic_thread;

void *
periodic_main(void *nullp)
{
	struct timeval tv;
	struct periodic *tp;

	time_t towait;

	periodic_thread = pthread_self();

	for (;;) {
		if (LIST_EMPTY(&head)) {
			sleep(10);
			continue;
		}

		gettimeofday(&tv, NULL);
		LIST_FOREACH(tp, &head, entry) {
			if (tp != NULL && tp->deadline <= tv.tv_sec)
				run_periodic(tp);
			else
				break;
		}

		if (LIST_EMPTY(&head))
			continue;

		tp = LIST_FIRST(&head);
		gettimeofday(&tv, NULL);
		towait = tp->deadline - tv.tv_sec;

		if (towait > 0)
			sleep(towait);
		else
			continue;
	}

periodic_main_exit:
	pthread_detach(pthread_self());
	return NULL;
}

void
register_periodic(lua_State *L, pthread_mutex_t *mtx,
	    const char *fname, int period)
{
	if (fname == NULL || period <= 0)
		return;

	struct periodic *p = malloc(sizeof(struct periodic));
	struct timeval tv;

	if (p == NULL)
		return;

	gettimeofday(&tv, NULL);
	p->lua = L;
	p->fname = strdup(fname);
	p->deadline = tv.tv_sec + period;
	p->period = period;
	p->mtx = mtx;
	insert_periodic(p);
	return;
}

void
insert_periodic(struct periodic *p)
{
	if (p == NULL)
		return;

	if (LIST_EMPTY(&head)) {
		LIST_INSERT_HEAD(&head, p, entry);
		return;
	}

	struct periodic *tp;
	LIST_FOREACH(tp, &head, entry) {
		if (p->deadline < tp->deadline) {
			LIST_INSERT_BEFORE(tp, p, entry);
			break;
		}

		if (LIST_NEXT(tp, entry) == NULL) {
			LIST_INSERT_AFTER(tp, p, entry);
			break;
		}
	}

	return;
}

void
run_periodic(struct periodic *p)
{
	LIST_REMOVE(p, entry);
	pthread_t thread;
	pthread_create(&thread, NULL, call_periodic, p);
	p->deadline += p->period;
	insert_periodic(p);
	return;
}

void
destroy_periodics()
{
	struct periodic *p, *tp;
	pthread_cancel(periodic_thread);
	pthread_detach(periodic_thread);

	LIST_FOREACH_SAFE(p, &head, entry, tp)
		free_periodic(p);
}

void
free_periodic(struct periodic *p)
{
	pthread_mutex_lock(p->mtx);
	lua_close(p->lua);
	free(p->fname);
	pthread_mutex_destroy(p->mtx);
	free(p);
}

void *
call_periodic(void *vp)
{
	if (vp == NULL)
		goto exit_call_periodic;

	struct periodic *p = (struct periodic *)vp;
	pthread_mutex_lock(p->mtx);
	lua_getglobal(p->lua, p->fname);
	if (!lua_isnil(p->lua, lua_gettop(p->lua)))
		lua_call(p->lua, 0, 0);
	pthread_mutex_unlock(p->mtx);

exit_call_periodic:
	pthread_detach(pthread_self());
	return NULL;
}
