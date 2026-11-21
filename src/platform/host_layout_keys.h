#pragma once

// ---------------------------------------------------------------------------
// Host keyboard layout typing (hungarian-ui fork, "Windows keyboard layout").
//
// On a non-US host layout (Hungarian QWERTZ is the case this was written for)
// the symbols sit on other keys than on the US layout the classic key path
// assumes (main_window.cpp qt_key_to_sdl: LOGICAL Qt key -> US-position SDL
// scancode). Shift+2 types `"` on a Hungarian keyboard but reaches the guest
// as Caps Shift + 2. This module types by CHARACTER instead: the frontend hands
// it the character the host layout produced (QKeyEvent::text()) and it emits
// the Spectrum key combination that types the same character, as SDL-scancode
// events for the key Router (host_key_latch.h):
//
//   - a..z and 0..9: the key itself; A..Z: Caps Shift + the letter;
//   - the symbols of the Spectrum keyboard: Symbol Shift + their key;
//   - accented vowels (Hungarian and a few more): the plain vowel;
//   - the extended-mode symbols [ ] { } \ | ~ and the copyright sign: a timed
//     sequence, E mode (Caps Shift + Symbol Shift), release, Symbol Shift +
//     the key, release, each step STEP_FRAMES emulated frames long;
//   - a character the Spectrum has no key for (e.g. the euro sign) and a dead
//     key (Qt::Key_Dead_*, whose character arrives with the next keystroke)
//     are swallowed: they must not fall through to the classic path.
//
// Keys that produce no printable character (Enter, Space, Backspace, cursor
// keys, Tab, Esc, Caps Lock, the function keys) and keys typed with Ctrl or the
// LEFT Alt take the classic path, unchanged: the module forwards their
// scancode. The right Alt is AltGr: it never reaches the Router while the
// layout is on, and on Windows (where AltGr is left Ctrl + right Alt) the Ctrl
// that comes with it does not either; otherwise AltGr+N (`}`) would arrive with
// Alt held and the guest would get GRAPH (keyboard.cpp: Alt + E/G/C).
//
// Left Alt + digit (its own switch): Caps Shift + the digit, so that EDIT,
// CAPS LOCK, TRUE/INV VIDEO, the cursor keys, GRAPH and DELETE can be typed on
// a keyboard whose Shift+digit gives other characters. Alt+1..8 stay the
// debugger script's host keys while a script listens for them (the Router's
// chord); the decision is taken on the press and the release follows it.
//
// Every host key is tracked by its PHYSICAL id, so a release lets go of
// exactly what its press asserted, whatever the character on release is
// (Shift may have been let go first). While a key typed by character is held,
// or a sequence runs, the module owns the shift keys: host modifier changes
// are recorded and applied when that ends.
//
// After the Fuse fork's hungarian_keyboard.c (same author, same rules). No
// hardware counterpart: the real Next reads a PS/2 US keymap by position
// (keymaps.vhd); this is host input translation. Free of Qt and SDL so it is
// unit-testable (test/platform/host_layout_keys_test.cpp); the scancode values
// are SDL's, pinned against SDL where the module is wired in.
// ---------------------------------------------------------------------------

#include <algorithm>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <utility>
#include <vector>

#include "platform/host_key_latch.h"

