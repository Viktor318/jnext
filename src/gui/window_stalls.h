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
//     second to open. Viktor's decision: while one is open the machine
//     pauses and the sound device is paused, instead of stuttering (ModalHold;
//     QtApp pauses the tick and the audio device).
//
// Plain numbers here, so the rules are testable without <windows.h>.

#include <cstdint>
#include <functional>

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

/// While a modal dialog is open (or being built) the machine is held. Holds
/// nest; the callback hears `true` on the first begin and `false` on the
/// last end, once each. An end with nothing held is ignored.
class ModalHold {
public:
    using Callback = std::function<void(bool held)>;

    void set_callback(Callback cb) { cb_ = std::move(cb); }
    void begin()
    {
        if (depth_++ == 0 && cb_) cb_(true);
    }
    void end()
    {
        if (depth_ == 0) return;
        if (--depth_ == 0 && cb_) cb_(false);
    }
    int depth() const { return depth_; }
    bool held() const { return depth_ > 0; }

    /// begin() for the lifetime of a scope.
    class Scope {
    public:
        explicit Scope(ModalHold& h) : h_(h) { h_.begin(); }
        ~Scope() { h_.end(); }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        ModalHold& h_;
    };

private:
    Callback cb_;
    int depth_ = 0;
};

}  // namespace window_stalls
