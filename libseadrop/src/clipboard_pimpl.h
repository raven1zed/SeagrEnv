#ifndef SEADROP_CLIPBOARD_PIMPL_H
#define SEADROP_CLIPBOARD_PIMPL_H

#include "seadrop/clipboard.h"
#include <mutex>
#include <vector>

namespace seadrop {

class ClipboardManager::Impl {
public:
  ClipboardConfig config;
  mutable std::mutex mutex;
  bool initialized = false;

  // Receive history
  std::vector<ReceivedClipboard> history;
  static constexpr size_t MAX_HISTORY = 50;

  // Callbacks
  std::function<void(const ReceivedClipboard &)> received_cb;
  std::function<void(const Device &)> sent_cb;
  std::function<void(const Error &)> error_cb;
};

// Platform hooks
Result<ClipboardData> platform_get_clipboard();
Result<void> platform_set_clipboard(const ClipboardData &data);
Result<void> platform_register_hotkey(const std::string &hotkey,
                                      std::function<void()> callback);
void platform_unregister_hotkey();

} // namespace seadrop

#endif // SEADROP_CLIPBOARD_PIMPL_H
