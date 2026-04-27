#pragma once

#include "cm-types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*cm_state_listener_fn)(const struct cm_runtime_state *state,
				     void *userdata);
typedef void (*cm_breaking_listener_fn)(bool entering, const struct cm_runtime_state *state,
					void *userdata);

void cm_bridge_register_state_listener(cm_state_listener_fn fn, void *userdata);
void cm_bridge_register_breaking_listener(cm_breaking_listener_fn fn, void *userdata);

void cm_bridge_clear_state_listeners(void);
void cm_bridge_clear_breaking_listeners(void);

void cm_bridge_emit_state(const struct cm_runtime_state *state);
void cm_bridge_emit_breaking(bool entering, const struct cm_runtime_state *state);

#ifdef __cplusplus
}
#endif
