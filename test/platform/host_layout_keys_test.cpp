// hungarian-ui fork - typing by the character the host keyboard layout produces
// (src/platform/host_layout_keys.h, "Windows keyboard layout").
//
// No hardware counterpart: the real Next reads a US PS/2 keymap by position
// (keymaps.vhd); this is host input translation. The oracles are the Spectrum
// keyboard legends (which key and shift type each character, the E-mode
// symbols), the Fuse fork's hungarian_keyboard.c the module follows, the
// Hungarian Windows layout (Shift+4 = '!', Shift+2 = '"', AltGr+N = '}', dead
// AltGr+1 = '~'), and the decisions recorded in doc/hu/jnext-atadas.md
// ("Windowsos magyar billentyűzet"). HLAY-40 drives the output through the real
// host_key_latch::Router to the matrix a frame sees; HLAY-42..48 the frontend's
// side (the setting, AltGr, the event a Qt key gives, the mouse release, and the
// real wiring function, host_key_wiring.h wire_layout_keys).
//
// Run: ./build/test/host_layout_keys_test

#include "platform/host_layout_keys.h"
#include "platform/host_key_latch.h"
#include "platform/host_key_wiring.h"
#include "platform/pointer_capture.h"

#include <bitset>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "../row_id.h"

namespace {

int g_pass = 0, g_fail = 0;

void check(const char* id, const char* desc, bool cond, const std::string& detail = {})
{
    report_row_id(id);
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::printf("  FAIL %s: %s", id, desc);
        if (!detail.empty()) std::printf(" [%s]", detail.c_str());
        std::printf("\n");
    }
}

using namespace host_layout;

constexpr int SC_RETURN = 40;   // SDL_SCANCODE_RETURN
constexpr int SC_SPACE = 44;    // SDL_SCANCODE_SPACE
constexpr int SC_GRAVE = 53;    // SDL_SCANCODE_GRAVE

std::string name(int sc)
{
    if (sc >= SC_A && sc <= SC_Z) return std::string(1, static_cast<char>('A' + sc - SC_A));
    if (sc >= SC_1 && sc <= SC_9) return std::string(1, static_cast<char>('1' + sc - SC_1));
    switch (sc) {
    case SC_0: return "0";
    case SC_LCTRL: return "LCTRL";
    case SC_LSHIFT: return "LSHIFT";
    case SC_LALT: return "LALT";
    case SC_LGUI: return "LGUI";
    case SC_RCTRL: return "RCTRL";
    case SC_RSHIFT: return "RSHIFT";
    case SC_RALT: return "RALT";
    case SC_RGUI: return "RGUI";
    case SC_RETURN: return "RETURN";
    case SC_SPACE: return "SPACE";
    case SC_GRAVE: return "GRAVE";
    default: return "#" + std::to_string(sc);
    }
}

/// The module wired to a recorder of what it sends the Router.
struct Rig {
    std::vector<std::pair<int, bool>> out;
    Translator t{[this](int sc, bool pressed) { out.emplace_back(sc, pressed); }};

    explicit Rig(bool layout = true, bool alt_digit = true)
    {
        t.set_layout(layout);
        t.set_alt_digit(alt_digit);
        out.clear();
    }

    /// "+LCTRL +1" since the last take().
    std::string take()
    {
        std::string s;
        for (const auto& [sc, pressed] : out) {
            if (!s.empty()) s += ' ';
            s += (pressed ? '+' : '-') + name(sc);
        }
        out.clear();
        return s;
    }

    bool press(uint32_t phys, int sc, char32_t ch = 0, bool dead = false)
    {
        return t.on_press(KeyEvent{phys, sc, ch, dead});
    }
    bool release(uint32_t phys, int sc, char32_t ch = 0, bool dead = false)
    {
        return t.on_release(KeyEvent{phys, sc, ch, dead});
    }
    // Modifier keys, by their Windows scan codes (the module does not care).
    void mod(int sc, bool pressed)
    {
        const uint32_t phys = sc == SC_RALT ? 0x138 : sc == SC_LALT ? 0x38
                            : sc == SC_LCTRL ? 0x1d : sc == SC_LSHIFT ? 0x2a : 0x100 + sc;
        if (pressed) t.on_press(KeyEvent{phys, sc, 0, false});
        else         t.on_release(KeyEvent{phys, sc, 0, false});
    }
};

// Windows scan codes of the keys the rows use (Hungarian layout legends).
constexpr uint32_t K_A = 0x1e, K_E = 0x12, K_N = 0x31, K_V = 0x2f, K_4 = 0x05, K_2 = 0x03,
                   K_5 = 0x06, K_1 = 0x02, K_9 = 0x0a, K_U_ACUTE = 0x1b, K_O_DACUTE = 0x1a,
                   K_SPACE = 0x39, K_RETURN = 0x1c, K_0 = 0x29, K_F = 0x21, K_E_ACUTE = 0x27;

