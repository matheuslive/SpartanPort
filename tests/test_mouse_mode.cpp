// How the game window holds the mouse (spartanport_mouse_mode.h, window.cpp).
#include <cassert>
#include <cstdio>
#include "spartanport_mouse_mode.h"

using SpartanMouse::Decide;
using SpartanMouse::Inputs;

static bool Same(SpartanMouse::Mode m, bool relative, bool grab, bool cursor) {
    return m.relative == relative && m.grab == grab && m.cursor == cursor;
}

int main() {
    // Default (cursor hidden): relative mode while the game has the mouse, as before.
    Inputs in{.show_cursor = false, .keep_in_window = true, .paused = false, .released = false,
              .menu_open = false, .focused = true};
    assert(Same(Decide(in), true, false, false));
    in.paused = true;  // the pause key lets go and shows the system cursor
    assert(Same(Decide(in), false, false, true));
    in.paused = false;
    in.released = in.menu_open = true;  // the menu draws its own cursor
    assert(Same(Decide(in), false, false, false));
    in.released = in.menu_open = false;
    in.focused = false;
    assert(Same(Decide(in), false, false, false));
    in.focused = true;
    in.keep_in_window = false;
    assert(Same(Decide(in), false, false, false));

    // "Show mouse cursor": never relative; the visible pointer is confined to the window instead.
    in = {.show_cursor = true, .keep_in_window = true, .paused = false, .released = false,
          .menu_open = false, .focused = true};
    assert(Same(Decide(in), false, true, true));
    in.keep_in_window = false;  // free to leave the window, still visible
    assert(Same(Decide(in), false, false, true));
    in.keep_in_window = true;
    in.paused = true;
    assert(Same(Decide(in), false, false, true));
    in.paused = false;
    in.focused = false;
    assert(Same(Decide(in), false, false, true));
    in.focused = true;
    in.released = true;  // the save list or a text dialog: let go, cursor still shown
    assert(Same(Decide(in), false, false, true));
    in.menu_open = true;  // the menu: only its own cursor
    assert(Same(Decide(in), false, false, false));
    std::puts("PASS: mouse mode");
}
