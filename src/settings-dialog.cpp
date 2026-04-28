#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpinBox>
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
		setMinimumWidth(360);

		auto *root = new QVBoxLayout(this);
		auto *form = new QFormLayout();
		root->addLayout(form);

		upper_sp = new QSpinBox(this);
		upper_sp->setRange(500, 50000);
		upper_sp->setSingleStep(500);
		upper_sp->setSuffix(" kbps");
		form->addRow("Upper limit", upper_sp);

		lower_sp = new QSpinBox(this);
		lower_sp->setRange(100, 50000);
		lower_sp->setSingleStep(100);
		lower_sp->setSuffix(" kbps");
		form->addRow("Lower limit", lower_sp);

		step_sp = new QSpinBox(this);
		step_sp->setRange(50, 10000);
		step_sp->setSingleStep(100);
		step_sp->setSuffix(" kbps");
		form->addRow("Step size", step_sp);

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
				upper_sp->setValue(def.upper_kbps);
				lower_sp->setValue(def.lower_kbps);
				step_sp->setValue(def.step_size_kbps);
			});
		connect(cancel_btn, &QPushButton::clicked, this,
			&QDialog::reject);
		connect(save_btn, &QPushButton::clicked, this,
			[this]() { save_and_close(); });

		const cm_settings *s = cm_settings_current();
		upper_sp->setValue(s->upper_kbps);
		lower_sp->setValue(s->lower_kbps);
		step_sp->setValue(s->step_size_kbps);

		blog(LOG_INFO, CM_LOG_PREFIX "settings dialog opened");
	}

private:
	void save_and_close()
	{
		/* Preserve all hidden fields from current settings; only
		 * overwrite the three visible ones. */
		cm_settings s = *cm_settings_current();
		s.upper_kbps = upper_sp->value();
		s.lower_kbps = lower_sp->value();
		s.step_size_kbps = step_sp->value();

		if (s.lower_kbps > s.upper_kbps)
			s.lower_kbps = s.upper_kbps;

		cm_settings_set_current(&s);
		cm_settings_save(&s);
		accept();
	}

	QSpinBox *upper_sp;
	QSpinBox *lower_sp;
	QSpinBox *step_sp;
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
