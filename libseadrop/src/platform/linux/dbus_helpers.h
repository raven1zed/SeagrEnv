/**
 * @file dbus_helpers.h
 * @brief D-Bus utility functions for Linux platform layer
 *
 * Provides helper macros and utilities for working with D-Bus,
 * used by both BlueZ and wpa_supplicant implementations.
 */

#ifndef SEADROP_PLATFORM_LINUX_DBUS_HELPERS_H
#define SEADROP_PLATFORM_LINUX_DBUS_HELPERS_H

#include "seadrop/error.h"
#include <dbus/dbus.h>
#include <functional>
#include <memory>
#include <string>

namespace seadrop {
namespace platform {

// ============================================================================
// D-Bus Connection RAII Wrapper
// ============================================================================

/**
 * @brief RAII wrapper for DBusConnection
 */
class DBusConnectionWrapper {
public:
  DBusConnectionWrapper() = default;

  explicit DBusConnectionWrapper(DBusConnection *conn, bool add_ref = false)
      : conn_(conn) {
    if (conn_ && add_ref) {
      dbus_connection_ref(conn_);
    }
  }

  ~DBusConnectionWrapper() {
    if (conn_) {
      dbus_connection_unref(conn_);
    }
  }

  // Move-only
  DBusConnectionWrapper(DBusConnectionWrapper &&other) noexcept
      : conn_(other.conn_) {
    other.conn_ = nullptr;
  }

  DBusConnectionWrapper &operator=(DBusConnectionWrapper &&other) noexcept {
    if (this != &other) {
      if (conn_) {
        dbus_connection_unref(conn_);
      }
      conn_ = other.conn_;
      other.conn_ = nullptr;
    }
    return *this;
  }

  DBusConnectionWrapper(const DBusConnectionWrapper &) = delete;
  DBusConnectionWrapper &operator=(const DBusConnectionWrapper &) = delete;

  DBusConnection *get() const { return conn_; }
  operator bool() const { return conn_ != nullptr; }

private:
  DBusConnection *conn_ = nullptr;
};

// ============================================================================
// D-Bus Message RAII Wrapper
// ============================================================================

/**
 * @brief RAII wrapper for DBusMessage
 */
class DBusMessageWrapper {
public:
  DBusMessageWrapper() = default;

  explicit DBusMessageWrapper(DBusMessage *msg) : msg_(msg) {}

  ~DBusMessageWrapper() {
    if (msg_) {
      dbus_message_unref(msg_);
    }
  }

  // Move-only
  DBusMessageWrapper(DBusMessageWrapper &&other) noexcept : msg_(other.msg_) {
    other.msg_ = nullptr;
  }

  DBusMessageWrapper &operator=(DBusMessageWrapper &&other) noexcept {
    if (this != &other) {
      if (msg_) {
        dbus_message_unref(msg_);
      }
      msg_ = other.msg_;
      other.msg_ = nullptr;
    }
    return *this;
  }

  DBusMessageWrapper(const DBusMessageWrapper &) = delete;
  DBusMessageWrapper &operator=(const DBusMessageWrapper &) = delete;

  DBusMessage *get() const { return msg_; }
  DBusMessage *release() {
    DBusMessage *tmp = msg_;
    msg_ = nullptr;
    return tmp;
  }
  operator bool() const { return msg_ != nullptr; }

private:
  DBusMessage *msg_ = nullptr;
};

// ============================================================================
// D-Bus Error Helper
// ============================================================================

/**
 * @brief Convert DBusError to SeaDrop Error
 */
inline Error dbus_error_to_seadrop(const DBusError &err) {
  if (!dbus_error_is_set(&err)) {
    return Error();
  }

  std::string message = err.name ? std::string(err.name) : "D-Bus error";
  if (err.message) {
    message += ": ";
    message += err.message;
  }

  return Error(ErrorCode::PlatformError, message);
}

/**
 * @brief RAII wrapper for DBusError
 */
class DBusErrorWrapper {
public:
  DBusErrorWrapper() { dbus_error_init(&err_); }
  ~DBusErrorWrapper() { dbus_error_free(&err_); }

  DBusError *get() { return &err_; }
  bool is_set() const { return dbus_error_is_set(&err_); }
  Error to_error() const { return dbus_error_to_seadrop(err_); }