void transparent_rows()
{
    Rig r(false, false);
    const bool a = r.press(K_A, letter('a'), U'a');
    const bool b = r.release(K_A, letter('a'), U'a');
    r.mod(SC_LSHIFT, true);
    r.press(K_4, digit(1), U'!');   // qt_key_to_sdl: Key_Exclam -> SC_1
    r.release(K_4, digit(1), U'!');
    r.mod(SC_LSHIFT, false);
    const bool unknown = r.press(0x56, 0, U'<');
    check("HLAY-01", "both switches off: a plain forward of the classic scancode, as without the module",
          a && b && r.take() == "+A -A +LSHIFT +1 -1 -LSHIFT" && !unknown);
}

void letter_digit_rows()
{
    {
        Rig r;
        r.press(K_A, letter('a'), U'a');
        const std::string down = r.take();
        r.release(K_A, letter('a'), U'a');
        check("HLAY-02", "a lower-case letter is its own key, nothing else",
              down == "+A" && r.take() == "-A", down);
    }
    {
        Rig r;
        r.mod(SC_LSHIFT, true);
        r.press(K_A, letter('a'), U'A');
        r.release(K_A, letter('a'), U'a');
        const std::string held = r.take();
        r.mod(SC_LSHIFT, false);
        check("HLAY-03", "an upper-case letter is Caps Shift + the letter; the host Shift already is it",
              held == "+LSHIFT +A -A" && r.take() == "-LSHIFT", held);
    }
    {
        Rig r;
        r.press(K_0, SC_GRAVE, U'0');   // the Hungarian 0 sits left of 1 (US grave = TRUE VIDEO)
        r.release(K_0, SC_GRAVE, U'0');
        r.press(K_5, digit(5), U'5');
        r.release(K_5, digit(5), U'5');
        check("HLAY-04", "a digit is its own key, whatever physical key typed it (Hungarian 0 left of 1)",
              r.take() == "+0 -0 +5 -5");
    }
}

void symbol_rows()
{
    {
        Rig r;
        r.mod(SC_LSHIFT, true);
        r.take();
        r.press(K_4, digit(1), U'!');
        const std::string down = r.take();
        r.release(K_4, digit(1), U'!');
        const std::string up = r.take();
        r.mod(SC_LSHIFT, false);
        check("HLAY-05", "Hungarian Shift+4 '!': host Shift is let go, Symbol Shift + 1, then Shift back",
              down == "-LSHIFT +LCTRL +1" && up == "-1 -LCTRL +LSHIFT" && r.take() == "-LSHIFT",
              down + " | " + up);
    }
    {
        Rig r;
        r.mod(SC_LSHIFT, true);
        r.take();
        r.press(K_2, digit(2), U'"');
        check("HLAY-06", "Hungarian Shift+2 '\"' is Symbol Shift + P, not Caps Shift + 2",
              r.take() == "-LSHIFT +LCTRL +P");
    }
    {
        struct Case { char32_t ch; int key; };
        const Case cases[] = {
            { U'!', digit(1) }, { U'@', digit(2) }, { U'#', digit(3) }, { U'$', digit(4) },
            { U'%', digit(5) }, { U'&', digit(6) }, { U'\'', digit(7) }, { U'(', digit(8) },
            { U')', digit(9) }, { U'_', digit(0) }, { U'<', letter('r') }, { U'>', letter('t') },
            { U';', letter('o') }, { U'"', letter('p') }, { U'^', letter('h') }, { U'-', letter('j') },
            { U'+', letter('k') }, { U'=', letter('l') }, { U':', letter('z') }, { 0xa3, letter('x') },
            { U'?', letter('c') }, { U'/', letter('v') }, { U'*', letter('b') }, { U',', letter('n') },
            { U'.', letter('m') }, { 0xd7, letter('b') }, { 0xf7, letter('v') },
        };
        std::string bad;
        for (const Case& c : cases) {
            Combo m;
            if (!map_character(c.ch, m) || m.key != c.key || !m.symbol || m.caps || m.extended)
                bad += " U+" + std::to_string(static_cast<unsigned>(c.ch));
        }
        check("HLAY-07", "every Symbol Shift legend of the 48K keyboard (plus the pound, times, divide signs)",
              bad.empty(), bad);
    }
    {
        struct Case { char32_t ch; int key; };
        const Case cases[] = {
            { U'[', letter('y') }, { U']', letter('u') }, { U'{', letter('f') }, { U'}', letter('g') },
            { U'\\', letter('d') }, { U'|', letter('s') }, { U'~', letter('a') }, { 0xa9, letter('p') },
        };
        std::string bad;
        for (const Case& c : cases) {
            Combo m;
            if (!map_character(c.ch, m) || m.key != c.key || !m.symbol || m.caps || !m.extended)
                bad += " U+" + std::to_string(static_cast<unsigned>(c.ch));
        }
        check("HLAY-08", "the E-mode symbols [ ] { } \\ | ~ (c) are Symbol Shift + their key in E mode",
              bad.empty(), bad);
    }
    {
        Combo e, O, u, A;
        const bool ok = map_character(0xe9, e) && map_character(0x150, O) &&
                        map_character(0x171, u) && map_character(0xc1, A);
        check("HLAY-09", "accented vowels type the plain vowel (e acute, O double acute, u double acute, A acute)",
              ok && e.key == letter('e') && !e.caps && O.key == letter('o') && O.caps &&
              u.key == letter('u') && !u.caps && A.key == letter('a') && A.caps);
    }
}

