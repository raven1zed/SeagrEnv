/**
 * @file devicemodel.h
 * @brief Qt Model for device list display
 */

#ifndef DEVICEMODEL_H
#define DEVICEMODEL_H

#include "seadropbridge.h"
#include <QAbstractListModel>
#include <QList>


class DeviceModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum DeviceRoles {
    IdRole = Qt::UserRole + 1,
    NameRole,
    TypeRole,
    RssiRole,
    TrustedRole,
    ConnectedRole
  };

  explicit DeviceModel(QObject *parent = nullptr);

  // QAbstractListModel interface
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  // Model operations
  void addOrUpdateDevice(const DeviceInfo &device);
  void removeDevice(const QString &deviceId);
  void clear();

  QString deviceIdAt(const QModelIndex &index) const;
  DeviceInfo deviceAt(const QModelIndex &index) const;

private:
  QList<DeviceInfo> devices_;

  int findDevice(const QString &id) const;
};

#endif // DEVICEMODEL_H
