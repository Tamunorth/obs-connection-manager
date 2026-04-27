#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define CM_LOG_PREFIX "[cm] "

enum cm_state {
	CM_IDLE = 0,
	CM_HEALTHY,
	CM_DEGRADING,
	CM_AT_FLOOR,
	CM_BREAKING,
};

struct cm_settings {
	bool enabled;
	int upper_kbps;
	int lower_kbps;
	int step_size_kbps;
	int poll_interval_ms;
	int step_down_interval_ms;
	int step_up_interval_ms;
	int recovery_window_ms;
	int breaking_hold_ms;
	float breaking_threshold;
	float congestion_threshold;
	bool notify_banner;
	bool notify_toast;
	bool auto_disable_obs_dbr;
	bool selftest_on_load;
};

struct cm_runtime_state {
	int target_kbps;
	int current_kbps;
	int floor_kbps;
	float congestion;
	int frames_dropped;
	int frames_total;
	enum cm_state state;
	bool encoder_supports_dyn;
	bool streaming;
	int64_t last_state_change_ms;
};

const char *cm_state_name(enum cm_state s);

#ifdef __cplusplus
}
#endif
