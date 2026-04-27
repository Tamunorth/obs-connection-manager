#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/threading.h>
#include <util/platform.h>

#include "cm-types.h"
#include "settings-store.h"
#include "session-log.h"
#include "connection-monitor.h"
#include "notification-bridge.h"
#include "dock.h"
#include "settings-dialog.h"
#include "toast.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-connection-manager", "en-US")

static obs_hotkey_id hk_force_red = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id hk_force_clear = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id hk_run_selftest = OBS_INVALID_HOTKEY_ID;

static void on_hk_force_red(void *data, obs_hotkey_id id, obs_hotkey_t *hk,
			    bool pressed)
{
	(void)data; (void)id; (void)hk;
	if (!pressed) return;
	cm_monitor_set_test_congestion(0.97f);
}

static void on_hk_force_clear(void *data, obs_hotkey_id id, obs_hotkey_t *hk,
			      bool pressed)
{
	(void)data; (void)id; (void)hk;
	if (!pressed) return;
	cm_monitor_clear_test_congestion();
}

static void on_hk_run_selftest(void *data, obs_hotkey_id id, obs_hotkey_t *hk,
			       bool pressed)
{
	(void)data; (void)id; (void)hk;
	if (!pressed) return;
	blog(LOG_INFO, CM_LOG_PREFIX "selftest hotkey pressed");
	cm_monitor_run_selftest();
}

static void on_frontend_event(enum obs_frontend_event event, void *priv)
{
	(void)priv;
	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		cm_monitor_on_streaming_started();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		cm_monitor_on_streaming_stopping();
		break;
	default:
		break;
	}
}

static void *delayed_selftest_thread(void *arg)
{
	(void)arg;
	os_sleep_ms(5000);
	if (cm_settings_current()->selftest_on_load) {
		blog(LOG_INFO, CM_LOG_PREFIX "auto-selftest triggered");
		cm_monitor_run_selftest();
	}
	return NULL;
}

bool obs_module_load(void)
{
	struct cm_settings s;
	cm_settings_load(&s);
	cm_settings_set_current(&s);

	cm_monitor_init();
	cm_dock_init();
	cm_toast_init();
	cm_settings_dialog_register();

	obs_frontend_add_event_callback(on_frontend_event, NULL);

	hk_force_red = obs_hotkey_register_frontend(
		"cm.force_congestion_red",
		"Connection Manager: Force congestion red",
		on_hk_force_red, NULL);
	hk_force_clear = obs_hotkey_register_frontend(
		"cm.force_congestion_clear",
		"Connection Manager: Clear forced congestion",
		on_hk_force_clear, NULL);
	hk_run_selftest = obs_hotkey_register_frontend(
		"cm.run_selftest", "Connection Manager: Run self-test",
		on_hk_run_selftest, NULL);

	blog(LOG_INFO, CM_LOG_PREFIX
	     "hotkeys registered force_red=%d force_clear=%d selftest=%d",
	     (int)hk_force_red, (int)hk_force_clear, (int)hk_run_selftest);

	if (s.selftest_on_load) {
		pthread_t t;
		pthread_create(&t, NULL, delayed_selftest_thread, NULL);
		pthread_detach(t);
	}

	blog(LOG_INFO, "[connection-manager] loaded");
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(on_frontend_event, NULL);
	if (hk_force_red != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(hk_force_red);
	if (hk_force_clear != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(hk_force_clear);
	if (hk_run_selftest != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(hk_run_selftest);

	cm_toast_destroy();
	cm_dock_destroy();
	cm_monitor_shutdown();
	cm_bridge_clear_state_listeners();
	cm_bridge_clear_breaking_listeners();
	blog(LOG_INFO, "[connection-manager] unloaded");
}

const char *obs_module_description(void)
{
	return "Adaptive bitrate manager for OBS. Monitors network congestion "
	       "during a stream and adjusts the encoder bitrate to keep the "
	       "stream alive. Surfaces an alert when the stream is breaking.";
}

const char *obs_module_name(void) { return "OBS Connection Manager"; }
