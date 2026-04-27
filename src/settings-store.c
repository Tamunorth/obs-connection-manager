#include "settings-store.h"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <stdio.h>
#include <string.h>

static struct cm_settings current = {0};
static char config_path[1024] = {0};

void cm_settings_init_defaults(struct cm_settings *out)
{
	out->enabled = true;
	out->upper_kbps = 6000;
	out->lower_kbps = 1500;
	out->step_size_kbps = 1000;
	out->poll_interval_ms = 300;
	out->step_down_interval_ms = 1000;
	out->step_up_interval_ms = 5000;
	out->recovery_window_ms = 5000;
	out->breaking_hold_ms = 5000;
	out->breaking_threshold = 0.95f;
	out->congestion_threshold = 0.50f;
	out->notify_banner = true;
	out->notify_toast = true;
	out->auto_disable_obs_dbr = true;
	out->selftest_on_load = true;
}

static const char *get_config_path(void)
{
	if (config_path[0])
		return config_path;

	char *base = obs_module_config_path("");
	if (!base) {
		blog(LOG_WARNING, CM_LOG_PREFIX
		     "obs_module_config_path returned NULL, using cwd");
		snprintf(config_path, sizeof(config_path), "cm-config.json");
		return config_path;
	}

	os_mkdirs(base);
	snprintf(config_path, sizeof(config_path), "%sconfig.json", base);
	bfree(base);
	return config_path;
}

bool cm_settings_load(struct cm_settings *out)
{
	cm_settings_init_defaults(out);

	const char *path = get_config_path();
	obs_data_t *data = obs_data_create_from_json_file(path);
	if (!data) {
		blog(LOG_INFO, CM_LOG_PREFIX
		     "settings file not found at %s, using defaults", path);
		return false;
	}

	out->enabled = obs_data_get_bool(data, "enabled");
	out->upper_kbps = (int)obs_data_get_int(data, "upper_kbps");
	out->lower_kbps = (int)obs_data_get_int(data, "lower_kbps");
	out->step_size_kbps = (int)obs_data_get_int(data, "step_size_kbps");
	out->poll_interval_ms =
		(int)obs_data_get_int(data, "poll_interval_ms");
	out->step_down_interval_ms =
		(int)obs_data_get_int(data, "step_down_interval_ms");
	out->step_up_interval_ms =
		(int)obs_data_get_int(data, "step_up_interval_ms");
	out->recovery_window_ms =
		(int)obs_data_get_int(data, "recovery_window_ms");
	out->breaking_hold_ms =
		(int)obs_data_get_int(data, "breaking_hold_ms");
	out->breaking_threshold =
		(float)obs_data_get_double(data, "breaking_threshold");
	out->congestion_threshold =
		(float)obs_data_get_double(data, "congestion_threshold");
	out->notify_banner = obs_data_get_bool(data, "notify_banner");
	out->notify_toast = obs_data_get_bool(data, "notify_toast");
	out->auto_disable_obs_dbr =
		obs_data_get_bool(data, "auto_disable_obs_dbr");
	out->selftest_on_load = obs_data_get_bool(data, "selftest_on_load");

	/* Defensive: if zero values were loaded for required fields, restore defaults. */
	struct cm_settings def;
	cm_settings_init_defaults(&def);
	if (out->upper_kbps <= 0)
		out->upper_kbps = def.upper_kbps;
	if (out->lower_kbps <= 0)
		out->lower_kbps = def.lower_kbps;
	if (out->step_size_kbps <= 0)
		out->step_size_kbps = def.step_size_kbps;
	if (out->poll_interval_ms <= 0)
		out->poll_interval_ms = def.poll_interval_ms;
	if (out->step_down_interval_ms <= 0)
		out->step_down_interval_ms = def.step_down_interval_ms;
	if (out->step_up_interval_ms <= 0)
		out->step_up_interval_ms = def.step_up_interval_ms;
	if (out->recovery_window_ms <= 0)
		out->recovery_window_ms = def.recovery_window_ms;
	if (out->breaking_hold_ms <= 0)
		out->breaking_hold_ms = def.breaking_hold_ms;
	if (out->breaking_threshold <= 0.0f)
		out->breaking_threshold = def.breaking_threshold;
	if (out->congestion_threshold <= 0.0f)
		out->congestion_threshold = def.congestion_threshold;

	obs_data_release(data);
	blog(LOG_INFO, CM_LOG_PREFIX "settings loaded keys=15 path=%s", path);
	return true;
}

bool cm_settings_save(const struct cm_settings *in)
{
	const char *path = get_config_path();

	obs_data_t *data = obs_data_create();
	obs_data_set_bool(data, "enabled", in->enabled);
	obs_data_set_int(data, "upper_kbps", in->upper_kbps);
	obs_data_set_int(data, "lower_kbps", in->lower_kbps);
	obs_data_set_int(data, "step_size_kbps", in->step_size_kbps);
	obs_data_set_int(data, "poll_interval_ms", in->poll_interval_ms);
	obs_data_set_int(data, "step_down_interval_ms",
			 in->step_down_interval_ms);
	obs_data_set_int(data, "step_up_interval_ms", in->step_up_interval_ms);
	obs_data_set_int(data, "recovery_window_ms", in->recovery_window_ms);
	obs_data_set_int(data, "breaking_hold_ms", in->breaking_hold_ms);
	obs_data_set_double(data, "breaking_threshold", in->breaking_threshold);
	obs_data_set_double(data, "congestion_threshold",
			    in->congestion_threshold);
	obs_data_set_bool(data, "notify_banner", in->notify_banner);
	obs_data_set_bool(data, "notify_toast", in->notify_toast);
	obs_data_set_bool(data, "auto_disable_obs_dbr",
			  in->auto_disable_obs_dbr);
	obs_data_set_bool(data, "selftest_on_load", in->selftest_on_load);

	bool ok = obs_data_save_json(data, path);
	obs_data_release(data);

	if (ok) {
		blog(LOG_INFO, CM_LOG_PREFIX "settings saved path=%s", path);
	} else {
		blog(LOG_WARNING,
		     CM_LOG_PREFIX "settings save FAILED path=%s", path);
	}
	return ok;
}

const struct cm_settings *cm_settings_current(void) { return &current; }

void cm_settings_set_current(const struct cm_settings *in)
{
	current = *in;
}
