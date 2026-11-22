#pragma once
// hungarian-ui - what stops the frame timer on Windows, and how the window
// meets it (doc/hu/jnext-atadas.md, wishlist 3; measured on Viktor's Windows
// on 2026-11-22 with a tick log):
//
//   * a click on the title bar that does not move: from WM_NCLBUTTONDOWN
//     (HTCAPTION) to WM_ENTERSIZEMOVE Windows waits to see whether it becomes
//     a drag, up to about half a second, and dispatches nothing meanwhile
//     (100-520 ms gaps every time). The move itself does not stop the timer.
//     The cure is the one SDL3 uses: post a WM_MOUSEMOVE at the cursor, so
//     the wait ends at once and Windows enters its move loop, where the timer
//     runs (nudge_caption_click, mouse_lparam; MainWindow::nativeEvent).
//   * a modal dialog: the native file dialog stalls the GUI thread for
//     100-900 ms at a time while it is open, and Preferences took about a
//     second to open. Pausing the machine meanwhile was tried and dropped
//     (Viktor, 2026-11-22: the emulation must keep running); still open.
//
// Plain numbers here, so the rules are testable without <windows.h>.

#include <cstdint>

namespace window_stalls {

inline constexpr unsigned WM_NCLBUTTONDOWN_ = 0x00A1;
inline constexpr unsigned HIT_CAPTION = 2;   // HTCAPTION

/// Does this message start Windows' wait for a title-bar drag?
constexpr bool nudge_caption_click(unsigned message, std::uintptr_t wparam)
{
    return message == WM_NCLBUTTONDOWN_ && wparam == HIT_CAPTION;
}

/// WM_MOUSEMOVE's lParam for a point in client coordinates: x in the low
/// word, y in the high word, each a signed 16-bit value (a title-bar point
/// is above the client area, so y is negative).
constexpr std::intptr_t mouse_lparam(int x, int y)
{
    const auto lo = static_cast<std::uint32_t>(static_cast<std::uint16_t>(static_cast<std::int16_t>(x)));
    const auto hi = static_cast<std::uint32_t>(static_cast<std::uint16_t>(static_cast<std::int16_t>(y)));
    return static_cast<std::intptr_t>(lo | (hi << 16));
}

}  // namespace window_stalls
