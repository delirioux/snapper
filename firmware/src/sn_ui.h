/* SPDX-License-Identifier: GPL-3.0-only */
/* SNAPPER screen and controls: Snapper's pages on Felucca's drawing (gfx.c: canvas, fonts, palettes, icons), in
 * Tuna's frame: the header, four knob cards, the panel, the footer (Daurade's layout). Compiled after gfx.c in one unit, as Felucca's
 * ui*.c are (the firmware's unity build, or a host test).
 *
 * The controls
 *   SELECT     tempo                                  MASTER     volume (Felucca's)
 *   PRESETS    the style (the list shows while turning; playing, it comes in on the next bar)
 *   ALGORITHM  a new song in the same style (on the next bar)
 *   KNOB 1..4  the four cards of the page (PLAY: BUILD DRAMA ENERGY ROOM)
 *   PLAY       start (with the hush, FORM on) / stop
 *   REC        BREAK-IN: tapped in the hush or the rise, the tom fill and the full band on the downbeat; held there,
 *              the tension holds until let go; in the full band a fill; in a drama, back in
 *   SEQ        DRAMA: from the full band, on the next bar; it comes back a KEY UP higher
 *   keys       KEYS, live, snapped to the scale. Music only, never settings
 *   OCT- OCT+  the keys' octave
 *   HOME       PLAY; held: MENU (colour, look)
 *   FX         SOUND, again: TONE           LFO   KEYS (the riff)    SCL   KEY
 *   ARP        GROOVE                       EDIT  DRUMS              GLO   MIX, again: SPACE
 *   ENV        FORM, again: KEYS            SAVE  SETS (eight sets on the knobs) */
#ifndef SN_UI_H
#define SN_UI_H

#include <stdbool.h>
#include <stdint.h>
#include "sn_groove.h"

enum { SNU_K_MASTER, SNU_K_SELECT, SNU_K_PRESETS, SNU_K_ALGO, SNU_K1, SNU_K2, SNU_K3, SNU_K4, SNU_KNOBS };
enum {
    SNU_B_FX, SNU_B_SCL, SNU_B_ENV, SNU_B_LFO, SNU_B_EDIT, SNU_B_GLO,
    SNU_B_HOME, SNU_B_SAVE, SNU_B_ARP, SNU_B_SEQ, SNU_B_PLAY, SNU_B_REC,
    SNU_B_OCTDN, SNU_B_OCTUP, SNU_BUTTONS
};
enum { SNU_PG_PLAY, SNU_PG_SOUND, SNU_PG_TONE, SNU_PG_KEY, SNU_PG_GROOVE, SNU_PG_DRUMS, SNU_PG_MIX,
       SNU_PG_SPACE, SNU_PG_FORM, SNU_PG_KEYS, SNU_PG_SETS, SNU_PG_MENU, SNU_PAGES };
#define SNU_KEY_LO 53                      /* F3: the FM-1's lowest key */
#define SNU_KEY_HI 79                      /* G5 */

typedef struct {
    sn_groove *g;
    int page;
    int hot, hot_ms;                      /* the knob just turned (its card in the accent); 9: SELECT */
    int browse_ms;                        /* the style list over the panel (PRESETS) */
    char msg[32];
    int msg_ms;
    int home_ms;
    bool home_down;
    uint32_t keys_down;                   /* bit (note - SNU_KEY_LO) */
    int oct;                              /* OCT- OCT+: the keys' octave, -2 .. 2 */
    int key_sent[27];                     /* the note each key sent (with the octave it was pressed in) */
    /* SETS */
    sn_settings sets[SN_SETS];
    uint8_t set_used;                     /* bit per set */
    int set_sel;
    int clear_turns, clear_ms;            /* CLEAR: three turns right within a second */
    bool sets_changed;                    /* the glue saves the sets while stopped */
    uint32_t frame;
    int batt;                             /* 0..3 bars, 4 = USB power */
    bool changed;                         /* a setting changed: the glue applies it (sound, mix) and saves it later */
    uint32_t sig_head, sig_card[4], sig_foot;
    bool force;
} sn_ui;

void snu_init(sn_ui *u, sn_groove *g);
void snu_knob(sn_ui *u, int knob, int delta);
void snu_button(sn_ui *u, int button, bool down);
void snu_key(sn_ui *u, int note, bool down);
void snu_tick(sn_ui *u, int ms);           /* timers; every frame */
void snu_draw(sn_ui *u);                   /* what changed; the panel every frame */
void snu_draw_all(sn_ui *u);
const char *snu_page_name(int page);

#endif
