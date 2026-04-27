#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QPointer>
#include <obs-module.h>
#include <obs-frontend-api.h>

#include "settings-dialog.h"
#include "settings-store.h"
#include "cm-types.h"

class CmSettingsDialog : public QDialog {
public:
	CmSettingsDialog(QWidget *parent = nullptr) : QDialog(parent)
	{
		setWindowTitle("Connection Manager Settings");
		setMinimumWidth(420);

		auto *root = new QVBoxLayout(this);
		auto *form = new QFormLayout();
		root->addLayout(form);

		enabled_cb = new QCheckBox(this);
		form->addRow("Enable adaptive control", enabled_cb);

		upper_sp = new QSpinBox(this);
		upper_sp->setRange(500, 50000);
		upper_sp->setSuffix(" kbps");
		form->addRow("Upper limit", upper_sp);

		lower_sp = new QSpinBox(this);
		lower_sp->setRange(100, 50000);
		lower_sp->setSuffix(" kbps");
		form->addRow("Lower limit", lower_sp);

		step_sp = new QSpinBox(this);
		step_sp->setRange(50, 10000);
		step_sp->setSuffix(" kbps");
		form->addRow("Step size", step_sp);

		poll_sp = new QSpinBox(this);
		poll_sp->setRange(50, 5000);
		poll_sp->setSuffix(" ms");
		form->addRow("Poll interval", poll_sp);

		stepdown_sp = new QSpinBox(this);
		stepdown_sp->setRange(100, 60000);
		stepdown_sp->setSuffix(" ms");
		form->addRow("Step-down interval", stepdown_sp);

		stepup_sp = new QSpinBox(this);
		stepup_sp->setRange(100, 60000);
		stepup_sp->setSuffix(" ms");
		form->addRow("Step-up interval", stepup_sp);

		recovery_sp = new QSpinBox(this);
		recovery_sp->setRange(100, 120000);
		recovery_sp->setSuffix(" ms");
		form->addRow("Recovery window", recovery_sp);

		breaking_hold_sp = new QSpinBox(this);
		breaking_hold_sp->setRange(100, 120000);
		breaking_hold_sp->setSuffix(" ms");
		form->addRow("Breaking hold", breaking_hold_sp);

		breaking_thr_sp = new QDoubleSpinBox(this);
		breaking_thr_sp->setRange(0.0, 1.0);
		breaking_thr_sp->setSingleStep(0.05);
		breaking_thr_sp->setDecimals(2);
		form->addRow("Breaking threshold", breaking_thr_sp);

		cong_thr_sp = new QDoubleSpinBox(this);
		cong_thr_sp->setRange(0.0, 1.0);
		cong_thr_sp->setSingleStep(0.05);
		cong_thr_sp->setDecimals(2);
		form->addRow("Step-down threshold", cong_thr_sp);

		notify_banner_cb = new QCheckBox(this);
		form->addRow("Show breaking banner", notify_banner_cb);

		notify_toast_cb = new QCheckBox(this);
		form->addRow("Show desktop toast", notify_toast_cb);

		auto_disable_dbr_cb = new QCheckBox(this);
		form->addRow("Auto-disable OBS built-in DBR", auto_disable_dbr_cb);

		selftest_cb = new QCheckBox(this);
		form->addRow("Run self-test on plugin load", selftest_cb);

		/* Buttons */
		auto *btn_row = new QHBoxLayout();
		btn_row->addStretch();

		auto *reset_btn = new QPushButton("Reset to defaults", this);
		btn_row->addWidget(reset_btn);
		auto *cancel_btn = new QPushButton("Cancel", this);
		btn_row->addWidget(cancel_btn);
		auto *save_btn = new QPushButton("Save", this);
		save_btn->setDefault(true);
		btn_row->addWidget(save_btn);
		root->addLayout(btn_row);

		connect(reset_btn, &QPushButton::clicked, this,
			[this]() {
				cm_settings def;
				cm_settings_init_defaults(&def);
				load_into_form(def);
			});
		connect(cancel_btn, &QPushButton::clicked, this,
			&QDialog::reject);
		connect(save_btn, &QPushButton::clicked, this,
			[this]() { save_and_close(); });

		load_current();
		blog(LOG_INFO, CM_LOG_PREFIX "settings dialog opened");
	}

private:
	void load_current()
	{
		const cm_settings *s = cm_settings_current();
		load_into_form(*s);
	}

