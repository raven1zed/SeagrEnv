/**
 * @file seadropbridge.cpp
 * @brief SeaDrop Bridge Implementation
 */

#include "seadropbridge.h"
#include <QDebug>
#include <QThread>
#include <seadrop/seadrop.h>


// ============================================================================
// DeviceInfo Helpers
// ============================================================================

QVariantMap DeviceInfo::toVariantMap() const {
  QVariantMap map;
  map["id"] = id;
  map["name"] = name;
  map["deviceType"] = deviceType;
  map["rssi"] = rssi;
  map["isTrusted"] = isTrusted;
  map["isConnected"] = isConnected;
  return map;
}

// ============================================================================
// SeaDropBridge Implementation
// ============================================================================

class SeaDropBridge::Impl {
public:
  seadrop::SeaDrop seadrop;
  bool discovering = false;
  ConnectionState connState = ConnectionState::Disconnected;
  QString deviceName = "My Device";
};

SeaDropBridge::SeaDropBridge(QObject *parent)
    : QObject(parent), impl_(std::make_unique<Impl>()) {
  initializeSeaDrop();
}

SeaDropBridge::~SeaDropBridge() { impl_->seadrop.shutdown(); }

// ============================================================================
// Properties
// ============================================================================

bool SeaDropBridge::isDiscovering() const { return impl_->discovering; }

bool SeaDropBridge::isConnected() const {
  return impl_->connState == ConnectionState::Connected ||
         impl_->connState == ConnectionState::Transferring;
}

ConnectionState SeaDropBridge::connectionState() const {
  return impl_->connState;
}

// ============================================================================
// Initialization
// ============================================================================

void SeaDropBridge::initializeSeaDrop() {
  seadrop::SeaDropConfig config;
  config.device_name = impl_->deviceName.toStdString();
  config.device_type = seadrop::DeviceType::Laptop;

  auto result = impl_->seadrop.init(config);
  if (result.is_error()) {
    qWarning() << "Failed to initialize SeaDrop:"
               << QString::fromStdString(result.error().message);
    emit errorOccurred("Initialization Error",
                       QString::fromStdString(result.error().message));
    return;
  }

  setupCallbacks();
}

void SeaDropBridge::setupCallbacks() {
  // Device discovered callback
  impl_->seadrop.on_device_discovered(
      [this](const seadrop::DiscoveredDevice &device) {
        DeviceInfo info;
        info.id = QString::fromStdString(device.id.to_hex());
        info.name = QString::fromStdString(device.name);
        info.rssi = device.rssi;
        // TODO: map device type
        info.deviceType = "unknown";

        // Emit on main thread
        QMetaObject::invokeMethod(
            this, [this, info]() { emit deviceDiscovered(info); },
            Qt::QueuedConnection);
      });

  // Device lost callback
  impl_->seadrop.on_device_lost([this](const seadrop::DeviceId &id) {
    QString deviceId = QString::fromStdString(id.to_hex());
    QMetaObject::invokeMethod(
        this, [this, deviceId]() { emit deviceLost(deviceId); },
        Qt::QueuedConnection);
  });

  // Connection request callback
  impl_->seadrop.on_connection_request([this](const seadrop::Device &device) {
    DeviceInfo info;
    info.id = QString::fromStdString(device.id.to_hex());
    info.name = QString::fromStdString(device.name);

    QMetaObject::invokeMethod(
        this, [this, info]() { emit connectionRequest(info); },
        Qt::QueuedConnection);
  });

  // Connected callback
  impl_->seadrop.on_connected([this](const seadrop::Device &device) {
    impl_->connState = ConnectionState::Connected;

    DeviceInfo info;
    info.id = QString::fromStdString(device.id.to_hex());
    info.name = QString::fromStdString(device.name);
    info.isConnected = true;

    QMetaObject::invokeMethod(
        this,
        [this, info]() {
          emit connectionStateChanged(ConnectionState::Connected);
          emit connected(info);
        },
        Qt::QueuedConnection);
  });

  // Disconnected callback
  impl_->seadrop.on_disconnected([this](const std::string &reason) {
    impl_->connState = ConnectionState::Disconnected;

    QString qReason = QString::fromStdString(reason);
    QMetaObject::invokeMethod(
        this,
        [this, qReason]() {
          emit connectionStateChanged(ConnectionState::Disconnected);
          emit disconnected(qReason);
        },
        Qt::QueuedConnection);
  });

  // Transfer progress callback
  impl_->seadrop.on_transfer_progress(
      [this](const seadrop::TransferProgress &progress) {
        TransferInfo info;
        info.transferId = QString::fromStdString(progress.transfer_id);
        info.fileName = QString::fromStdString(progress.current_file);
        info.totalBytes = progress.total_bytes;
        info.transferredBytes = progress.transferred_bytes;
        info.progressPercent =
            static_cast<int>((progress.transferred_bytes * 100) /
                             std::max(progress.total_bytes, int64_t(1)));

        QMetaObject::invokeMethod(
            this, [this, info]() { emit transferProgress(info); },
            Qt::QueuedConnection);
      });
}