void swallow_rows()
{
    {
        Rig r;
        const bool p = r.press(K_E, letter('e'), 0x20ac);   // AltGr+U on some layouts: euro
        const bool q = r.release(K_E, letter('e'), 0x20ac);
        const bool p2 = r.press(K_0, SC_GRAVE, 0xa7);        // Hungarian Shift+0: section sign
        r.release(K_0, SC_GRAVE, 0xa7);
        check("HLAY-10", "a character with no Spectrum key (euro, section sign) is swallowed, press and release",
              p && q && p2 && r.take().empty());
    }
    {
        Rig r;
        r.mod(SC_RALT, true);
        r.take();
        const bool p = r.press(K_1, digit(1), 0, true);   // AltGr+1: dead tilde
        const bool q = r.release(K_1, digit(1), 0, true);
        const std::string dead = r.take();
        r.mod(SC_RALT, false);
        r.take();
        r.press(K_SPACE, SC_SPACE, U'~');                 // the next key brings the tilde
        check("HLAY-11", "a dead key is swallowed (not the classic 1); the key that brings its character types it",
              p && q && dead.empty() && r.take() == "+LCTRL +LSHIFT" && r.t.sequence_running(), dead);
        r.release(K_SPACE, SC_SPACE, U' ');
    }
}

void sequence_rows()
{
    {
        Rig r;
        r.press(K_F, letter('f'), U'[');   // AltGr+F on the Hungarian layout, here with no AltGr for brevity
        const std::string s0 = r.take();
        r.t.on_frames(STEP_FRAMES - 1);
        const std::string early = r.take();
        r.t.on_frames(1);
        const std::string s1 = r.take();
        r.t.on_frames(STEP_FRAMES);
        const std::string s2 = r.take();
        r.t.on_frames(STEP_FRAMES);
        const std::string s3 = r.take();
        r.t.on_frames(STEP_FRAMES);
        const std::string end = r.take();
        check("HLAY-12", "'[': E mode, release, Symbol Shift + Y, release, each step STEP_FRAMES frames",
              s0 == "+LCTRL +LSHIFT" && early.empty() && s1 == "-LCTRL -LSHIFT" &&
              s2 == "+LCTRL +Y" && s3 == "-Y -LCTRL" && end.empty() && !r.t.sequence_running(),
              s0 + " | " + s1 + " | " + s2 + " | " + s3 + " | " + end);
        r.release(K_F, letter('f'), U'f');
        check("HLAY-13", "the key's release after its sequence sends nothing", r.take().empty());
    }
    {
        Rig r;
        r.press(K_F, letter('f'), U'[');
        r.take();
        r.t.on_frames(0);
        const std::string paused = r.take();
        r.t.on_frames(4 * STEP_FRAMES);
        check("HLAY-14", "no emulated frame, no step (paused machine); one long tick runs the whole sequence",
              paused.empty() && r.take() == "-LCTRL -LSHIFT +LCTRL +Y -Y -LCTRL" && !r.t.sequence_running());
    }
    {
        Rig r;
        bool idle = false;
        r.t.set_output_idle([&idle] { return idle; });
        r.press(K_F, letter('f'), U'[');
        r.take();
        r.t.on_frames(4 * STEP_FRAMES);
        const std::string busy = r.take();
        idle = true;
        r.t.on_frames(STEP_FRAMES);   // caught up during this tick: not yet shown to a frame
        const std::string first = r.take();
        r.t.on_frames(STEP_FRAMES);
        check("HLAY-37", "a step is counted only on frames that ran after the Router had caught up",
              busy.empty() && first.empty() && r.take() == "-LCTRL -LSHIFT", busy + " | " + first);
    }
    {
        Rig r;
        r.press(K_F, letter('f'), U'[');
        r.press(K_A, letter('a'), U'a');   // typed while the sequence runs
        const std::string start = r.take();
        r.t.on_frames(4 * STEP_FRAMES);
        const std::string ext = r.take();
        r.t.on_frames(STEP_FRAMES);
        const std::string a = r.take();
        r.t.on_frames(STEP_FRAMES);
        check("HLAY-15", "a key typed during a sequence waits for it, in typing order, without E mode",
              start == "+LCTRL +LSHIFT" && ext == "-LCTRL -LSHIFT +LCTRL +Y -Y -LCTRL +A" &&
              a == "-A" && !r.t.sequence_running(), start + " | " + ext + " | " + a);
    }
    {
        Rig r;
        r.press(K_A, letter('a'), U'a');
        r.take();
        r.press(K_F, letter('f'), U'[');
        const std::string start = r.take();
        r.release(K_A, letter('a'), U'a');
        check("HLAY-16", "a sequence first lets go of the typed keys still held; their release then sends nothing",
              start == "-A +LCTRL +LSHIFT" && r.take().empty(), start);
    }
    {
        Rig r;
        r.press(K_F, letter('f'), U'[');
        r.take();
        r.mod(SC_LSHIFT, true);
        const std::string during = r.take();
        r.t.on_frames(4 * STEP_FRAMES);
        check("HLAY-17", "a host modifier pressed during a sequence is applied when it ends",
              during.empty() && r.take() == "-LCTRL -LSHIFT +LCTRL +Y -Y -LCTRL +LSHIFT");
    }
}