namespace host_layout {

// SDL scancodes (SDL_scancode.h): letters, digits.
inline constexpr int SC_A = 4;    // SDL_SCANCODE_A .. SDL_SCANCODE_Z = 29
inline constexpr int SC_Z = 29;
inline constexpr int SC_1 = 30;   // SDL_SCANCODE_1 .. SDL_SCANCODE_9 = 38
inline constexpr int SC_9 = 38;
inline constexpr int SC_0 = 39;   // SDL_SCANCODE_0

using host_key_latch::SC_LCTRL;
using host_key_latch::SC_LSHIFT;
using host_key_latch::SC_LALT;
using host_key_latch::SC_LGUI;
using host_key_latch::SC_RCTRL;
using host_key_latch::SC_RSHIFT;
using host_key_latch::SC_RALT;
using host_key_latch::SC_RGUI;
using host_key_latch::is_modifier;

/// Emulated frames each step of an extended-mode sequence lasts (the Fuse
/// fork's value; long enough for the 48K and 128K editors' KEYBOARD routine).
inline constexpr int STEP_FRAMES = 3;
inline constexpr std::size_t MAX_HELD = 32;
inline constexpr std::size_t QUEUE_SIZE = 32;

inline constexpr int letter(char c) { return SC_A + (c - 'a'); }
inline constexpr int digit(int d) { return d == 0 ? SC_0 : SC_1 + (d - 1); }
inline constexpr bool is_digit_sc(int sc) { return sc >= SC_1 && sc <= SC_0; }
/// 1..9, 0 for SC_0.
inline constexpr int digit_of(int sc) { return sc == SC_0 ? 0 : sc - SC_1 + 1; }

/// The Spectrum key combination that types one character.
struct Combo {
    int  key = 0;           ///< SDL scancode of the letter or digit key
    bool caps = false;      ///< with Caps Shift
    bool symbol = false;    ///< with Symbol Shift
    bool extended = false;  ///< in E mode (a timed sequence)
};

/// Accented letters (Hungarian and a few more) to the plain letter.
inline constexpr char32_t remove_accent(char32_t c)
{
    switch (c) {
    case 0xe1: case 0xe4:               return U'a';   // á ä
    case 0xc1: case 0xc4:               return U'A';   // Á Ä
    case 0xe9:                          return U'e';   // é
    case 0xc9:                          return U'E';   // É
    case 0xed:                          return U'i';   // í
    case 0xcd:                          return U'I';   // Í
    case 0xf3: case 0xf6: case 0x151:   return U'o';   // ó ö ő
    case 0xd3: case 0xd6: case 0x150:   return U'O';   // Ó Ö Ő
    case 0xfa: case 0xfc: case 0x171:   return U'u';   // ú ü ű
    case 0xda: case 0xdc: case 0x170:   return U'U';   // Ú Ü Ű
    default:                            return c;
    }
}

/// The combination for `ch`, or false when the Spectrum has no key for it.
inline bool map_character(char32_t ch, Combo& out)
{
    struct Entry { char32_t ch; int key; };
    // Symbol Shift + key (the red symbols of the 48K keyboard).
    static constexpr Entry symbols[] = {
        { U'!', digit(1) }, { U'@', digit(2) }, { U'#', digit(3) }, { U'$', digit(4) },
        { U'%', digit(5) }, { U'&', digit(6) }, { U'\'', digit(7) }, { U'(', digit(8) },
        { U')', digit(9) }, { U'_', digit(0) },
        { U'<', letter('r') }, { U'>', letter('t') }, { U';', letter('o') }, { U'"', letter('p') },
        { U'^', letter('h') }, { U'-', letter('j') }, { U'+', letter('k') }, { U'=', letter('l') },
        { U':', letter('z') }, { 0xa3, letter('x') },   // £
        { U'?', letter('c') }, { U'/', letter('v') }, { U'*', letter('b') },
        { U',', letter('n') }, { U'.', letter('m') },
        { 0xd7, letter('b') },                          // × (AltGr + ú on the Hungarian layout)
        { 0xf7, letter('v') },                          // ÷ (AltGr + ő)
    };
    // E mode, then Symbol Shift + key.
    static constexpr Entry extended[] = {
        { U'[', letter('y') }, { U']', letter('u') }, { U'{', letter('f') }, { U'}', letter('g') },
        { U'\\', letter('d') }, { U'|', letter('s') }, { U'~', letter('a') },
        { 0xa9, letter('p') },                          // ©
    };

    const char32_t c = remove_accent(ch);
    out = Combo{};
    if (c >= U'a' && c <= U'z') { out.key = SC_A + static_cast<int>(c - U'a'); return true; }
    if (c >= U'A' && c <= U'Z') { out.key = SC_A + static_cast<int>(c - U'A'); out.caps = true; return true; }
    if (c >= U'0' && c <= U'9') { out.key = digit(static_cast<int>(c - U'0')); return true; }
    for (const Entry& e : symbols)
        if (e.ch == c) { out.key = e.key; out.symbol = true; return true; }
    for (const Entry& e : extended)
        if (e.ch == c) { out.key = e.key; out.symbol = true; out.extended = true; return true; }
    return false;
}

/// A character worth typing: printable, not Space (Space is a key of its own on
/// the classic path), not a C0/C1 control character or DEL.
inline constexpr bool printable(char32_t c)
{
    return c > 0x20 && c != 0x7f && !(c >= 0x80 && c < 0xa0);
}

/// One host key event, as the frontend resolved it.
struct KeyEvent {
    /// The physical key (QKeyEvent::nativeScanCode(); if 0, nativeVirtualKey();
    /// if 0, key()). A press and its release carry the same value.
    uint32_t phys = 0;
    /// The classic path's scancode (qt_key_to_sdl), 0 if it has none. For a
    /// modifier key: SC_LSHIFT, SC_LCTRL, SC_LALT, and SC_RALT for the right
    /// Alt / AltGr (the frontend tells them apart).
    int sc = 0;
    /// The character the host layout produced (QKeyEvent::text()), 0 if none.
    char32_t ch = 0;
    /// A dead key (Qt::Key_Dead_*).
    bool dead = false;
};

class Translator {
public:
    /// Receives the module's output: the Router's on_host_key(sc, pressed).
    using Emit = std::function<void(int sc, bool pressed)>;
    /// Does a debugger script listen for Alt+n (n = 1..8)? Asked on the press.
    using ScriptOwns = std::function<bool(int n)>;