// ============================================================================
// Discovery
// ============================================================================

void SeaDropBridge::startDiscovery() {
  auto result = impl_->seadrop.start_discovery();
  if (result.is_error()) {
    emit errorOccurred("Discovery Error",
                       QString::fromStdString(result.error().message));
    return;
  }

  impl_->discovering = true;
  emit discoveryStateChanged(true);
}

void SeaDropBridge::stopDiscovery() {
  impl_->seadrop.stop_discovery();
  impl_->discovering = false;
  emit discoveryStateChanged(false);
}

// ============================================================================
// Connection
// ============================================================================

void SeaDropBridge::connectToDevice(const QString &deviceId) {
  impl_->connState = ConnectionState::Connecting;
  emit connectionStateChanged(ConnectionState::Connecting);

  seadrop::DeviceId id = seadrop::DeviceId::from_hex(deviceId.toStdString());
  auto result = impl_->seadrop.connect(id);

  if (result.is_error()) {
    impl_->connState = ConnectionState::Error;
    emit connectionStateChanged(ConnectionState::Error);
    emit errorOccurred("Connection Error",
                       QString::fromStdString(result.error().message));
  }
}

void SeaDropBridge::disconnect() { impl_->seadrop.disconnect(); }

void SeaDropBridge::acceptConnection(const QString &deviceId) {
  seadrop::DeviceId id = seadrop::DeviceId::from_hex(deviceId.toStdString());
  impl_->seadrop.accept_connection(id);
}

void SeaDropBridge::rejectConnection(const QString &deviceId) {
  seadrop::DeviceId id = seadrop::DeviceId::from_hex(deviceId.toStdString());
  impl_->seadrop.reject_connection(id);
}

// ============================================================================
// Transfer
// ============================================================================

void SeaDropBridge::sendFiles(const QStringList &filePaths) {
  std::vector<std::string> paths;
  for (const auto &path : filePaths) {
    paths.push_back(path.toStdString());
  }

  auto result = impl_->seadrop.send_files(paths);
  if (result.is_error()) {
    emit errorOccurred("Transfer Error",
                       QString::fromStdString(result.error().message));
  }
}

void SeaDropBridge::sendClipboard() {
  auto result = impl_->seadrop.send_clipboard();
  if (result.is_error()) {
    emit errorOccurred("Clipboard Error",
                       QString::fromStdString(result.error().message));
  }
}

void SeaDropBridge::cancelTransfer(const QString &transferId) {
  impl_->seadrop.cancel_transfer(transferId.toStdString());
}

void SeaDropBridge::acceptTransfer(const QString &transferId) {
  impl_->seadrop.accept_transfer(transferId.toStdString());
}

void SeaDropBridge::rejectTransfer(const QString &transferId) {
  impl_->seadrop.reject_transfer(transferId.toStdString());
}

// ============================================================================
// Trust Management
// ============================================================================

void SeaDropBridge::trustDevice(const QString &deviceId) {
  seadrop::DeviceId id = seadrop::DeviceId::from_hex(deviceId.toStdString());
  impl_->seadrop.trust_device(id);
}

void SeaDropBridge::untrustDevice(const QString &deviceId) {
  seadrop::DeviceId id = seadrop::DeviceId::from_hex(deviceId.toStdString());
  impl_->seadrop.untrust_device(id);
}

QList<DeviceInfo> SeaDropBridge::getTrustedDevices() const {
  QList<DeviceInfo> result;
  auto devices = impl_->seadrop.get_trusted_devices();

  for (const auto &device : devices) {
    DeviceInfo info;
    info.id = QString::fromStdString(device.id.to_hex());
    info.name = QString::fromStdString(device.name);
    info.isTrusted = true;
    result.append(info);
  }

  return result;
}

// ============================================================================
// Settings
// ============================================================================

void SeaDropBridge::setDeviceName(const QString &name) {
  impl_->deviceName = name;
  impl_->seadrop.set_device_name(name.toStdString());
}

QString SeaDropBridge::getDeviceName() const { return impl_->deviceName; }

void SeaDropBridge::setAutoAcceptFromTrusted(bool enabled) {
  impl_->seadrop.set_auto_accept_trusted(enabled);
}