void altgr_rows()
{
    {
        // Windows: AltGr arrives as left Ctrl, then right Alt.
        Rig r;
        r.mod(SC_LCTRL, true);
        const std::string ctrl = r.take();
        r.mod(SC_RALT, true);
        const std::string altgr = r.take();
        r.press(K_N, letter('n'), U'}');
        r.t.on_frames(4 * STEP_FRAMES);
        r.release(K_N, letter('n'), U'}');
        const std::string seq = r.take();
        r.mod(SC_LCTRL, false);
        r.mod(SC_RALT, false);
        const std::string up = r.take();
        const std::string all = ctrl + " " + altgr + " " + seq + " " + up;
        check("HLAY-18", "Windows AltGr+N '}': the Ctrl that comes with AltGr is withdrawn, no Alt ever reaches the Router (G is not GRAPH)",
              ctrl == "+LCTRL" && altgr == "-LCTRL" &&
              seq == "+LCTRL +LSHIFT -LCTRL -LSHIFT +LCTRL +G -G -LCTRL" && up.empty() &&
              all.find("ALT") == std::string::npos, all);
    }
    {
        // X11: AltGr is the right Alt alone.
        Rig r;
        r.mod(SC_RALT, true);
        r.press(K_V, letter('v'), U'@');
        const std::string down = r.take();
        r.release(K_V, letter('v'), U'v');
        r.mod(SC_RALT, false);
        check("HLAY-19", "AltGr+V '@' is Symbol Shift + 2, with no Alt on the Router",
              down == "+LCTRL +2" && r.take() == "-2 -LCTRL", down);
    }
    {
        Rig r;
        r.mod(SC_RALT, true);
        r.press(K_U_ACUTE, 0, 0xd7);
        const std::string times = r.take();
        r.release(K_U_ACUTE, 0, 0xd7);
        r.take();
        r.press(K_O_DACUTE, 0, 0xf7);
        check("HLAY-20", "AltGr+u-acute 'x' and AltGr+o-double-acute '/' (keys with no classic scancode) type * and /",
              times == "+LCTRL +B" && r.take() == "+LCTRL +V", times);
    }
    {
        Rig r(false, true);   // layout off: AltGr is an Alt like before
        r.mod(SC_RALT, true);
        check("HLAY-21", "with the layout off the right Alt reaches the Router as before",
              r.take() == "+RALT");
    }
}

void classic_rows()
{
    {
        Rig r;
        r.mod(SC_LCTRL, true);
        r.press(K_A, letter('a'), 0x01);   // Ctrl+A: a control character
        r.release(K_A, letter('a'), 0x01);
        r.press(K_E, letter('e'), U'e');   // a layout that still reports the letter
        r.release(K_E, letter('e'), U'e');
        r.mod(SC_LCTRL, false);
        check("HLAY-22", "Ctrl + letter stays Symbol Shift + letter (the classic path)",
              r.take() == "+LCTRL +A -A +E -E -LCTRL");
    }
    {
        Rig r;
        r.mod(SC_LALT, true);
        r.press(K_E, letter('e'), U'e');
        r.release(K_E, letter('e'), U'e');
        r.mod(SC_LALT, false);
        check("HLAY-23", "left Alt + E stays the guest's EDIT chord (the classic path)",
              r.take() == "+LALT +E -E -LALT");
    }
    {
        Rig r;
        r.press(K_SPACE, SC_SPACE, U' ');
        r.release(K_SPACE, SC_SPACE, U' ');
        r.press(K_RETURN, SC_RETURN, U'\r');
        r.release(K_RETURN, SC_RETURN, U'\r');
        const bool unmapped = r.press(0x3b, 0, 0);   // F1-like: no scancode, no character
        check("HLAY-24", "Space, Enter: the classic path; a key with neither scancode nor character is not handled",
              r.take() == "+SPACE -SPACE +RETURN -RETURN" && !unmapped);
    }
}