    explicit Translator(Emit emit) : emit_(std::move(emit)) {}

    /// The two switches. With both off the module is a plain forward of the
    /// classic scancode, exactly the path without it. Changing either lets go
    /// of everything the module holds on the Router and forgets its state.
    void set_layout(bool on) { if (on != layout_) { release_everything(); layout_ = on; } }
    void set_alt_digit(bool on) { if (on != alt_digit_) { release_everything(); alt_digit_ = on; } }
    bool layout() const { return layout_; }
    bool alt_digit() const { return alt_digit_; }

    void set_script_owns(ScriptOwns fn) { script_owns_ = std::move(fn); }

    /// Is the Router done with what it was sent (`router.pending() == 0`)? A
    /// sequence step is counted only on frames after which it is: behind a
    /// backlog (fast typing, a stall) the Router shows events late, and a step
    /// timed on the module's own clock would be shown for less than
    /// STEP_FRAMES frames, or merged with the next one.
    void set_output_idle(std::function<bool()> fn) { output_idle_ = std::move(fn); }

    /// A key press. Returns false when the module did nothing with it (no
    /// classic scancode and nothing to type): the frontend's default handling
    /// then applies, as for an unmapped key without the module.
    bool on_press(const KeyEvent& e)
    {
        if (transparent()) {
            if (e.sc == 0) return false;
            emit_(e.sc, true);
            return true;
        }
        if (is_modifier(e.sc)) {
            const std::size_t m = mod_index(e.sc);
            if (host_mods_.test(m)) return true;            // host autorepeat
            host_mods_.set(m);
            if (!owned()) apply_mods(base_mods());
            return true;
        }
        if (find(e.phys) != held_.end()) return true;       // host autorepeat
        if (held_.size() >= MAX_HELD) return true;

        Held h;
        h.phys = e.phys;
        h.order = ++order_;

        if (layout_ && e.dead) {
            h.kind = Kind::Swallowed;
            held_.push_back(h);
            return true;
        }

        if (alt_digit_ && is_digit_sc(e.sc) && only_left_alt()) {
            const int n = digit_of(e.sc);
            const bool script = n >= 1 && n <= 8 && script_owns_ && script_owns_(n);
            if (!script) {
                h.kind = Kind::Typed;
                Combo c;
                c.key = e.sc;
                c.caps = true;
                start_typed(h, c);
                return true;
            }
            // A script's host key: the classic path, where the Router's chord
            // takes it (left Alt is held on the Router, nothing else).
        } else if (layout_ && printable(e.ch) && character_path_open()) {
            Combo c;
            if (!map_character(e.ch, c)) {
                h.kind = Kind::Swallowed;
                held_.push_back(h);
                return true;
            }
            h.kind = Kind::Typed;
            start_typed(h, c);
            return true;
        }

        if (e.sc == 0) return false;
        h.kind = Kind::Forwarded;
        h.sc = e.sc;
        held_.push_back(h);
        emit_(e.sc, true);
        return true;
    }

