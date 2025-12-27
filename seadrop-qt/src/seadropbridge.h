/**
 * @file seadropbridge.h
 * @brief Bridge between Qt UI and libseadrop
 *
 * This class wraps libseadrop functionality and exposes it via
 * Qt-friendly signals and slots. All heavy operations run in
 * worker threads to keep the UI responsive.
 */

#ifndef SEADROPBRIDGE_H
#define SEADROPBRIDGE_H

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>


// Forward declare seadrop types to avoid including in header
namespace seadrop {
class SeaDrop;
}

/**
 * @brief Device information for display in UI
 */
struct DeviceInfo {
  QString id;         // Device ID (hex string)
  QString name;       // Display name
  QString deviceType; // "phone", "tablet", "laptop", etc.
  int rssi = 0;       // Signal strength (dBm)
  bool isTrusted = false;
  bool isConnected = false;

  QVariantMap toVariantMap() const;
};

/**
 * @brief Transfer progress information
 */
struct TransferInfo {
  QString transferId;
  QString fileName;
  qint64 totalBytes = 0;
  qint64 transferredBytes = 0;
  int progressPercent = 0;
  QString status; // "pending", "active", "completed", "failed"
  bool isIncoming = false;
};

/**
 * @brief Connection state
 */
enum class ConnectionState {
  Disconnected,
  Connecting,
  Connected,
  Transferring,
  Error
};

/**
 * @brief Bridge class connecting Qt UI to libseadrop
 */
class SeaDropBridge : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool isDiscovering READ isDiscovering NOTIFY discoveryStateChanged)
  Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionStateChanged)

public:
  explicit SeaDropBridge(QObject *parent = nullptr);
  ~SeaDropBridge();

  // ========================================================================
  // Properties
  // ========================================================================

  bool isDiscovering() const;
  bool isConnected() const;
  ConnectionState connectionState() const;

  // ========================================================================
  // Discovery
  // ========================================================================

  Q_INVOKABLE void startDiscovery();
  Q_INVOKABLE void stopDiscovery();

  // ========================================================================
  // Connection
  // ========================================================================

  Q_INVOKABLE void connectToDevice(const QString &deviceId);
  Q_INVOKABLE void disconnect();
  Q_INVOKABLE void acceptConnection(const QString &deviceId);
  Q_INVOKABLE void rejectConnection(const QString &deviceId);

  // ========================================================================
  // Transfer
  // ========================================================================

  Q_INVOKABLE void sendFiles(const QStringList &filePaths);
  Q_INVOKABLE void sendClipboard();
  Q_INVOKABLE void cancelTransfer(const QString &transferId);
  Q_INVOKABLE void acceptTransfer(const QString &transferId);
  Q_INVOKABLE void rejectTransfer(const QString &transferId);

  // ========================================================================
  // Trust Management
  // ========================================================================

  Q_INVOKABLE void trustDevice(const QString &deviceId);
  Q_INVOKABLE void untrustDevice(const QString &deviceId);
  Q_INVOKABLE QList<DeviceInfo> getTrustedDevices() const;

  // ========================================================================
  // Settings
  // ========================================================================

  Q_INVOKABLE void setDeviceName(const QString &name);
  Q_INVOKABLE QString getDeviceName() const;
  Q_INVOKABLE void setAutoAcceptFromTrusted(bool enabled);

signals:
  // Discovery signals
  void discoveryStateChanged(bool isDiscovering);
  void deviceDiscovered(const DeviceInfo &device);
  void deviceUpdated(const DeviceInfo &device);
  void deviceLost(const QString &deviceId);

  // Connection signals
  void connectionStateChanged(ConnectionState state);
  void connectionRequest(const DeviceInfo &device);
  void connected(const DeviceInfo &device);
  void disconnected(const QString &reason);

  // Transfer signals
  void transferRequest(const TransferInfo &transfer,
                       const DeviceInfo &fromDevice);
  void transferProgress(const TransferInfo &transfer);
  void transferCompleted(const TransferInfo &transfer);
  void transferFailed(const TransferInfo &transfer, const QString &error);

  // Clipboard signals
  void clipboardReceived(const QString &text, const DeviceInfo &fromDevice);

  // Error signals
  void errorOccurred(const QString &title, const QString &message);

private:
  class Impl;
  std::unique_ptr<Impl> impl_;

  void initializeSeaDrop();
  void setupCallbacks();
};

#endif // SEADROPBRIDGE_H