	void load_into_form(const cm_settings &s)
	{
		enabled_cb->setChecked(s.enabled);
		upper_sp->setValue(s.upper_kbps);
		lower_sp->setValue(s.lower_kbps);
		step_sp->setValue(s.step_size_kbps);
		poll_sp->setValue(s.poll_interval_ms);
		stepdown_sp->setValue(s.step_down_interval_ms);
		stepup_sp->setValue(s.step_up_interval_ms);
		recovery_sp->setValue(s.recovery_window_ms);
		breaking_hold_sp->setValue(s.breaking_hold_ms);
		breaking_thr_sp->setValue(s.breaking_threshold);
		cong_thr_sp->setValue(s.congestion_threshold);
		notify_banner_cb->setChecked(s.notify_banner);
		notify_toast_cb->setChecked(s.notify_toast);
		auto_disable_dbr_cb->setChecked(s.auto_disable_obs_dbr);
		selftest_cb->setChecked(s.selftest_on_load);
	}

	void save_and_close()
	{
		cm_settings s;
		s.enabled = enabled_cb->isChecked();
		s.upper_kbps = upper_sp->value();
		s.lower_kbps = lower_sp->value();
		s.step_size_kbps = step_sp->value();
		s.poll_interval_ms = poll_sp->value();
		s.step_down_interval_ms = stepdown_sp->value();
		s.step_up_interval_ms = stepup_sp->value();
		s.recovery_window_ms = recovery_sp->value();
		s.breaking_hold_ms = breaking_hold_sp->value();
		s.breaking_threshold = (float)breaking_thr_sp->value();
		s.congestion_threshold = (float)cong_thr_sp->value();
		s.notify_banner = notify_banner_cb->isChecked();
		s.notify_toast = notify_toast_cb->isChecked();
		s.auto_disable_obs_dbr = auto_disable_dbr_cb->isChecked();
		s.selftest_on_load = selftest_cb->isChecked();

		cm_settings_set_current(&s);
		cm_settings_save(&s);
		accept();
	}

	QCheckBox *enabled_cb;
	QSpinBox *upper_sp;
	QSpinBox *lower_sp;
	QSpinBox *step_sp;
	QSpinBox *poll_sp;
	QSpinBox *stepdown_sp;
	QSpinBox *stepup_sp;
	QSpinBox *recovery_sp;
	QSpinBox *breaking_hold_sp;
	QDoubleSpinBox *breaking_thr_sp;
	QDoubleSpinBox *cong_thr_sp;
	QCheckBox *notify_banner_cb;
	QCheckBox *notify_toast_cb;
	QCheckBox *auto_disable_dbr_cb;
	QCheckBox *selftest_cb;
};

static QPointer<CmSettingsDialog> g_dialog;

extern "C" void cm_dock_open_settings(void)
{
	if (g_dialog) {
		g_dialog->raise();
		g_dialog->activateWindow();
		return;
	}
	QWidget *parent =
		reinterpret_cast<QWidget *>(obs_frontend_get_main_window());
	g_dialog = new CmSettingsDialog(parent);
	g_dialog->setAttribute(Qt::WA_DeleteOnClose);
	g_dialog->show();
}

static void on_tools_menu_triggered(void *)
{
	cm_dock_open_settings();
}

extern "C" void cm_settings_dialog_register(void)
{
	obs_frontend_add_tools_menu_item("Connection Manager Settings…",
					 on_tools_menu_triggered, nullptr);
	blog(LOG_INFO, CM_LOG_PREFIX "tools-menu registered");
}