    /// A key release (see on_press for the return value).
    bool on_release(const KeyEvent& e)
    {
        if (transparent()) {
            if (e.sc == 0) return false;
            emit_(e.sc, false);
            return true;
        }
        if (is_modifier(e.sc)) {
            host_mods_.reset(mod_index(e.sc));
            if (!owned()) apply_mods(base_mods());
            return true;
        }
        const auto it = find(e.phys);
        if (it == held_.end()) {
            // Pressed before the module tracked it (a switch changed while it
            // was held): let go of what the classic path would have pressed.
            if (e.sc == 0) return false;
            emit_(e.sc, false);
            return true;
        }
        const Held h = *it;
        held_.erase(it);
        switch (h.kind) {
        case Kind::Forwarded:
            emit_(h.sc, false);
            break;
        case Kind::Typed:
            if (h.sc != 0) emit_(h.sc, false);
            restore_shifts();
            break;
        case Kind::Swallowed:
            break;
        }
        return true;
    }

    /// `frames` emulated frames have run (the same count the Router's
    /// on_tick_end gets, called after it; 0 when the machine is paused, and
    /// then nothing moves).
    void on_frames(int frames)
    {
        // Count these frames only if the Router had applied everything before
        // they ran (sampled at the end of the previous call): what a drain in
        // this tick's on_tick_end applied is seen from the NEXT frame on.
        const bool count = shown_;
        shown_ = idle();
        if (!count) return;
        while (frames > 0 && !queue_.empty()) {
            const int take = std::min(frames, step_left_);
            step_left_ -= take;
            frames -= take;
            if (step_left_ > 0) break;
            if (++step_ > 3) {
                queue_.pop_front();
                if (queue_.empty()) { restore_shifts(); break; }
                step_ = queue_.front().extended ? 0 : 2;
            }
            step_left_ = STEP_FRAMES;
            apply_step();
        }
        shown_ = idle();
    }

    /// The window lost the keyboard: the Router lets go of everything itself
    /// (Router::release_all), so only the module's bookkeeping is cleared.
    void reset()
    {
        host_mods_.reset();
        out_mods_.reset();
        held_.clear();
        queue_.clear();
        step_ = 0;
        step_left_ = 0;
        shown_ = true;
    }

    // Diagnostics and tests.
    bool sequence_running() const { return !queue_.empty(); }
    std::size_t held_count() const { return held_.size(); }
    std::size_t queued() const { return queue_.size(); }

private:
    enum class Kind { Forwarded, Typed, Swallowed };

    struct Held {
        uint32_t phys = 0;
        Kind kind = Kind::Forwarded;
        int sc = 0;             ///< the key held on the Router for it, 0 = none
        bool caps = false;
        bool symbol = false;
        unsigned long order = 0;
    };

    using Mods = std::bitset<8>;   // index = scancode - SC_LCTRL

    static std::size_t mod_index(int sc) { return static_cast<std::size_t>(sc - SC_LCTRL); }
    static Mods mods_for(bool caps, bool symbol)
    {
        Mods m;
        if (caps) m.set(mod_index(SC_LSHIFT));
        if (symbol) m.set(mod_index(SC_LCTRL));
        return m;
    }

    bool transparent() const { return !layout_ && !alt_digit_; }
    bool host(int sc) const { return host_mods_.test(mod_index(sc)); }

    /// AltGr (right Alt) opens the character path even with Ctrl (Windows'
    /// AltGr is Ctrl + right Alt); otherwise Ctrl and left Alt close it.
    bool character_path_open() const
    {
        if (host(SC_RALT)) return true;
        return !host(SC_LCTRL) && !host(SC_RCTRL) && !host(SC_LALT);
    }

    bool only_left_alt() const
    {
        Mods want;
        want.set(mod_index(SC_LALT));
        return host_mods_ == want;
    }