void alt_digit_rows()
{
    {
        Rig r;
        r.mod(SC_LALT, true);
        r.take();
        r.press(K_5, digit(5), U'5');
        const std::string down = r.take();
        r.release(K_5, digit(5), U'5');
        const std::string up = r.take();
        check("HLAY-25", "left Alt + 5: Caps Shift + 5 (cursor left), the Alt withdrawn for it and put back",
              down == "-LALT +LSHIFT +5" && up == "-5 -LSHIFT +LALT", down + " | " + up);
    }
    {
        Rig r;
        r.t.set_script_owns([](int) { return true; });
        r.mod(SC_LALT, true);
        r.take();
        r.press(K_1, digit(1), U'1');
        r.release(K_1, digit(1), U'1');
        r.press(0x09, digit(8), U'8');
        r.release(0x09, digit(8), U'8');
        const std::string one = r.take();
        r.press(K_9, digit(9), U'9');
        check("HLAY-26", "a listening script keeps Alt+1..8 (the Router's chord); Alt+9 is always Caps Shift + 9",
              one == "+1 -1 +8 -8" && r.take() == "-LALT +LSHIFT +9", one);
    }
    {
        Rig r;
        bool owns = false;
        r.t.set_script_owns([&owns](int) { return owns; });
        r.mod(SC_LALT, true);
        r.take();
        r.press(K_2, digit(2), U'2');
        owns = true;                         // a script loaded between press and release
        r.release(K_2, digit(2), U'2');
        const std::string a = r.take();
        r.press(K_2, digit(2), U'2');
        owns = false;                        // ... and unloaded
        r.release(K_2, digit(2), U'2');
        check("HLAY-27", "the Alt+digit decision is taken on the press; the release follows it",
              a == "-LALT +LSHIFT +2 -2 -LSHIFT +LALT" && r.take() == "+2 -2", a);
    }
    {
        Rig r(true, false);
        r.mod(SC_LALT, true);
        r.press(K_5, digit(5), U'5');
        check("HLAY-28", "with Alt+digit off, Alt + 5 is the classic path", r.take() == "+LALT +5");
    }
    {
        Rig r;
        r.mod(SC_LSHIFT, true);
        r.mod(SC_LALT, true);
        r.press(K_5, digit(5), U'%');
        check("HLAY-29", "Shift + Alt + digit is not the Alt+digit chord (only the left Alt alone is)",
              r.take() == "+LSHIFT +LALT +5");
    }
    {
        Rig r(false, true);   // Alt+digit works with the layout off too
        r.mod(SC_LALT, true);
        r.take();
        r.press(K_0, digit(0), U'0');
        check("HLAY-30", "Alt + 0 is Caps Shift + 0 (DELETE) with the layout off as well",
              r.take() == "-LALT +LSHIFT +0");
    }
}

void bookkeeping_rows()
{
    {
        Rig r;
        r.mod(SC_LSHIFT, true);
        r.press(K_4, digit(1), U'!');
        r.mod(SC_LSHIFT, false);          // Shift let go first: recorded, nothing sent
        const std::string mid = r.take();
        r.release(K_4, digit(4), U'4');   // the release reports another key and character
        check("HLAY-31", "a release lets go of what its PHYSICAL key's press asserted, Shift let go first",
              mid == "+LSHIFT -LSHIFT +LCTRL +1" && r.take() == "-1 -LCTRL", mid);
    }
    {
        Rig r;
        r.press(K_A, letter('a'), U'a');
        r.take();
        const bool rep = r.press(K_A, letter('a'), U'a');
        r.mod(SC_LSHIFT, true);
        r.take();
        r.mod(SC_LSHIFT, true);
        check("HLAY-32", "a host autorepeat of a held key or modifier sends nothing", rep && r.take().empty());
    }
    {
        Rig r;
        r.press(K_A, letter('a'), U'a');
        r.mod(SC_LSHIFT, true);
        r.press(K_4, digit(1), U'!');
        r.take();
        r.release(K_4, digit(1), U'!');
        const std::string back = r.take();
        r.release(K_A, letter('a'), U'a');
        check("HLAY-33", "rollover: the shifts follow the newest typed key, and the one still held on release",
              back == "-1 -LCTRL" && r.take() == "-A +LSHIFT", back);
    }
    {
        Rig r;
        r.mod(SC_LSHIFT, true);
        r.press(K_4, digit(1), U'!');
        r.press(K_F, letter('f'), U'[');
        r.take();
        r.t.reset();
        const bool clean = r.out.empty() && r.t.held_count() == 0 && !r.t.sequence_running();
        r.t.on_frames(4 * STEP_FRAMES);
        check("HLAY-34", "reset (keyboard lost) forgets everything and sends nothing: the Router lets go itself",
              clean && r.take().empty());
    }
    {
        Rig r;
        r.mod(SC_LSHIFT, true);
        r.press(K_4, digit(1), U'!');
        r.take();
        r.t.set_layout(false);
        check("HLAY-35", "turning the layout off lets go of what the module holds on the Router",
              r.take() == "-1 -LCTRL" && r.t.held_count() == 0);
    }
    {
        Rig r;
        r.press(K_E_ACUTE, 0, 0xe9);   // the Hungarian e-acute key has no classic scancode
        check("HLAY-36", "a key the classic path drops (e acute) types its plain vowel", r.take() == "+E");
    }
}

// ---------------------------------------------------------------------------
// Through the real Router: what a frame's matrix sees.

