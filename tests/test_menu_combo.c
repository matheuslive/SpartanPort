/* The menu's Select+Start shortcut and what the game sees of Back and Start (spartanport_menu_combo.h,
 * runtime_pad.c). */
#include <assert.h>
#include <stdio.h>
#include "gpu/shim/spartanport_menu_combo.h"

enum { MS = 1000 };
static SpartanComboFilter f;
static bool out[2];

/* One read of the pad at time t (ms): Back and Start held; returns what the game sees. */
static void step(bool back, bool start, unsigned t) {
    const bool held[2] = {back, start};
    spartan_combo_filter(&f, true, held, (uint64_t)t * MS, out);
}
static bool seen(bool back, bool start) { return out[0] == back && out[1] == start; }

int main(void) {
    assert(spartan_menu_combo_r3_l2(SPARTAN_MENU_COMBO_R3_L2) && !spartan_menu_combo_select_start(SPARTAN_MENU_COMBO_R3_L2));
    assert(!spartan_menu_combo_r3_l2(SPARTAN_MENU_COMBO_SELECT_START) && spartan_menu_combo_select_start(SPARTAN_MENU_COMBO_SELECT_START));
    assert(spartan_menu_combo_r3_l2(SPARTAN_MENU_COMBO_EITHER) && spartan_menu_combo_select_start(SPARTAN_MENU_COMBO_EITHER));

    /* Off (R3+L2 only): the buttons go straight through. */
    const bool both[2] = {true, true};
    spartan_combo_filter(&f, false, both, 0, out);
    assert(seen(true, true));
    const bool none[2] = {false, false};
    spartan_combo_filter(&f, false, none, MS, out);
    assert(seen(false, false));

    /* Back, then Start 40 ms later: the shortcut; the game sees neither, until both are released. */
    step(true, false, 1000);   assert(seen(false, false));
    step(true, false, 1020);   assert(seen(false, false));
    step(true, true, 1040);    assert(seen(false, false));
    step(true, true, 2600);    assert(seen(false, false));  /* held long (Wolf closes the game) */
    step(false, true, 2610);   assert(seen(false, false));
    step(false, false, 2620);  assert(seen(false, false));

    /* Start held alone: the game gets it once the wait is over (the pause, 0.1 s later). */
    step(false, true, 3000);   assert(seen(false, false));
    step(false, true, 3099);   assert(seen(false, false));
    step(false, true, 3100);   assert(seen(false, true));
    step(false, true, 3500);   assert(seen(false, true));
    step(false, false, 3510);  assert(seen(false, false));

    /* A quick tap of Back: the game still sees it, for the tap time after the release. */
    step(true, false, 4000);   assert(seen(false, false));
    step(true, false, 4030);   assert(seen(false, false));
    step(false, false, 4040);  assert(seen(true, false));
    step(false, false, 4100);  assert(seen(true, false));
    step(false, false, 4120);  assert(seen(false, false));

    /* Start already reaching the game, then Back: the shortcut takes both from then on. */
    step(false, true, 5000);
    step(false, true, 5200);   assert(seen(false, true));
    step(true, true, 5210);    assert(seen(false, false));
    step(true, false, 5220);   assert(seen(false, false));  /* hidden until released */
    step(false, false, 5230);  assert(seen(false, false));

    /* The menu took the input while Back was waiting: no stray tap once it closes. */
    step(true, false, 6000);   assert(seen(false, false));
    spartan_combo_filter_capture(&f);
    step(true, false, 6500);   assert(seen(false, false));  /* still held after the menu closed */
    step(false, false, 6510);  assert(seen(false, false));
    step(false, false, 6600);  assert(seen(false, false));
    step(false, true, 7000);   assert(seen(false, false));  /* and back to normal */
    step(false, true, 7100);   assert(seen(false, true));
    step(false, false, 7110);
    puts("PASS: menu combo");
    return 0;
}
