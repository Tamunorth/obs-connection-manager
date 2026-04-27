#pragma once

#include <obs.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool cm_bitrate_attach(obs_output_t *output);
void cm_bitrate_detach(void);

bool cm_bitrate_supports_dyn(void);
int cm_bitrate_get_target_kbps(void);
int cm_bitrate_get_current_kbps(void);

bool cm_bitrate_set_kbps(int kbps);
void cm_bitrate_restore_target(void);

float cm_bitrate_read_congestion(void);
int cm_bitrate_read_dropped(void);
int cm_bitrate_read_total_frames(void);
int cm_bitrate_read_connect_time_ms(void);

#ifdef __cplusplus
}
#endif