struct Matrix {
    std::bitset<host_key_latch::MAX_KEYS> down;
    void set_key(int sc, bool pressed) { down.set(static_cast<std::size_t>(sc), pressed); }
};
using MatrixRouter = host_key_latch::Router<Matrix, int>;

void router_rows()
{
    Matrix m;
    MatrixRouter router;
    router.attach(m);
    Translator t([&router](int sc, bool pressed) { router.on_host_key(sc, pressed); });
    t.set_layout(true);
    t.set_alt_digit(true);
    t.set_output_idle([&router] { return router.pending() == 0; });

    auto frame = [&](std::vector<std::string>& seen) {
        std::string s;
        for (std::size_t i = 0; i < m.down.size(); ++i)
            if (m.down.test(i)) { if (!s.empty()) s += '+'; s += name(static_cast<int>(i)); }
        seen.push_back(s);
        router.on_tick_end(1);
        t.on_frames(1);
    };

    std::vector<std::string> seen;
    // Shift+4 typed quickly: Shift and 4 in one inter-frame gap.
    t.on_press(KeyEvent{0x2a, SC_LSHIFT, 0, false});
    t.on_press(KeyEvent{K_4, digit(1), U'!', false});
    for (int i = 0; i < 3; ++i) frame(seen);
    t.on_release(KeyEvent{K_4, digit(1), U'!', false});
    t.on_release(KeyEvent{0x2a, SC_LSHIFT, 0, false});
    for (int i = 0; i < 3; ++i) frame(seen);
    bool bang = false, wrong = false;
    for (const auto& s : seen) {
        if (s == "LCTRL+1" || s == "1+LCTRL") bang = true;
        if (s.find("LSHIFT") != std::string::npos && s.find('1') != std::string::npos) wrong = true;
    }
    std::string all;
    for (const auto& s : seen) all += "[" + s + "]";
    check("HLAY-40", "through the real Router: Shift+4 shows Symbol Shift + 1 to a frame, never Caps Shift + 1",
          bang && !wrong && m.down.none(), all);

    seen.clear();
    t.on_press(KeyEvent{K_F, letter('f'), U'[', false});
    for (int i = 0; i < 12 * STEP_FRAMES; ++i) frame(seen);
    t.on_release(KeyEvent{K_F, letter('f'), U'f', false});
    for (int i = 0; i < 2; ++i) frame(seen);
    int emode = 0, sym_y = 0;
    for (const auto& s : seen) {
        if (s == "LCTRL+LSHIFT" || s == "LSHIFT+LCTRL") ++emode;
        if (s == "Y+LCTRL" || s == "LCTRL+Y") ++sym_y;
    }
    all.clear();
    for (const auto& s : seen) all += "[" + s + "]";
    check("HLAY-41", "through the real Router, behind a backlog: '[' shows E mode, then Symbol Shift + Y, each for STEP_FRAMES frames",
          emode >= STEP_FRAMES && sym_y >= STEP_FRAMES && m.down.none(), all);
}


// --- the frontend's side -------------------------------------------------

constexpr int QK_A = 0x41;                // Qt::Key_A
constexpr int QK_DEAD_TILDE = 0x01001253;  // Qt::Key_Dead_Tilde
constexpr int QK_DEAD_HORN = 0x01001262;   // Qt::Key_Dead_Horn (the last one Qt 5 has)
constexpr int QK_CONTROL = 0x01000021;    // Qt::Key_Control

