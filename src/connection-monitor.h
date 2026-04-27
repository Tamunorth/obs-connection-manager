#pragma once

#include "cm-types.h"

#ifdef __cplusplus
extern "C" {
#endif

void cm_monitor_init(void);
void cm_monitor_shutdown(void);

void cm_monitor_on_streaming_started(void);
void cm_monitor_on_streaming_stopping(void);

void cm_monitor_get_state(struct cm_runtime_state *out);

void cm_monitor_set_test_congestion(float value);
void cm_monitor_clear_test_congestion(void);

void cm_monitor_run_selftest(void);

const char *cm_state_name_impl(enum cm_state s);

#ifdef __cplusplus
}
#endif
