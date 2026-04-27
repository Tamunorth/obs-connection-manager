#include "notification-bridge.h"
#include <util/threading.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LISTENERS 8

struct state_listener_entry {
	cm_state_listener_fn fn;
	void *userdata;
};

struct breaking_listener_entry {
	cm_breaking_listener_fn fn;
	void *userdata;
};

static struct state_listener_entry state_listeners[MAX_LISTENERS];
static int state_listener_count = 0;

static struct breaking_listener_entry breaking_listeners[MAX_LISTENERS];
static int breaking_listener_count = 0;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void cm_bridge_register_state_listener(cm_state_listener_fn fn, void *userdata)
{
	pthread_mutex_lock(&lock);
	if (state_listener_count < MAX_LISTENERS) {
		state_listeners[state_listener_count].fn = fn;
		state_listeners[state_listener_count].userdata = userdata;
		state_listener_count++;
	}
	pthread_mutex_unlock(&lock);
}

void cm_bridge_register_breaking_listener(cm_breaking_listener_fn fn,
					  void *userdata)
{
	pthread_mutex_lock(&lock);
	if (breaking_listener_count < MAX_LISTENERS) {
		breaking_listeners[breaking_listener_count].fn = fn;
		breaking_listeners[breaking_listener_count].userdata = userdata;
		breaking_listener_count++;
	}
	pthread_mutex_unlock(&lock);
}

void cm_bridge_clear_state_listeners(void)
{
	pthread_mutex_lock(&lock);
	state_listener_count = 0;
	pthread_mutex_unlock(&lock);
}

void cm_bridge_clear_breaking_listeners(void)
{
	pthread_mutex_lock(&lock);
	breaking_listener_count = 0;
	pthread_mutex_unlock(&lock);
}

void cm_bridge_emit_state(const struct cm_runtime_state *state)
{
	struct state_listener_entry copy[MAX_LISTENERS];
	int n;
	pthread_mutex_lock(&lock);
	n = state_listener_count;
	memcpy(copy, state_listeners, sizeof(state_listeners[0]) * n);
	pthread_mutex_unlock(&lock);

	for (int i = 0; i < n; i++)
		copy[i].fn(state, copy[i].userdata);
}

void cm_bridge_emit_breaking(bool entering, const struct cm_runtime_state *state)
{
	struct breaking_listener_entry copy[MAX_LISTENERS];
	int n;
	pthread_mutex_lock(&lock);
	n = breaking_listener_count;
	memcpy(copy, breaking_listeners, sizeof(breaking_listeners[0]) * n);
	pthread_mutex_unlock(&lock);

	for (int i = 0; i < n; i++)
		copy[i].fn(entering, state, copy[i].userdata);
}
