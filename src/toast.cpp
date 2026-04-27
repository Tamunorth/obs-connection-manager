#include <QSystemTrayIcon>
#include <QApplication>
#include <QStyle>
#include <QMetaObject>
#include <QString>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>

#include "toast.h"
#include "notification-bridge.h"
#include "settings-store.h"
#include "cm-types.h"

static QSystemTrayIcon *g_tray = nullptr;
static int64_t last_toast_ms = 0;
static const int64_t TOAST_COOLDOWN_MS = 30000;

static int64_t now_ms() { return os_gettime_ns() / 1000000; }

static void on_breaking(bool entering, const cm_runtime_state *state, void *)
{
	const cm_settings *s = cm_settings_current();
	if (!s->notify_toast)
		return;
	if (!entering)
		return;
	if (!g_tray)
		return;

	int64_t t = now_ms();
	if (t - last_toast_ms < TOAST_COOLDOWN_MS) {
		blog(LOG_INFO, CM_LOG_PREFIX
		     "toast suppressed (cooldown remaining %lldms)",
		     (long long)(TOAST_COOLDOWN_MS - (t - last_toast_ms)));
		return;
	}
	last_toast_ms = t;

	QString title = "OBS stream is breaking";
	QString body = QString(
		"Bitrate %1 kbps, floor %2 kbps, congestion %3%")
		.arg(state ? state->current_kbps : 0)
		.arg(state ? state->floor_kbps : 0)
		.arg(state ? (int)(state->congestion * 100.0f) : 0);

	QMetaObject::invokeMethod(
		g_tray,
		[title, body]() {
			g_tray->showMessage(
				title, body,
				QSystemTrayIcon::Critical, 8000);
		},
		Qt::QueuedConnection);

	blog(LOG_INFO, CM_LOG_PREFIX "notification toast-fired=1");
}

extern "C" void cm_toast_init(void)
{
	if (g_tray)
		return;
	if (!QSystemTrayIcon::isSystemTrayAvailable()) {
		blog(LOG_WARNING, CM_LOG_PREFIX
		     "system tray not available; toast disabled");
		return;
	}
	g_tray = new QSystemTrayIcon();
	QStyle *style = QApplication::style();
	g_tray->setIcon(style->standardIcon(QStyle::SP_MessageBoxWarning));
	g_tray->setToolTip("OBS Connection Manager");
	g_tray->show();

	cm_bridge_register_breaking_listener(on_breaking, nullptr);
	blog(LOG_INFO, CM_LOG_PREFIX "toast initialised");
}

extern "C" void cm_toast_destroy(void)
{
	if (g_tray) {
		g_tray->hide();
		g_tray = nullptr;
	}
}
