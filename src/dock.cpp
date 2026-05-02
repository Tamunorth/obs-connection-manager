#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QMetaObject>
#include <obs-module.h>
#include <obs-frontend-api.h>

#include "dock.h"
#include "notification-bridge.h"
#include "connection-monitor.h"
#include "bitrate-controller.h"
#include "settings-store.h"
#include "cm-types.h"
#include <util/platform.h>

static QString human_state(enum cm_state s)
{
	switch (s) {
	case CM_HEALTHY:   return QStringLiteral("Healthy");
	case CM_AT_FLOOR:  return QStringLiteral("Floor reached");
	case CM_BREAKING:  return QStringLiteral("Stream breaking");
	case CM_IDLE:
	case CM_DEGRADING:
	default:           return QString::fromUtf8(cm_state_name(s));
	}
}

class CmDock : public QWidget {
public:
	CmDock(QWidget *parent = nullptr) : QWidget(parent)
	{
		auto *root = new QVBoxLayout(this);
		root->setContentsMargins(8, 8, 8, 8);
		root->setSpacing(6);

		/* Red banner — hidden until BREAKING */
		banner = new QLabel("Stream is breaking", this);
		banner->setAlignment(Qt::AlignCenter);
		banner->setStyleSheet(
			"background-color: #c62828;"
			"color: white;"
			"font-weight: bold;"
			"padding: 6px;"
			"border-radius: 4px;"
		);
		banner->setVisible(false);
		root->addWidget(banner);

		/* Big bitrate display */
		current_label = new QLabel("— kbps", this);
		current_label->setStyleSheet(
			"font-size: 22px; font-weight: bold; color: #fff;");
		current_label->setAlignment(Qt::AlignCenter);
		root->addWidget(current_label);

		configured_label = new QLabel("configured —", this);
		configured_label->setStyleSheet(
			"color: #777; font-size: 10px;");
		configured_label->setAlignment(Qt::AlignCenter);
		root->addWidget(configured_label);

		/* Editable target row */
		auto *target_row = new QHBoxLayout();
		auto *target_lab = new QLabel("Target", this);
		target_lab->setStyleSheet("color: #aaa; font-size: 11px;");
		target_row->addWidget(target_lab);

		target_spin = new QSpinBox(this);
		target_spin->setRange(500, 50000);
		target_spin->setSingleStep(500);
		target_spin->setSuffix(" kbps");
		target_spin->setKeyboardTracking(false); /* commit on Enter / focus-out */
		target_spin->setValue(cm_settings_current()->upper_kbps);
		target_row->addWidget(target_spin, 1);
		root->addLayout(target_row);

		connect(target_spin,
			QOverload<int>::of(&QSpinBox::valueChanged), this,
			[](int new_val) {
				cm_settings updated = *cm_settings_current();
				if (updated.upper_kbps == new_val)
					return;
				updated.upper_kbps = new_val;
				if (updated.lower_kbps > new_val)
					updated.lower_kbps = new_val;
				cm_settings_set_current(&updated);
				cm_settings_save(&updated);
				blog(LOG_INFO, CM_LOG_PREFIX
				     "dock target changed -> %d kbps",
				     new_val);
			});

		floor_label = new QLabel("floor —", this);
		floor_label->setStyleSheet("color: #777; font-size: 10px;");
		floor_label->setAlignment(Qt::AlignCenter);
		root->addWidget(floor_label);

		/* Congestion bar */
		auto *cong_row = new QHBoxLayout();
		auto *cong_label = new QLabel("Congestion", this);
		cong_label->setStyleSheet("color: #aaa; font-size: 11px;");
		cong_row->addWidget(cong_label);
		cong_bar = new QProgressBar(this);
		cong_bar->setRange(0, 100);
		cong_bar->setValue(0);
		cong_bar->setTextVisible(true);
		cong_bar->setFormat("%p%");
		cong_row->addWidget(cong_bar, 1);
		root->addLayout(cong_row);

		/* State badge */
		state_label = new QLabel("IDLE", this);
		state_label->setAlignment(Qt::AlignCenter);
		state_label->setStyleSheet(state_style(CM_IDLE));
		root->addWidget(state_label);

		/* Settings button */
		settings_btn = new QPushButton("Settings…", this);
		settings_btn->setStyleSheet(
			"QPushButton {"
			"  background-color: #333;"
			"  color: #fff;"
			"  border: 1px solid #555;"
			"  border-radius: 4px;"
			"  padding: 6px 12px;"
			"}"
			"QPushButton:hover { background-color: #444; }");
		connect(settings_btn, &QPushButton::clicked, this,
			[]() { cm_dock_open_settings(); });
		root->addWidget(settings_btn);

		root->addStretch();

		/* Poll-pull state at 4Hz as a safety net (bridge is push). */
		auto *timer = new QTimer(this);
		connect(timer, &QTimer::timeout, this, &CmDock::pull_state);
		timer->start(250);
	}

