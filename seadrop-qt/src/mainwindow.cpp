/**
 * @file mainwindow.cpp
 * @brief Main window implementation
 */

#include "mainwindow.h"
#include "devicemodel.h"
#include "settingsdialog.h"
#include "transferdialog.h"
#include "ui_mainwindow.h"


#include <QClipboard>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QSystemTrayIcon>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(std::make_unique<Ui::MainWindow>()),
      bridge_(std::make_unique<SeaDropBridge>(this)),
      deviceModel_(std::make_unique<DeviceModel>(this)) {

  ui->setupUi(this);
  setupUi();
  setupConnections();

  // Enable drag and drop
  setAcceptDrops(true);

  // Start discovery on launch
  bridge_->startDiscovery();
}

MainWindow::~MainWindow() = default;

// ============================================================================
// Setup
// ============================================================================

void MainWindow::setupUi() {
  setWindowTitle("SeaDrop");
  setMinimumSize(400, 500);

  // Set device list model
  ui->deviceListView->setModel(deviceModel_.get());

  // Initial state
  updateConnectionState(ConnectionState::Disconnected);
}

void MainWindow::setupConnections() {
  // UI buttons
  connect(ui->discoverButton, &QPushButton::toggled, this,
          [this](bool checked) {
            if (checked)
              onStartDiscovery();
            else
              onStopDiscovery();
          });

  connect(ui->connectButton, &QPushButton::clicked, this,
          &MainWindow::onConnectClicked);

  connect(ui->disconnectButton, &QPushButton::clicked, this,
          &MainWindow::onDisconnectClicked);

  connect(ui->sendFilesButton, &QPushButton::clicked, this,
          &MainWindow::onSendFilesClicked);

  connect(ui->sendClipboardButton, &QPushButton::clicked, this,
          &MainWindow::onSendClipboardClicked);

  connect(ui->actionSettings, &QAction::triggered, this,
          &MainWindow::onSettingsClicked);

  connect(ui->actionAbout, &QAction::triggered, this,
          &MainWindow::onAboutClicked);

  // Bridge signals
  connect(bridge_.get(), &SeaDropBridge::deviceDiscovered, this,
          &MainWindow::onDeviceDiscovered);

  connect(bridge_.get(), &SeaDropBridge::deviceUpdated, this,
          &MainWindow::onDeviceUpdated);

  connect(bridge_.get(), &SeaDropBridge::deviceLost, this,
          &MainWindow::onDeviceLost);

  connect(bridge_.get(), &SeaDropBridge::connectionRequest, this,
          &MainWindow::onConnectionRequest);

  connect(bridge_.get(), &SeaDropBridge::connected, this,
          &MainWindow::onConnected);

  connect(bridge_.get(), &SeaDropBridge::disconnected, this,
          &MainWindow::onDisconnected);

  connect(bridge_.get(), &SeaDropBridge::transferProgress, this,
          &MainWindow::onTransferProgress);

  connect(bridge_.get(), &SeaDropBridge::errorOccurred, this,
          &MainWindow::onErrorOccurred);
}

// ============================================================================
// UI Actions
// ============================================================================

void MainWindow::onStartDiscovery() {
  bridge_->startDiscovery();
  ui->statusBar->showMessage("Discovering nearby devices...");
}

void MainWindow::onStopDiscovery() {
  bridge_->stopDiscovery();
  ui->statusBar->showMessage("Discovery stopped");
}

void MainWindow::onConnectClicked() {
  auto index = ui->deviceListView->currentIndex();
  if (!index.isValid()) {
    QMessageBox::warning(this, "No Device Selected",
                         "Please select a device to connect to.");
    return;
  }

  QString deviceId = deviceModel_->deviceIdAt(index);
  bridge_->connectToDevice(deviceId);
}

void MainWindow::onDisconnectClicked() { bridge_->disconnect(); }

void MainWindow::onSendFilesClicked() {
  QStringList files =
      QFileDialog::getOpenFileNames(this, "Select Files to Send");

  if (!files.isEmpty()) {
    bridge_->sendFiles(files);
  }
}

