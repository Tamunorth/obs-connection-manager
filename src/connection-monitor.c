#include "connection-monitor.h"
#include "bitrate-controller.h"
#include "dbr-conflict.h"
#include "session-log.h"
#include "settings-store.h"
#include "notification-bridge.h"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/threading.h>
#include <util/platform.h>
#include <string.h>
#include <math.h>

static pthread_t monitor_thread;
static pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile bool thread_running = false;
static volatile bool stop_requested = false;
static volatile bool selftest_active = false;
static int selftest_step = 0;
static const float selftest_curve[] = {
	0.0f, 0.0f, 0.40f, 0.70f,
	0.95f, 0.95f, 0.95f, 0.95f, 0.95f, 0.95f, 0.95f,
	0.20f, 0.05f, 0.05f, 0.05f, 0.05f, 0.0f
};
static const int selftest_steps =
	(int)(sizeof(selftest_curve) / sizeof(selftest_curve[0]));

static struct cm_runtime_state runtime = {0};
static int64_t streaming_start_ms = 0;
static int64_t last_step_down_ms = 0;
static int64_t last_step_up_ms = 0;
static int64_t healthy_since_ms = 0;
static int64_t breaking_since_ms = 0;
static bool breaking_emitted = false;
static volatile bool test_override_active = false;
static volatile float test_override_value = 0.0f;

const char *cm_state_name(enum cm_state s)
{
	switch (s) {
	case CM_IDLE: return "IDLE";
	case CM_HEALTHY: return "HEALTHY";
	case CM_DEGRADING: return "DEGRADING";
	case CM_AT_FLOOR: return "AT_FLOOR";
	case CM_BREAKING: return "BREAKING";
	}
	return "?";
}

static int64_t now_ms(void) { return os_gettime_ns() / 1000000; }

static void set_state_locked(enum cm_state new_state)
{
	if (runtime.state == new_state)
		return;
	enum cm_state old = runtime.state;
	runtime.state = new_state;
	runtime.last_state_change_ms = now_ms();
	blog(LOG_INFO, CM_LOG_PREFIX "state %s -> %s kbps=%d cong=%.2f",
	     cm_state_name(old), cm_state_name(new_state),
	     runtime.current_kbps, runtime.congestion);
	cm_log_event("state %s -> %s kbps=%d cong=%.2f",
		     cm_state_name(old), cm_state_name(new_state),
		     runtime.current_kbps, runtime.congestion);
}

static void emit_state_unlocked(void)
{
	struct cm_runtime_state copy;
	pthread_mutex_lock(&state_lock);
	copy = runtime;
	pthread_mutex_unlock(&state_lock);
	cm_bridge_emit_state(&copy);
}

static void emit_breaking_unlocked(bool entering)
{
	struct cm_runtime_state copy;
	pthread_mutex_lock(&state_lock);
	copy = runtime;
	pthread_mutex_unlock(&state_lock);
	cm_bridge_emit_breaking(entering, &copy);
}

static int compute_floor(int target, int lower)
{
	int floor = (int)(target * 0.5f);
	if (floor < lower)
		floor = lower;
	if (floor < 100)
		floor = 100;
	return floor;
}