void frontend_rows()
{
    check("HLAY-42", "input.windows_layout: auto follows the interface language, on and off are fixed",
          layout_on(Mode::Auto, true) && !layout_on(Mode::Auto, false) && layout_on(Mode::On, false)
              && layout_on(Mode::On, true) && !layout_on(Mode::Off, true) && !layout_on(Mode::Off, false));

    {
        const bool win = is_right_alt(QT_KEY_ALT, 0x138, RIGHT_ALT_SCAN_WINDOWS)
                      && !is_right_alt(QT_KEY_ALT, 0x38, RIGHT_ALT_SCAN_WINDOWS);
        const bool xkb = is_right_alt(QT_KEY_ALT, 108, RIGHT_ALT_SCAN_XKB)
                      && !is_right_alt(QT_KEY_ALT, 64, RIGHT_ALT_SCAN_XKB);
        const bool altgr = is_right_alt(QT_KEY_ALTGR, 0, 0) && is_right_alt(QT_KEY_ALTGR, 108, RIGHT_ALT_SCAN_XKB);
        const bool unknown = !is_right_alt(QT_KEY_ALT, 0, 0) && !is_right_alt(QT_KEY_ALT, 0x138, 0)
                          && !is_right_alt(QK_A, 0x138, RIGHT_ALT_SCAN_WINDOWS);
        check("HLAY-43", "the right Alt: Key_AltGr, or Key_Alt with the platform's right-Alt scan code (Windows 0x138, xkb 108); never without one",
              win && xkb && altgr && unknown);
    }

    {
        const KeyEvent a = make_event(QK_A, 0x1e, 0x41, U'a', letter('a'), true, RIGHT_ALT_SCAN_WINDOWS);
        const KeyEvent b = make_event(QK_A, 0, 0x41, U'a', letter('a'), true, RIGHT_ALT_SCAN_WINDOWS);
        const KeyEvent c = make_event(QK_A, 0, 0, U'a', letter('a'), true, RIGHT_ALT_SCAN_WINDOWS);
        const bool phys = a.phys == 0x1e && b.phys == 0x41 && c.phys == static_cast<uint32_t>(QK_A)
                       && a.sc == letter('a') && a.ch == U'a' && !a.dead;
        const bool dead = make_event(QK_DEAD_TILDE, 0x02, 0, 0, 0, true, 0).dead
                       && make_event(QT_KEY_DEAD_FIRST, 0x02, 0, 0, 0, true, 0).dead
                       && make_event(QK_DEAD_HORN, 0x02, 0, 0, 0, true, 0).dead
                       && !make_event(QT_KEY_DEAD_LAST + 1, 0x02, 0, 0, 0, true, 0).dead
                       && !make_event(QT_KEY_ALTGR, 0x02, 0, 0, 0, true, 0).dead;
        const bool on = make_event(QT_KEY_ALT, 0x138, 0, 0, SC_LALT, true, RIGHT_ALT_SCAN_WINDOWS).sc == SC_RALT
                     && make_event(QT_KEY_ALTGR, 108, 0, 0, 0, true, RIGHT_ALT_SCAN_XKB).sc == SC_RALT
                     && make_event(QT_KEY_ALT, 0x38, 0, 0, SC_LALT, true, RIGHT_ALT_SCAN_WINDOWS).sc == SC_LALT;
        const bool off = make_event(QT_KEY_ALT, 0x138, 0, 0, SC_LALT, false, RIGHT_ALT_SCAN_WINDOWS).sc == SC_LALT
                      && make_event(QT_KEY_ALTGR, 108, 0, 0, 0, false, RIGHT_ALT_SCAN_XKB).sc == 0;
        check("HLAY-44", "a Qt key event: physical id scan code, else virtual key, else key; Key_Dead_* is dead; AltGr is SC_RALT only with the layout on",
              phys && dead && on && off);
    }

    {
        using pointer_capture::HostKey;
        // One key event as the window hands it to the release filter.
        struct Win {
            pointer_capture::ReleaseFilter f;
            AltSides sides;
            bool layout;
            bool ctrl = false, alt = false;   // the toolkit's modifier state
            explicit Win(bool l) : layout(l) { f.set_method(pointer_capture::ReleaseMethod::CtrlAlt); }
            bool key(int qt_key, uint32_t scan, bool pressed)
            {
                sides.on_key(qt_key, scan, RIGHT_ALT_SCAN_WINDOWS, pressed);
                const HostKey k = qt_key == QK_CONTROL ? HostKey::Ctrl
                                : qt_key == QT_KEY_ALT ? HostKey::Alt : HostKey::Other;
                const ReleaseView v = release_view(layout, k, is_right_alt(qt_key, scan, RIGHT_ALT_SCAN_WINDOWS),
                                                   alt, sides);
                const bool released = f.on_key(v.key, pressed, ctrl, v.alt_held, true).release;
                if (qt_key == QK_CONTROL) ctrl = pressed;
                if (qt_key == QT_KEY_ALT) alt = pressed;
                return released;
            }
        };
        // AltGr on Windows: a left Ctrl, then the right Alt with Ctrl held.
        Win on(true);
        const bool altgr_on = on.key(QK_CONTROL, 0x1d, true) || on.key(QT_KEY_ALT, 0x138, true);
        on.key(QT_KEY_ALT, 0x138, false); on.key(QK_CONTROL, 0x1d, false);
        const bool left_on = on.key(QT_KEY_ALT, 0x38, true) ? false : on.key(QK_CONTROL, 0x1d, true);
        on.key(QK_CONTROL, 0x1d, false); on.key(QT_KEY_ALT, 0x38, false);
        const bool ctrl_first_on = !on.key(QK_CONTROL, 0x1d, true) && on.key(QT_KEY_ALT, 0x38, true);
        on.key(QT_KEY_ALT, 0x38, false); on.key(QK_CONTROL, 0x1d, false);
        Win off(false);
        const bool altgr_off = !off.key(QK_CONTROL, 0x1d, true) && off.key(QT_KEY_ALT, 0x138, true);
        check("HLAY-45", "mouse release, layout on: AltGr (Ctrl + right Alt) keeps the mouse, Ctrl + left Alt in either order lets go; layout off: AltGr lets go, as before",
              !altgr_on && left_on && ctrl_first_on && altgr_off && !on.sides.left() && !on.sides.right());
    }
}

/// The window, as wire_layout_keys sees it.
struct FakeWindow {
    std::function<bool(const KeyEvent&, bool)> key;
    std::function<void()> lost;
    void set_layout_key_callback(std::function<bool(const KeyEvent&, bool)> f) { key = std::move(f); }
    void set_keyboard_lost_callback(std::function<void()> f) { lost = std::move(f); }
};
struct FakeBackend {
    bool listening = false;
    bool has_host_subscribers() const { return listening; }
};

