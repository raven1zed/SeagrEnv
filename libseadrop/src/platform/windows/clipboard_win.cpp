/**
 * @file clipboard_win.cpp
 * @brief Windows clipboard implementation
 */

#include "seadrop/clipboard.h"
#include "seadrop/platform.h"

#ifdef SEADROP_PLATFORM_WINDOWS

#include <windows.h>

namespace seadrop {

// ============================================================================
// Platform Helper Implementations
// ============================================================================

// Windows clipboard access helper functions would go here

// ============================================================================
// ClipboardManager Platform Implementation
// ============================================================================

// Platform-specific clipboard implementation
// Uses Win32 clipboard API:
// - OpenClipboard()
// - GetClipboardData()
// - SetClipboardData()
// - CloseClipboard()

// For clipboard monitoring:
// - AddClipboardFormatListener()
// - RemoveClipboardFormatListener()

} // namespace seadrop

#endif // SEADROP_PLATFORM_WINDOWS