static void tick(void)
{
	const struct cm_settings *s = cm_settings_current();
	int64_t t = now_ms();

	float congestion;
	if (test_override_active) {
		congestion = test_override_value;
	} else {
		congestion = cm_bitrate_read_congestion();
	}

	pthread_mutex_lock(&state_lock);

	runtime.congestion = congestion;
	runtime.frames_dropped = cm_bitrate_read_dropped();
	runtime.frames_total = cm_bitrate_read_total_frames();
	if (!selftest_active) {
		/* Live ceiling = settings.upper_kbps. The dock writes here. */
		runtime.target_kbps = s->upper_kbps;
		runtime.current_kbps = cm_bitrate_get_current_kbps();
		if (runtime.current_kbps == 0)
			runtime.current_kbps = cm_bitrate_get_target_kbps();
	}
	runtime.encoder_supports_dyn = cm_bitrate_supports_dyn();
	if (runtime.target_kbps > 0)
		runtime.floor_kbps = compute_floor(runtime.target_kbps,
						   s->lower_kbps);

	bool above_threshold = congestion >= s->congestion_threshold;
	bool above_breaking =
		congestion >= s->breaking_threshold;
	bool at_floor = runtime.current_kbps <= runtime.floor_kbps;

	enum cm_state new_state = runtime.state;

	if (runtime.state == CM_IDLE) {
		new_state = CM_HEALTHY;
		healthy_since_ms = t;
	} else if (above_breaking && at_floor) {
		if (breaking_since_ms == 0)
			breaking_since_ms = t;
		if (t - breaking_since_ms >= s->breaking_hold_ms)
			new_state = CM_BREAKING;
		else
			new_state = CM_AT_FLOOR;
	} else if (at_floor && above_threshold) {
		new_state = CM_AT_FLOOR;
		breaking_since_ms = 0;
	} else if (above_threshold) {
		new_state = CM_DEGRADING;
		breaking_since_ms = 0;
	} else {
		new_state = CM_HEALTHY;
		breaking_since_ms = 0;
	}

	bool was_breaking = (runtime.state == CM_BREAKING);
	bool is_breaking = (new_state == CM_BREAKING);

	set_state_locked(new_state);

	int decision_kbps = runtime.current_kbps;
	const char *decision = "hold";

	if (s->enabled && runtime.target_kbps > 0) {
		if ((new_state == CM_DEGRADING || new_state == CM_AT_FLOOR ||
		     new_state == CM_BREAKING) &&
		    runtime.current_kbps > runtime.floor_kbps &&
		    (t - last_step_down_ms) >= s->step_down_interval_ms) {
			decision_kbps = runtime.current_kbps - s->step_size_kbps;
			if (decision_kbps < runtime.floor_kbps)
				decision_kbps = runtime.floor_kbps;
			decision = "step-down";
			last_step_down_ms = t;
		} else if (new_state == CM_HEALTHY &&
			   runtime.current_kbps < runtime.target_kbps) {
			if (healthy_since_ms == 0)
				healthy_since_ms = t;
			if ((t - healthy_since_ms) >= s->recovery_window_ms &&
			    (t - last_step_up_ms) >= s->step_up_interval_ms) {
				decision_kbps =
					runtime.current_kbps + s->step_size_kbps;
				if (decision_kbps > runtime.target_kbps)
					decision_kbps = runtime.target_kbps;
				decision = "step-up";
				last_step_up_ms = t;
			}
		} else {
			healthy_since_ms = (new_state == CM_HEALTHY)
						   ? healthy_since_ms
						   : 0;
		}
	}

	pthread_mutex_unlock(&state_lock);

	if (strcmp(decision, "hold") != 0) {
		blog(LOG_INFO,
		     CM_LOG_PREFIX
		     "decision %s kbps=%d->%d cong=%.2f state=%s",
		     decision, runtime.current_kbps, decision_kbps,
		     congestion, cm_state_name(new_state));
		cm_log_event("decision %s kbps=%d->%d cong=%.2f state=%s",
			     decision, runtime.current_kbps, decision_kbps,
			     congestion, cm_state_name(new_state));

		if (!selftest_active)
			cm_bitrate_set_kbps(decision_kbps);
		else
			runtime.current_kbps = decision_kbps;
	}

	emit_state_unlocked();

	if (is_breaking && !breaking_emitted) {
		breaking_emitted = true;
		blog(LOG_INFO, CM_LOG_PREFIX "breaking-enter kbps=%d",
		     runtime.current_kbps);
		emit_breaking_unlocked(true);
	} else if (!is_breaking && breaking_emitted) {
		breaking_emitted = false;
		blog(LOG_INFO, CM_LOG_PREFIX "breaking-exit kbps=%d",
		     runtime.current_kbps);
		emit_breaking_unlocked(false);
	}
	(void)was_breaking;
}

static void *monitor_thread_fn(void *arg)
{
	(void)arg;
	const struct cm_settings *s = cm_settings_current();
	while (!stop_requested) {
		int interval = s->poll_interval_ms;
		if (interval < 50)
			interval = 50;
		if (interval > 5000)
			interval = 5000;
		tick();
		os_sleep_ms(interval);
		s = cm_settings_current();
	}
	thread_running = false;
	return NULL;
}

static void start_thread_if_needed(void)
{
	if (thread_running)
		return;
	stop_requested = false;
	thread_running = true;
	pthread_create(&monitor_thread, NULL, monitor_thread_fn, NULL);
}

static void stop_thread(void)
{
	if (!thread_running)
		return;
	stop_requested = true;
	pthread_join(monitor_thread, NULL);
	thread_running = false;
}

