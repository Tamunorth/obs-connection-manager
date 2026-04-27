#include "bitrate-controller.h"
#include "cm-types.h"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs.h>

static obs_output_t *attached_output = NULL;
static obs_encoder_t *attached_encoder = NULL;
static int target_kbps = 0;
static int current_kbps = 0;
static bool encoder_supports_dyn = false;

bool cm_bitrate_attach(obs_output_t *output)
{
	cm_bitrate_detach();
	if (!output)
		return false;

	attached_output = output;
	attached_encoder = obs_output_get_video_encoder(output);
	if (!attached_encoder) {
		blog(LOG_WARNING,
		     CM_LOG_PREFIX "attach: no video encoder on output");
		attached_output = NULL;
		return false;
	}

	uint32_t caps = obs_encoder_get_caps(attached_encoder);
	encoder_supports_dyn = (caps & OBS_ENCODER_CAP_DYN_BITRATE) != 0;

	obs_data_t *settings = obs_encoder_get_settings(attached_encoder);
	if (settings) {
		target_kbps = (int)obs_data_get_int(settings, "bitrate");
		obs_data_release(settings);
	}
	if (target_kbps <= 0)
		target_kbps = 6000;

	current_kbps = target_kbps;

	const char *enc_id = obs_encoder_get_id(attached_encoder);
	blog(LOG_INFO,
	     CM_LOG_PREFIX
	     "encoder attached id=%s target=%d caps=0x%x dyn=%d",
	     enc_id ? enc_id : "?", target_kbps, caps,
	     encoder_supports_dyn ? 1 : 0);
	return true;
}

void cm_bitrate_detach(void)
{
	if (attached_output) {
		blog(LOG_INFO, CM_LOG_PREFIX "encoder detached");
	}
	attached_output = NULL;
	attached_encoder = NULL;
	target_kbps = 0;
	current_kbps = 0;
	encoder_supports_dyn = false;
}

bool cm_bitrate_supports_dyn(void) { return encoder_supports_dyn; }
int cm_bitrate_get_target_kbps(void) { return target_kbps; }
int cm_bitrate_get_current_kbps(void) { return current_kbps; }

bool cm_bitrate_set_kbps(int kbps)
{
	if (!attached_encoder) {
		current_kbps = kbps;
		return false;
	}
	if (!encoder_supports_dyn) {
		blog(LOG_WARNING,
		     CM_LOG_PREFIX
		     "encoder does not support dynamic bitrate; refusing to set %d",
		     kbps);
		return false;
	}
	if (kbps == current_kbps)
		return true;

	obs_data_t *settings = obs_encoder_get_settings(attached_encoder);
	if (!settings)
		settings = obs_data_create();

	obs_data_set_int(settings, "bitrate", kbps);
	obs_encoder_update(attached_encoder, settings);
	obs_data_release(settings);

	current_kbps = kbps;
	blog(LOG_INFO, CM_LOG_PREFIX "encoder bitrate set kbps=%d", kbps);
	return true;
}

void cm_bitrate_restore_target(void)
{
	if (target_kbps > 0)
		cm_bitrate_set_kbps(target_kbps);
}

float cm_bitrate_read_congestion(void)
{
	if (!attached_output)
		return 0.0f;
	return obs_output_get_congestion(attached_output);
}

int cm_bitrate_read_dropped(void)
{
	if (!attached_output)
		return 0;
	return obs_output_get_frames_dropped(attached_output);
}

int cm_bitrate_read_total_frames(void)
{
	if (!attached_output)
		return 0;
	return obs_output_get_total_frames(attached_output);
}

int cm_bitrate_read_connect_time_ms(void)
{
	if (!attached_output)
		return 0;
	return obs_output_get_connect_time_ms(attached_output);
}
