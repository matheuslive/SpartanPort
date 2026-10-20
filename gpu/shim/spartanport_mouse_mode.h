// spartanport: how the game window holds the mouse (window.cpp), kept apart so it can be tested.
#pragma once

namespace SpartanMouse {

struct Inputs {
    bool show_cursor;    ///< menu "Show mouse cursor": the system cursor stays visible while playing
    bool keep_in_window; ///< menu "Keep mouse in window" (mouse_lock)
    bool paused;         ///< let go by the pause key
    bool released;       ///< the in-game menu, the save list or a text dialog has the mouse
    bool menu_open;      ///< the in-game menu is open (it draws its own cursor)
    bool focused;        ///< the window has the keyboard focus
};

struct Mode {
    bool relative; ///< SDL relative mode: hidden cursor, no edges
    bool grab;     ///< the pointer is confined to the window, still visible
    bool cursor;   ///< the system cursor shows
};

// The game holds the mouse while "Keep mouse in window" is on, the window has the focus and nothing
// took it back. It hides the pointer in relative mode, unless "Show mouse cursor" is on: then the
// visible pointer is only confined to the window (it stops at the edges, and so does the L2 aim's
// movement). With the menu open its own cursor is drawn instead of the system one.
constexpr Mode Decide(const Inputs& in) {
    const bool hold = in.keep_in_window && in.focused && !in.paused && !in.released;
    if (in.show_cursor) {
        return {false, hold, !in.menu_open};
    }
    return {hold, false, in.paused};
}

} // namespace SpartanMouse
