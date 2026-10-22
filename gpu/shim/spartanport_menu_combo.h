/* spartanport: the gamepad shortcut that opens the in-game menu (spartanport.ini menu_combo), kept
 * apart so it can be tested. C, for runtime_pad.c; the overlay (C++) uses the mode helpers. */
#ifndef SPARTANPORT_MENU_COMBO_H
#define SPARTANPORT_MENU_COMBO_H
#include <stdbool.h>
#include <stdint.h>

/* menu_combo: R3+L2 (default), Select+Start (Back/View/Share + Start/Menu/Options), or either. */
enum { SPARTAN_MENU_COMBO_R3_L2 = 0, SPARTAN_MENU_COMBO_SELECT_START = 1, SPARTAN_MENU_COMBO_EITHER = 2 };

static inline bool spartan_menu_combo_r3_l2(int mode) { return mode != SPARTAN_MENU_COMBO_SELECT_START; }
static inline bool spartan_menu_combo_select_start(int mode) {
    return mode == SPARTAN_MENU_COMBO_SELECT_START || mode == SPARTAN_MENU_COMBO_EITHER;
}

/* In the game Start is Options (pause) and Back the right touchpad click (pause too), so with
 * Select+Start on, the first of the two would reach the game a few ms before the second closes
 * the shortcut. The filter holds Back or Start pressed alone for SPARTAN_COMBO_WAIT_US: the other
 * one coming in that time is the shortcut, and both stay hidden from the game until released; held
 * longer, the game gets it (the pause comes that much later); let go sooner, the game still gets
 * the tap, for SPARTAN_COMBO_TAP_US. */
enum { SPARTAN_COMBO_WAIT_US = 100000, SPARTAN_COMBO_TAP_US = 80000 };
enum { SPARTAN_COMBO_IDLE, SPARTAN_COMBO_WAIT, SPARTAN_COMBO_SHOWN, SPARTAN_COMBO_HIDDEN, SPARTAN_COMBO_TAP };
typedef struct {
    int state[2];      /* per button: 0 Back, 1 Start */
    uint64_t time[2];  /* WAIT: when pressed; TAP: until when the game sees it */
} SpartanComboFilter;

/* Something else took the input (the menu opened): forget what was waiting, and buttons still held
 * stay hidden until released, so closing the menu sends the game no stray tap. */
static inline void spartan_combo_filter_capture(SpartanComboFilter *f) {
    for (int i = 0; i < 2; ++i) f->state[i] = SPARTAN_COMBO_HIDDEN;
}

/* held: Back and Start as the gamepad has them; out: as the game should see them. Off (the mode
 * without Select+Start): out = held. */
static inline void spartan_combo_filter(SpartanComboFilter *f, bool enabled, const bool held[2], uint64_t now,
                                        bool out[2]) {
    if (!enabled) {
        for (int i = 0; i < 2; ++i) { f->state[i] = SPARTAN_COMBO_IDLE; out[i] = held[i]; }
        return;
    }
    if (held[0] && held[1]) f->state[0] = f->state[1] = SPARTAN_COMBO_HIDDEN; /* the shortcut */
    for (int i = 0; i < 2; ++i) {
        if (f->state[i] == SPARTAN_COMBO_TAP && now >= f->time[i]) f->state[i] = SPARTAN_COMBO_IDLE;
        switch (f->state[i]) {
        case SPARTAN_COMBO_TAP:
            break;
        case SPARTAN_COMBO_IDLE:
            if (held[i]) { f->state[i] = SPARTAN_COMBO_WAIT; f->time[i] = now; }
            break;
        case SPARTAN_COMBO_WAIT:
            if (!held[i]) { f->state[i] = SPARTAN_COMBO_TAP; f->time[i] = now + SPARTAN_COMBO_TAP_US; }
            else if (now - f->time[i] >= SPARTAN_COMBO_WAIT_US) f->state[i] = SPARTAN_COMBO_SHOWN;
            break;
        case SPARTAN_COMBO_SHOWN:
        case SPARTAN_COMBO_HIDDEN:
            if (!held[i]) f->state[i] = SPARTAN_COMBO_IDLE;
            break;
        }
        out[i] = f->state[i] == SPARTAN_COMBO_SHOWN || f->state[i] == SPARTAN_COMBO_TAP;
    }
}

#endif
