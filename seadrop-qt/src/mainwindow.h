/**
 * @file mainwindow.h
 * @brief Main application window
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "seadropbridge.h"
#include <QMainWindow>
#include <memory>


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class DeviceModel;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

protected:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  void closeEvent(QCloseEvent *event) override;

private slots:
  // UI Actions
  void onStartDiscovery();
  void onStopDiscovery();
  void onConnectClicked();
  void onDisconnectClicked();
  void onSendFilesClicked();
  void onSendClipboardClicked();
  void onSettingsClicked();
  void onAboutClicked();

  // Bridge signals
  void onDeviceDiscovered(const DeviceInfo &device);
  void onDeviceUpdated(const DeviceInfo &device);
  void onDeviceLost(const QString &deviceId);
  void onConnectionRequest(const DeviceInfo &device);
  void onConnected(const DeviceInfo &device);
  void onDisconnected(const QString &reason);
  void onTransferProgress(const TransferInfo &transfer);
  void onErrorOccurred(const QString &title, const QString &message);

private:
  std::unique_ptr<Ui::MainWindow> ui;
  std::unique_ptr<SeaDropBridge> bridge_;
  std::unique_ptr<DeviceModel> deviceModel_;

  void setupUi();
  void setupConnections();
  void updateConnectionState(ConnectionState state);
  void showTrayNotification(const QString &title, const QString &message);
};

#endif // MAINWINDOW_H