    /// What the Router should hold when the module owns nothing: the host's
    /// modifiers, minus AltGr (and, while AltGr is held, the Ctrl that comes
    /// with it on Windows) when the layout is on.
    Mods base_mods() const
    {
        Mods m = host_mods_;
        if (layout_ && host(SC_RALT)) {
            m.reset(mod_index(SC_RALT));
            m.reset(mod_index(SC_LCTRL));
            m.reset(mod_index(SC_RCTRL));
        }
        return m;
    }

    bool owned() const
    {
        if (!queue_.empty()) return true;
        return std::any_of(held_.begin(), held_.end(),
                           [](const Held& h) { return h.kind == Kind::Typed && h.sc != 0; });
    }

    std::vector<Held>::iterator find(uint32_t phys)
    {
        return std::find_if(held_.begin(), held_.end(),
                            [phys](const Held& h) { return h.phys == phys; });
    }

    /// Bring the modifiers held on the Router to `want`: releases first.
    void apply_mods(const Mods& want)
    {
        for (std::size_t i = 0; i < out_mods_.size(); ++i) {
            if (out_mods_.test(i) && !want.test(i)) {
                out_mods_.reset(i);
                emit_(SC_LCTRL + static_cast<int>(i), false);
            }
        }
        for (std::size_t i = 0; i < out_mods_.size(); ++i) {
            if (!out_mods_.test(i) && want.test(i)) {
                out_mods_.set(i);
                emit_(SC_LCTRL + static_cast<int>(i), true);
            }
        }
    }

    void start_typed(Held& h, const Combo& c)
    {
        if (c.extended || !queue_.empty()) {
            h.sc = 0;
            held_.push_back(h);
            enqueue(c);
            return;
        }
        h.sc = c.key;
        h.caps = c.caps;
        h.symbol = c.symbol;
        held_.push_back(h);
        apply_mods(mods_for(c.caps, c.symbol));
        emit_(c.key, true);
    }

    void enqueue(const Combo& c)
    {
        if (queue_.size() >= QUEUE_SIZE) return;
        const bool first = queue_.empty();
        queue_.push_back(c);
        if (!first) return;
        // A sequence starts: our typed keys stop being held on the Router (they
        // stay held on the host) so they cannot join the sequence's chords.
        for (Held& h : held_) {
            if (h.kind == Kind::Typed && h.sc != 0) {
                emit_(h.sc, false);
                h.sc = 0;
            }
        }
        step_ = c.extended ? 0 : 2;
        step_left_ = STEP_FRAMES;
        apply_step();
        shown_ = idle();
    }

    bool idle() const { return !output_idle_ || output_idle_(); }

    /// Steps: 0 E mode (Caps + Symbol Shift), 1 nothing, 2 the key with its
    /// shifts, 3 nothing.
    void apply_step()
    {
        const Combo& c = queue_.front();
        switch (step_) {
        case 0: apply_mods(mods_for(true, true)); break;
        case 1: apply_mods(Mods{}); break;
        case 2: apply_mods(mods_for(c.caps, c.symbol)); emit_(c.key, true); break;
        default: emit_(c.key, false); apply_mods(Mods{}); break;
        }
    }

    /// The shifts of the last typed key still held, else the host's.
    void restore_shifts()
    {
        if (!queue_.empty()) return;   // the running sequence owns them
        const Held* last = nullptr;
        for (const Held& h : held_) {
            if (h.kind != Kind::Typed || h.sc == 0) continue;
            if (!last || h.order > last->order) last = &h;
        }
        apply_mods(last ? mods_for(last->caps, last->symbol) : base_mods());
    }

    void release_everything()
    {
        for (const Held& h : held_)
            if (h.sc != 0) emit_(h.sc, false);
        if (!queue_.empty() && step_ == 2) emit_(queue_.front().key, false);
        apply_mods(Mods{});
        reset();
    }

    Emit emit_;
    ScriptOwns script_owns_;
    std::function<bool()> output_idle_;
    bool shown_ = true;   ///< the Router had applied everything at the last sample
    bool layout_ = false;
    bool alt_digit_ = false;
    Mods host_mods_;
    Mods out_mods_;
    std::vector<Held> held_;
    std::deque<Combo> queue_;
    int step_ = 0;
    int step_left_ = 0;
    unsigned long order_ = 0;
};

} // namespace host_layout