	void apply_state(const cm_runtime_state &s)
	{
		QMetaObject::invokeMethod(
			this,
			[this, s]() {
				/* Live measured bitrate (matches OBS bottom counter) */
				int live = compute_live_kbps();
				if (s.streaming && live > 0)
					current_label->setText(
						QString("%1 kbps")
							.arg(live));
				else if (s.current_kbps > 0)
					current_label->setText(
						QString("%1 kbps")
							.arg(s.current_kbps));
				else
					current_label->setText("— kbps");

				if (s.current_kbps > 0)
					configured_label->setText(
						QString("configured %1 kbps")
							.arg(s.current_kbps));
				else
					configured_label->setText(
						"configured —");

				if (s.floor_kbps > 0)
					floor_label->setText(
						QString("floor %1 kbps")
							.arg(s.floor_kbps));
				else
					floor_label->setText("not streaming");

				/* Keep target spin in sync with settings,
				 * but never while the user is editing it. */
				int upper =
					cm_settings_current()->upper_kbps;
				if (!target_spin->hasFocus() &&
				    target_spin->value() != upper) {
					target_spin->blockSignals(true);
					target_spin->setValue(upper);
					target_spin->blockSignals(false);
				}

				int cong = (int)(s.congestion * 100.0f);
				if (cong < 0) cong = 0;
				if (cong > 100) cong = 100;
				cong_bar->setValue(cong);
				cong_bar->setStyleSheet(cong_style(s.congestion));

				state_label->setText(human_state(s.state));
				state_label->setStyleSheet(state_style(s.state));
			},
			Qt::QueuedConnection);
	}

	void apply_breaking(bool entering)
	{
		QMetaObject::invokeMethod(
			this,
			[this, entering]() {
				banner->setVisible(entering);
				blog(LOG_INFO, CM_LOG_PREFIX
				     "dock banner=%s",
				     entering ? "on" : "off");
			},
			Qt::QueuedConnection);
	}

private:
	void pull_state()
	{
		cm_runtime_state s;
		cm_monitor_get_state(&s);
		apply_state(s);
	}

	/* Live throughput in kbps. Samples over a 1-second window (matches
	 * OBS's status-bar refresh) with light EMA smoothing on top. The
	 * dock polls at 4Hz; the displayed value only refreshes once per
	 * full window. */
	int compute_live_kbps()
	{
		const uint64_t WINDOW_NS = 1000000000ULL; /* 1 s */
		uint64_t bytes = cm_bitrate_read_total_bytes();
		uint64_t now = os_gettime_ns();

		if (window_start_bytes == 0 || bytes < window_start_bytes) {
			window_start_bytes = bytes;
			window_start_ns = now;
			return cached_kbps;
		}

		uint64_t dt_ns = now - window_start_ns;
		if (dt_ns < WINDOW_NS)
			return cached_kbps;

		uint64_t db = bytes - window_start_bytes;
		double seconds = (double)dt_ns / 1.0e9;
		double sample = ((double)db * 8.0) / 1000.0 / seconds;
		if (sample < 0)
			sample = 0;

		/* EMA: alpha=0.5 gives noticeable smoothing but still responsive. */
		if (cached_kbps < 0)
			cached_kbps = (int)(sample + 0.5);
		else
			cached_kbps = (int)(cached_kbps * 0.5 + sample * 0.5 + 0.5);

		window_start_bytes = bytes;
		window_start_ns = now;
		return cached_kbps;
	}

	uint64_t window_start_bytes = 0;
	uint64_t window_start_ns = 0;
	int cached_kbps = -1;

	static QString state_style(enum cm_state s)
	{
		const char *bg = "#444";
		const char *fg = "#fff";
		switch (s) {
		case CM_HEALTHY: bg = "#2e7d32"; break;
		case CM_DEGRADING: bg = "#ef6c00"; break;
		case CM_AT_FLOOR: bg = "#c62828"; break;
		case CM_BREAKING: bg = "#7b1fa2"; break;
		case CM_IDLE:
		default: bg = "#444"; break;
		}
		return QString("background-color: %1; color: %2;"
			       "font-weight: bold; padding: 6px;"
			       "border-radius: 4px;")
			.arg(bg)
			.arg(fg);
	}

	static QString cong_style(float cong)
	{
		const char *chunk = "#4caf50";
		if (cong >= 0.80f) chunk = "#c62828";
		else if (cong >= 0.50f) chunk = "#ef6c00";
		return QString(
			"QProgressBar { border: 1px solid #555; border-radius: 4px;"
			" text-align: center; color: #fff; }"
			"QProgressBar::chunk { background-color: %1; }")
			.arg(chunk);
	}

	QLabel *banner;
	QLabel *current_label;
	QLabel *configured_label;
	QSpinBox *target_spin;
	QLabel *floor_label;
	QLabel *state_label;
	QProgressBar *cong_bar;
	QPushButton *settings_btn;
};

static CmDock *g_dock = nullptr;

static void on_state(const cm_runtime_state *state, void *)
{
	if (g_dock && state)
		g_dock->apply_state(*state);
}

static void on_breaking(bool entering, const cm_runtime_state *, void *)
{
	if (g_dock)
		g_dock->apply_breaking(entering);
}

extern "C" void cm_dock_init(void)
{
	if (g_dock)
		return;
	g_dock = new CmDock();
	obs_frontend_add_dock_by_id("connection-manager-dock",
				    "Connection Manager", g_dock);

	cm_bridge_register_state_listener(on_state, nullptr);
	cm_bridge_register_breaking_listener(on_breaking, nullptr);

	blog(LOG_INFO, CM_LOG_PREFIX "dock registered");
}

extern "C" void cm_dock_destroy(void)
{
	if (!g_dock)
		return;
	obs_frontend_remove_dock("connection-manager-dock");
	g_dock = nullptr;
}

/* Forward declared here so dock can call it; provided by settings-dialog or
 * a stub in plugin-main if D hasn't landed yet. */
extern "C" void cm_dock_open_settings(void);
