/**
 * @file settingsdialog.h
 * @brief Settings dialog
 */

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui {
class SettingsDialog;
}
QT_END_NAMESPACE

class SettingsDialog : public QDialog {
  Q_OBJECT

public:
  explicit SettingsDialog(QWidget *parent = nullptr);
  ~SettingsDialog();

  void setDeviceName(const QString &name);
  QString deviceName() const;

  void setAutoAcceptFromTrusted(bool enabled);
  bool autoAcceptFromTrusted() const;

private:
  std::unique_ptr<Ui::SettingsDialog> ui;
};

#endif // SETTINGSDIALOG_H
