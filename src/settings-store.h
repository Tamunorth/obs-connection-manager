#pragma once

#include "cm-types.h"

#ifdef __cplusplus
extern "C" {
#endif

void cm_settings_init_defaults(struct cm_settings *out);
bool cm_settings_load(struct cm_settings *out);
bool cm_settings_save(const struct cm_settings *in);

const struct cm_settings *cm_settings_current(void);
void cm_settings_set_current(const struct cm_settings *in);

#ifdef __cplusplus
}
#endif