void MainWindow::onSendClipboardClicked() { bridge_->sendClipboard(); }

void MainWindow::onSettingsClicked() {
  SettingsDialog dialog(this);
  dialog.setDeviceName(bridge_->getDeviceName());

  if (dialog.exec() == QDialog::Accepted) {
    bridge_->setDeviceName(dialog.deviceName());
    bridge_->setAutoAcceptFromTrusted(dialog.autoAcceptFromTrusted());
  }
}

void MainWindow::onAboutClicked() {
  QMessageBox::about(
      this, "About SeaDrop",
      "<h2>SeaDrop</h2>"
      "<p>Version 1.0.0</p>"
      "<p>Open-source AirDrop alternative for Linux & Android.</p>"
      "<p>Using WiFi Direct for fast, peer-to-peer transfers.</p>");
}

// ============================================================================
// Bridge Signal Handlers
// ============================================================================

void MainWindow::onDeviceDiscovered(const DeviceInfo &device) {
  deviceModel_->addOrUpdateDevice(device);
  ui->statusBar->showMessage(QString("Found: %1").arg(device.name), 3000);
}

void MainWindow::onDeviceUpdated(const DeviceInfo &device) {
  deviceModel_->addOrUpdateDevice(device);
}

void MainWindow::onDeviceLost(const QString &deviceId) {
  deviceModel_->removeDevice(deviceId);
}

void MainWindow::onConnectionRequest(const DeviceInfo &device) {
  int result = QMessageBox::question(
      this, "Connection Request",
      QString("%1 wants to connect.\n\nAccept connection?").arg(device.name),
      QMessageBox::Yes | QMessageBox::No);

  if (result == QMessageBox::Yes) {
    bridge_->acceptConnection(device.id);
  } else {
    bridge_->rejectConnection(device.id);
  }
}

void MainWindow::onConnected(const DeviceInfo &device) {
  updateConnectionState(ConnectionState::Connected);
  ui->statusBar->showMessage(QString("Connected to %1").arg(device.name));
}

void MainWindow::onDisconnected(const QString &reason) {
  updateConnectionState(ConnectionState::Disconnected);
  ui->statusBar->showMessage(QString("Disconnected: %1").arg(reason));
}

void MainWindow::onTransferProgress(const TransferInfo &transfer) {
  // Update progress in status bar or dedicated widget
  ui->statusBar->showMessage(QString("Transferring: %1 (%2%)")
                                 .arg(transfer.fileName)
                                 .arg(transfer.progressPercent));
}

void MainWindow::onErrorOccurred(const QString &title, const QString &message) {
  QMessageBox::warning(this, title, message);
}

// ============================================================================
// Drag and Drop
// ============================================================================

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
  }
}

void MainWindow::dropEvent(QDropEvent *event) {
  QStringList files;
  for (const auto &url : event->mimeData()->urls()) {
    if (url.isLocalFile()) {
      files.append(url.toLocalFile());
    }
  }

  if (!files.isEmpty() && bridge_->isConnected()) {
    bridge_->sendFiles(files);
  } else if (!bridge_->isConnected()) {
    QMessageBox::information(this, "Not Connected",
                             "Connect to a device first before sending files.");
  }
}

void MainWindow::closeEvent(QCloseEvent *event) {
  bridge_->stopDiscovery();
  bridge_->disconnect();
  event->accept();
}

// ============================================================================
// Helpers
// ============================================================================

void MainWindow::updateConnectionState(ConnectionState state) {
  bool connected = (state == ConnectionState::Connected);

  ui->connectButton->setEnabled(!connected);
  ui->disconnectButton->setEnabled(connected);
  ui->sendFilesButton->setEnabled(connected);
  ui->sendClipboardButton->setEnabled(connected);
}

void MainWindow::showTrayNotification(const QString &title,
                                      const QString &message) {
  // TODO: Implement system tray notifications
  Q_UNUSED(title);
  Q_UNUSED(message);
}
