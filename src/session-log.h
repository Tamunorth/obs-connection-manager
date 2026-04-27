#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void cm_log_open(void);
void cm_log_close(void);
void cm_log_event(const char *fmt, ...);

#ifdef __cplusplus
}
#endif
