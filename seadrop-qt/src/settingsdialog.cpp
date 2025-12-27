/**
 * @file settingsdialog.cpp
 * @brief Settings dialog implementation
 */

#include "settingsdialog.h"
#include "ui_settingsdialog.h"

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent), ui(std::make_unique<Ui::SettingsDialog>()) {

  ui->setupUi(this);
  setWindowTitle("Settings");
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::setDeviceName(const QString &name) {
  ui->deviceNameEdit->setText(name);
}

QString SettingsDialog::deviceName() const {
  return ui->deviceNameEdit->text();
}

void SettingsDialog::setAutoAcceptFromTrusted(bool enabled) {
  ui->autoAcceptCheckBox->setChecked(enabled);
}

bool SettingsDialog::autoAcceptFromTrusted() const {
  return ui->autoAcceptCheckBox->isChecked();
}
