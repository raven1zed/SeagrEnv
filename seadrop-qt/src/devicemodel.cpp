/**
 * @file devicemodel.cpp
 * @brief Device model implementation
 */

#include "devicemodel.h"

DeviceModel::DeviceModel(QObject *parent) : QAbstractListModel(parent) {}

int DeviceModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return devices_.size();
}

QVariant DeviceModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= devices_.size()) {
    return QVariant();
  }

  const auto &device = devices_.at(index.row());

  switch (role) {
  case Qt::DisplayRole:
  case NameRole:
    return device.name;
  case IdRole:
    return device.id;
  case TypeRole:
    return device.deviceType;
  case RssiRole:
    return device.rssi;
  case TrustedRole:
    return device.isTrusted;
  case ConnectedRole:
    return device.isConnected;
  case Qt::DecorationRole:
    // TODO: Return icon based on device type
    return QVariant();
  case Qt::ToolTipRole:
    return QString("%1\nSignal: %2 dBm\n%3")
        .arg(device.name)
        .arg(device.rssi)
        .arg(device.isTrusted ? "Trusted" : "Not trusted");
  default:
    return QVariant();
  }
}

QHash<int, QByteArray> DeviceModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[IdRole] = "deviceId";
  roles[NameRole] = "deviceName";
  roles[TypeRole] = "deviceType";
  roles[RssiRole] = "rssi";
  roles[TrustedRole] = "isTrusted";
  roles[ConnectedRole] = "isConnected";
  return roles;
}

void DeviceModel::addOrUpdateDevice(const DeviceInfo &device) {
  int idx = findDevice(device.id);

  if (idx >= 0) {
    // Update existing
    devices_[idx] = device;
    emit dataChanged(index(idx), index(idx));
  } else {
    // Add new
    beginInsertRows(QModelIndex(), devices_.size(), devices_.size());
    devices_.append(device);
    endInsertRows();
  }
}

void DeviceModel::removeDevice(const QString &deviceId) {
  int idx = findDevice(deviceId);
  if (idx >= 0) {
    beginRemoveRows(QModelIndex(), idx, idx);
    devices_.removeAt(idx);
    endRemoveRows();
  }
}

void DeviceModel::clear() {
  beginResetModel();
  devices_.clear();
  endResetModel();
}

QString DeviceModel::deviceIdAt(const QModelIndex &index) const {
  if (!index.isValid() || index.row() >= devices_.size()) {
    return QString();
  }
  return devices_.at(index.row()).id;
}

DeviceInfo DeviceModel::deviceAt(const QModelIndex &index) const {
  if (!index.isValid() || index.row() >= devices_.size()) {
    return DeviceInfo();
  }
  return devices_.at(index.row());
}

int DeviceModel::findDevice(const QString &id) const {
  for (int i = 0; i < devices_.size(); ++i) {
    if (devices_[i].id == id) {
      return i;
    }
  }
  return -1;
}