void cm_monitor_init(void)
{
	memset(&runtime, 0, sizeof(runtime));
	runtime.state = CM_IDLE;
	blog(LOG_INFO, CM_LOG_PREFIX "monitor init");
}

void cm_monitor_shutdown(void)
{
	stop_thread();
	memset(&runtime, 0, sizeof(runtime));
	blog(LOG_INFO, CM_LOG_PREFIX "monitor shutdown");
}

void cm_monitor_on_streaming_started(void)
{
	streaming_start_ms = now_ms();
	last_step_down_ms = 0;
	last_step_up_ms = 0;
	healthy_since_ms = streaming_start_ms;
	breaking_since_ms = 0;
	breaking_emitted = false;

	obs_output_t *output = obs_frontend_get_streaming_output();
	if (output) {
		cm_bitrate_attach(output);
		obs_output_release(output);
	} else {
		blog(LOG_WARNING,
		     CM_LOG_PREFIX
		     "streaming-started but no output found");
	}

	const struct cm_settings *s = cm_settings_current();
	if (s->auto_disable_obs_dbr)
		cm_dbr_disable_obs_dbr();

	cm_log_open();
	cm_log_event("streaming-started target=%d caps_dyn=%d",
		     cm_bitrate_get_target_kbps(),
		     cm_bitrate_supports_dyn() ? 1 : 0);

	pthread_mutex_lock(&state_lock);
	runtime.streaming = true;
	runtime.state = CM_HEALTHY;
	pthread_mutex_unlock(&state_lock);

	start_thread_if_needed();
	blog(LOG_INFO, CM_LOG_PREFIX "frontend-event STREAMING_STARTED");
}

void cm_monitor_on_streaming_stopping(void)
{
	blog(LOG_INFO, CM_LOG_PREFIX "frontend-event STREAMING_STOPPING");
	stop_thread();

	cm_bitrate_restore_target();
	cm_bitrate_detach();

	const struct cm_settings *s = cm_settings_current();
	if (s->auto_disable_obs_dbr)
		cm_dbr_restore_obs_dbr();

	cm_log_event("streaming-stopped");
	cm_log_close();

	pthread_mutex_lock(&state_lock);
	runtime.streaming = false;
	runtime.state = CM_IDLE;
	pthread_mutex_unlock(&state_lock);
}

void cm_monitor_get_state(struct cm_runtime_state *out)
{
	pthread_mutex_lock(&state_lock);
	*out = runtime;
	pthread_mutex_unlock(&state_lock);
}

void cm_monitor_set_test_congestion(float value)
{
	test_override_value = value;
	test_override_active = true;
	blog(LOG_INFO, CM_LOG_PREFIX "test-override congestion=%.2f", value);
}

void cm_monitor_clear_test_congestion(void)
{
	test_override_active = false;
	blog(LOG_INFO, CM_LOG_PREFIX "test-override cleared");
}

static void *selftest_thread_fn(void *arg)
{
	(void)arg;
	blog(LOG_INFO, CM_LOG_PREFIX "selftest START");
	cm_log_open();
	cm_log_event("selftest started");

	pthread_mutex_lock(&state_lock);
	runtime.streaming = true;
	runtime.target_kbps = 6000;
	runtime.current_kbps = 6000;
	runtime.floor_kbps = compute_floor(6000, cm_settings_current()->lower_kbps);
	runtime.state = CM_HEALTHY;
	runtime.encoder_supports_dyn = true;
	pthread_mutex_unlock(&state_lock);

	selftest_active = true;
	start_thread_if_needed();

	for (int i = 0; i < selftest_steps && !stop_requested; i++) {
		selftest_step = i;
		float v = selftest_curve[i];
		cm_monitor_set_test_congestion(v);
		os_sleep_ms(1500);
	}

	cm_monitor_clear_test_congestion();
	os_sleep_ms(500);

	stop_thread();
	selftest_active = false;

	pthread_mutex_lock(&state_lock);
	runtime.streaming = false;
	runtime.state = CM_IDLE;
	runtime.current_kbps = 0;
	runtime.target_kbps = 0;
	pthread_mutex_unlock(&state_lock);

	cm_log_event("selftest finished");
	cm_log_close();
	blog(LOG_INFO, CM_LOG_PREFIX "selftest END");
	return NULL;
}

void cm_monitor_run_selftest(void)
{
	pthread_t t;
	pthread_create(&t, NULL, selftest_thread_fn, NULL);
	pthread_detach(t);
}
