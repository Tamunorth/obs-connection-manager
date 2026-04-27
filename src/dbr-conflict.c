#include "dbr-conflict.h"
#include "cm-types.h"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

/*
 * OBS's built-in "Dynamically change bitrate to manage congestion (Beta)"
 * is stored under the user / global config under section "Output", key
 * "DynamicBitrate" (boolean). The exact key name has shifted between OBS
 * versions; we read the current value, save it, and write false. On stream
 * end we restore.
 *
 * We avoid hard-failing if the key cannot be found.
 */

#define DBR_SECTION "Output"
#define DBR_KEY "DynamicBitrate"

static bool prior_value = false;
static bool prior_set = false;

static config_t *get_user_config(void)
{
#if OBS_VERSION_AT_LEAST_31_0
	return obs_frontend_get_user_config();
#else
	return obs_frontend_get_global_config();
#endif
}

void cm_dbr_disable_obs_dbr(void)
{
	config_t *cfg = NULL;

	/* obs_frontend_get_user_config exists from OBS 31+; fall back to global */
	cfg = obs_frontend_get_profile_config();
	if (!cfg) {
		blog(LOG_WARNING,
		     CM_LOG_PREFIX "dbr: no profile config available");
		return;
	}

	if (config_has_user_value(cfg, DBR_SECTION, DBR_KEY)) {
		prior_value = config_get_bool(cfg, DBR_SECTION, DBR_KEY);
		prior_set = true;
	} else {
		prior_value = false;
		prior_set = false;
	}

	config_set_bool(cfg, DBR_SECTION, DBR_KEY, false);
	config_save_safe(cfg, "tmp", NULL);
	blog(LOG_INFO,
	     CM_LOG_PREFIX "dbr: built-in DBR disabled (prior=%s prior_set=%d)",
	     prior_value ? "true" : "false", prior_set ? 1 : 0);
}

void cm_dbr_restore_obs_dbr(void)
{
	config_t *cfg = obs_frontend_get_profile_config();
	if (!cfg) {
		blog(LOG_WARNING,
		     CM_LOG_PREFIX "dbr-restore: no profile config available");
		return;
	}

	if (prior_set) {
		config_set_bool(cfg, DBR_SECTION, DBR_KEY, prior_value);
	} else {
		config_remove_value(cfg, DBR_SECTION, DBR_KEY);
	}
	config_save_safe(cfg, "tmp", NULL);
	blog(LOG_INFO,
	     CM_LOG_PREFIX
	     "dbr: built-in DBR restored (prior=%s prior_set=%d)",
	     prior_value ? "true" : "false", prior_set ? 1 : 0);

	prior_value = false;
	prior_set = false;
}