void wiring_rows()
{
    Matrix m;
    MatrixRouter router;
    router.attach(m);
    std::vector<int> script;
    router.set_script_key_callback([&script](int n) { script.push_back(n); });
    Translator t;
    t.set_layout(true);
    t.set_alt_digit(true);
    FakeWindow w;
    FakeBackend backend;
    wire_layout_keys(w, t, router, backend);

    // The matrix each of `n` frames sees, as "LCTRL+1".
    auto frames = [&](int n) {
        std::vector<std::string> seen;
        for (int i = 0; i < n; ++i) {
            std::string f;
            for (std::size_t k = 0; k < m.down.size(); ++k)
                if (m.down.test(k)) { if (!f.empty()) f += '+'; f += name(static_cast<int>(k)); }
            seen.push_back(f);
            router.on_tick_end(1);
            t.on_frames(1);
        }
        return seen;
    };
    auto any = [](const std::vector<std::string>& seen, const std::string& want) {
        for (const auto& f : seen) if (f == want) return true;
        return false;
    };
    auto joined = [](const std::vector<std::string>& seen) {
        std::string all;
        for (const auto& f : seen) all += "[" + f + "]";
        return all;
    };

    // Shift+4 ('!') through the window's callback: Symbol Shift + 1 on the matrix.
    const bool took = w.key(KeyEvent{0x2a, SC_LSHIFT, 0, false}, true)
                   && w.key(KeyEvent{K_4, digit(4), U'!', false}, true);
    auto seen = frames(12);
    w.key(KeyEvent{K_4, digit(4), U'!', false}, false);
    w.key(KeyEvent{0x2a, SC_LSHIFT, 0, false}, false);
    auto after = frames(12);
    bool caps = false;   // Caps Shift + 1 at any frame
    for (const auto& f : seen) caps = caps || f == "1+LSHIFT" || f == "1+LCTRL+LSHIFT";
    // An unmapped key the module has nothing for: the window's default handling.
    const bool unmapped = !w.key(KeyEvent{0x5b, 0, 0, false}, true);
    w.key(KeyEvent{0x5b, 0, 0, false}, false);
    check("HLAY-46", "wire_layout_keys: the window's keys reach the Translator and its output the Router (Shift+4 = Symbol Shift + 1); an unmapped key is not taken",
          took && any(seen, "1+LCTRL") && !caps && after.back().empty() && unmapped,
          joined(seen) + joined(after));

    // '[' starts a sequence; the window loses the keyboard in the middle of it.
    w.key(KeyEvent{K_F, letter('f'), U'[', false}, true);
    seen = frames(4);
    const bool running = t.sequence_running() && any(seen, "LCTRL+LSHIFT");
    w.lost();
    after = frames(12);
    check("HLAY-47", "wire_layout_keys: keyboard loss clears the Translator first, then the Router lets go of everything",
          running && !t.sequence_running() && t.held_count() == 0 && after.back().empty(),
          joined(seen) + joined(after));
    w.key(KeyEvent{K_F, letter('f'), U'f', false}, false);   // its key-up went elsewhere; harmless here

    // Left Alt + 1: Caps Shift + 1 (EDIT) with no script listening; the script's
    // host key (the Router's chord) while one listens.
    w.key(KeyEvent{0x38, SC_LALT, 0, false}, true);
    w.key(KeyEvent{K_1, digit(1), U'1', false}, true);
    seen = frames(12);
    w.key(KeyEvent{K_1, digit(1), U'1', false}, false);
    auto mid = frames(12);
    const bool edit = any(seen, "1+LSHIFT") && script.empty();
    backend.listening = true;
    w.key(KeyEvent{K_1, digit(1), U'1', false}, true);
    after = frames(12);
    w.key(KeyEvent{K_1, digit(1), U'1', false}, false);
    w.key(KeyEvent{0x38, SC_LALT, 0, false}, false);
    auto end = frames(12);
    bool digit_seen = false;
    for (const auto& f : after) digit_seen = digit_seen || f.find('1') != std::string::npos;
    const bool chord = script.size() == 1 && script[0] == 1 && !digit_seen;
    check("HLAY-48", "wire_layout_keys: Alt+1 is Caps Shift + 1 while no script listens for host events, the script's key while one does",
          edit && chord && end.back().empty(),
          joined(seen) + joined(mid) + joined(after) + joined(end));
}

} // namespace

int main()
{
    std::printf("Host layout keys (hungarian-ui)\n");
    transparent_rows();
    letter_digit_rows();
    symbol_rows();
    swallow_rows();
    sequence_rows();
    altgr_rows();
    classic_rows();
    alt_digit_rows();
    bookkeeping_rows();
    router_rows();
    frontend_rows();
    wiring_rows();
    std::printf("Total: %4d  Passed: %4d  Failed: %4d  Skipped: %4d\n",
                g_pass + g_fail, g_pass, g_fail, 0);
    return g_fail ? 1 : 0;
}