  const char *name() const { return err_.name; }
  const char *message() const { return err_.message; }

private:
  DBusError err_;
};

// ============================================================================
// D-Bus Helper Functions
// ============================================================================

/**
 * @brief Get system D-Bus connection
 * @return Connection wrapper or error
 */
inline Result<DBusConnectionWrapper> get_system_bus() {
  DBusErrorWrapper error;
  DBusConnection *conn = dbus_bus_get(DBUS_BUS_SYSTEM, error.get());

  if (!conn || error.is_set()) {
    return error.to_error();
  }

  return DBusConnectionWrapper(conn);
}

/**
 * @brief Call a D-Bus method and get reply
 * @param conn D-Bus connection
 * @param dest Destination service name
 * @param path Object path
 * @param iface Interface name
 * @param method Method name
 * @param timeout_ms Timeout in milliseconds (-1 for default)
 * @return Reply message or error
 */
inline Result<DBusMessageWrapper>
call_method(DBusConnection *conn, const char *dest, const char *path,
            const char *iface, const char *method, int timeout_ms = -1) {

  DBusMessageWrapper msg(
      dbus_message_new_method_call(dest, path, iface, method));

  if (!msg) {
    return Error(ErrorCode::PlatformError, "Failed to create D-Bus message");
  }

  DBusErrorWrapper error;
  DBusMessage *reply = dbus_connection_send_with_reply_and_block(
      conn, msg.get(), timeout_ms, error.get());

  if (!reply || error.is_set()) {
    return error.to_error();
  }

  return DBusMessageWrapper(reply);
}

/**
 * @brief Get a string property from a D-Bus object
 */
inline Result<std::string>
get_string_property(DBusConnection *conn, const char *dest, const char *path,
                    const char *iface, const char *property) {
  // Call org.freedesktop.DBus.Properties.Get
  DBusMessageWrapper msg(dbus_message_new_method_call(
      dest, path, "org.freedesktop.DBus.Properties", "Get"));

  if (!msg) {
    return Error(ErrorCode::PlatformError, "Failed to create D-Bus message");
  }

  // Append arguments: interface name and property name
  dbus_message_append_args(msg.get(), DBUS_TYPE_STRING, &iface,
                           DBUS_TYPE_STRING, &property, DBUS_TYPE_INVALID);

  DBusErrorWrapper error;
  DBusMessage *reply = dbus_connection_send_with_reply_and_block(
      conn, msg.get(), -1, error.get());

  if (!reply || error.is_set()) {
    return error.to_error();
  }

  DBusMessageWrapper reply_wrapper(reply);

  // Parse variant containing string
  DBusMessageIter iter, variant_iter;
  dbus_message_iter_init(reply, &iter);

  if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
    return Error(ErrorCode::PlatformError, "Expected variant type");
  }

  dbus_message_iter_recurse(&iter, &variant_iter);
  if (dbus_message_iter_get_arg_type(&variant_iter) != DBUS_TYPE_STRING) {
    return Error(ErrorCode::PlatformError, "Expected string in variant");
  }

  const char *value = nullptr;
  dbus_message_iter_get_basic(&variant_iter, &value);

  return std::string(value ? value : "");
}

/**
 * @brief Set a property on a D-Bus object
 */
inline Result<void> set_property(DBusConnection *conn, const char *dest,
                                 const char *path, const char *iface,
                                 const char *property, int type,
                                 const void *value) {
  DBusMessageWrapper msg(dbus_message_new_method_call(
      dest, path, "org.freedesktop.DBus.Properties", "Set"));

  if (!msg) {
    return Error(ErrorCode::PlatformError, "Failed to create D-Bus message");
  }

  DBusMessageIter iter, variant_iter;
  dbus_message_iter_init_append(msg.get(), &iter);

  dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &iface);
  dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &property);

  // Create variant with the value
  char type_sig[2] = {static_cast<char>(type), '\0'};
  dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, type_sig,
                                   &variant_iter);
  dbus_message_iter_append_basic(&variant_iter, type, value);
  dbus_message_iter_close_container(&iter, &variant_iter);

  DBusErrorWrapper error;
  DBusMessage *reply = dbus_connection_send_with_reply_and_block(
      conn, msg.get(), -1, error.get());

  if (error.is_set()) {
    return error.to_error();
  }

  if (reply) {
    dbus_message_unref(reply);
  }

  return Result<void>::ok();
}

} // namespace platform
} // namespace seadrop

#endif // SEADROP_PLATFORM_LINUX_DBUS_HELPERS_H
