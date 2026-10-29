/* SPDX-License-Identifier: GPL-3.0-only */
/* SNAPPER's screen and controls on the host: firmware/src/sn_ui.c drawn by Felucca's gfx.c into a 240 x 240 frame,
 * the groove running (its notes counted, no sound), the controls driven as a player would, every page in every
 * palette written out, and the layout lint (from Tuna's and Flyfish's UI tests): every text inside its canvas, not
 * cut, not ellipsised unless it is a name, no two texts of one canvas overlapping.
 *   tests/sn_ui_test.sh [OUTDIR]     (exit 0: all passed; PNGs and a GIF of PLAY under OUTDIR) */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, checks;
#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        checks++;                                                          \
        if (!(cond)) {                                                     \
            fails++;                                                       \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                    \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
        }                                                                  \
    } while (0)

/* ---- the layout lint: the ink boxes of one canvas, checked when it is blitted -------------------------- */
typedef struct { int x0, y0, x1, y1, flags; char s[24]; } box;
static box boxes[256];
static int nbox;
static const char *lint_where = "";
static int lint_findings;
static void hook_text(int x0, int y0, int x1, int y1, const char *s, int flags)
{
    if (nbox < 256) {
        box *b = &boxes[nbox++];
        b->x0 = x0; b->y0 = y0; b->x1 = x1; b->y1 = y1; b->flags = flags;
        strncpy(b->s, s, sizeof b->s - 1);
        b->s[sizeof b->s - 1] = 0;
    }
}
#define GFX_HOOK_TEXT(x0, y0, x1, y1, s, flags) hook_text((x0), (y0), (x1), (y1), (s), (flags))
#define GFX_HOOK_BLIT(x, y, r0) ((void)0)
#define GFX_HOOK_BEGIN() (nbox = 0)
#define GFX_HOOK_PIXELS(n) ((void)0)

static uint16_t fb[240 * 240];
static void lint_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    for (int i = 0; i < nbox; ++i) {
        box *b = &boxes[i];
        if (b->x1 <= b->x0)
            continue;
        bool name = (b->flags & 8) != 0;            /* free text (a name, a message) may be ellipsised */
        if (b->x0 < 0 || b->y0 < 0 || b->x1 > (int)w || b->y1 > (int)h) {
            printf("LINT %s: \"%s\" leaves its canvas (%d,%d..%d,%d of %ux%u at %u,%u)\n", lint_where, b->s, b->x0,
                   b->y0, b->x1, b->y1, w, h, x, y);
            lint_findings++;
        }
        if ((int)x + b->x1 > 240 || (int)y + b->y1 > 240) {
            printf("LINT %s: \"%s\" off the screen\n", lint_where, b->s);
            lint_findings++;
        }
        if ((b->flags & 1) && !name) {
            printf("LINT %s: \"%s\" ellipsised\n", lint_where, b->s);
            lint_findings++;
        }
        for (int j = i + 1; j < nbox; ++j) {
            box *c = &boxes[j];
            if (c->x1 <= c->x0)
                continue;
            if (b->x0 < c->x1 && c->x0 < b->x1 && b->y0 < c->y1 && c->y0 < b->y1) {
                printf("LINT %s: \"%s\" overlaps \"%s\" (%d,%d..%d,%d / %d,%d..%d,%d)\n", lint_where, b->s, c->s, b->x0,
                       b->y0, b->x1, b->y1, c->x0, c->y0, c->x1, c->y1);
                lint_findings++;
            }
        }
    }
    nbox = 0;
}
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    lint_blit(x, y, w, h);
    for (uint32_t j = 0; j < h && y + j < 240; j++)
        for (uint32_t i = 0; i < w && x + i < 240; i++)
            fb[(y + j) * 240 + x + i] = p[j * w + i];
}
#define main hostsim_main
#include "hostsim.c"                               /* (core.h, the matrix: sn_sounds.h's track settings) */
#undef main
#include "gfx.c"
#include "sn_groove.h"
#include "sn_sounds.h"
#include "sn_ui.c"

static const char *outdir;
static int frames_written;
static void write_ppm(const char *name)
{
    if (!outdir) return;
    char path[512];
    snprintf(path, sizeof path, "%s/ppm/%s.ppm", outdir, name);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6 240 240 255\n");
    for (int i = 0; i < 240 * 240; i++) {
        uint16_t c = (uint16_t)((fb[i] >> 8) | (fb[i] << 8));
        uint8_t px[3] = {(uint8_t)((c >> 11) << 3 | (c >> 13)), (uint8_t)(((c >> 5) & 63) << 2 | ((c >> 9) & 3)),
                         (uint8_t)((c & 31) << 3 | ((c >> 2) & 7))};
        fwrite(px, 1, 3, f);
    }
    fclose(f);
    frames_written++;
}

static int notes_on, notes_off;
static int part_on[SN_PARTS];
static void sound(void *ctx, const sn_event *e)
{
    (void)ctx;
    if (e->on) { notes_on++; part_on[e->part % SN_PARTS]++; }
    else notes_off++;
}

static sn_groove g;
static sn_ui ui;
static void run_ms(int ms)
{
    for (int t = 0; t < ms; t += 16) {
        sn_process(&g, 44100 * 16 / 1000);
        snu_tick(&ui, 16);
        snu_draw(&ui);
    }
}
static void press(int b) { snu_button(&ui, b, true); snu_button(&ui, b, false); }
static void shot(const char *name)
{
    lint_where = name;
    snu_draw_all(&ui);
    write_ppm(name);
}
static void all_palettes(const char *page)
{
    int keep = g.s.palette, keep_st = g.s.look;
    for (int p = 0; p < (int)NPALETTES; ++p)
        for (int st = 0; st < 2; ++st) {
            char name[64];
            g.s.palette = (uint8_t)p;
            g.s.look = (uint8_t)st;
            snu_apply_style(&ui);
            snprintf(name, sizeof name, "%s%s_%s", st ? "LINE_" : "", UI_PALETTES[p].name, page);
            shot(name);
        }
    g.s.palette = (uint8_t)keep;
    g.s.look = (uint8_t)keep_st;
    snu_apply_style(&ui);
}
static void bars(int n)                            /* play until n bars on */
{
    int b = g.bar + n;
    for (int k = 0; k < 4000 && g.bar < b; ++k)
        run_ms(16);
}

int main(int argc, char **argv)
{
    outdir = argc > 1 ? argv[1] : 0;
    sn_init(&g, 44100.0f);
    sn_set_sound(&g, sound, 0);
    snu_init(&ui, &g);
    CHECK(g.s.palette == UI_GATE_INDEX && !strcmp(UI_PALETTES[UI_GATE_INDEX].name, "GATE"), "GATE by default");
    CHECK(g.s.style == SN_ST_SLOWBURN, "SLOWBURN first");
    g.s.build = SN_BUILD_8;                        /* (a short build for the test) */
    shot("01_play_stopped");
    all_palettes("play_stopped");
    press(SNU_B_PLAY);
    CHECK(g.playing && g.sec == SN_SEC_HUSH, "PLAY plays, from the hush");
    bars(5);
    CHECK(g.sec == SN_SEC_RISE, "the rise at bar 4 (%s at %d)", sn_sec_name(g.sec), g.bar);
    run_ms(900);
    shot("02_play_rise");
    all_palettes("play_rise");
    CHECK(part_on[SN_PADS] && part_on[SN_DRUMS], "the drone and the machine (%d %d %d %d)", part_on[0], part_on[1],
          part_on[2], part_on[3]);
    /* the knobs of PLAY */
    snu_knob(&ui, SNU_K1, 3);
    CHECK(g.s.build == SN_BUILD_HOLD, "BUILD: HOLD (%d)", g.s.build);
    snu_knob(&ui, SNU_K3, -4);
    CHECK(g.s.energy == 50, "ENERGY -4 steps of 5 (%d)", g.s.energy);
    snu_knob(&ui, SNU_K3, 4);
    bars(6);
    CHECK(g.sec == SN_SEC_RISE, "HOLD: the rise holds (%s)", sn_sec_name(g.sec));
    /* REC held: HOLD; let go early in a bar: the break-in */
    while (g.step != 1) run_ms(4);
    snu_button(&ui, SNU_B_REC, true);
    CHECK(g.rec_held, "REC held: the tension holds");
    run_ms(48);
    shot("03_hold");
    snu_button(&ui, SNU_B_REC, false);
    CHECK(g.sec == SN_SEC_IN || g.in_next, "let go: the break-in (%s)", sn_sec_name(g.sec));
    while (g.sec != SN_SEC_FULL) run_ms(16);
    bars(2);
    CHECK(part_on[SN_KEYS] > 0, "the full band: the keys (%d)", part_on[SN_KEYS]);
    shot("04_play_full");
    all_palettes("play_full");
    /* the PLAY animation: a bar, a frame every 48 ms */
    for (int f = 0; f < 40; ++f) {
        char nm[32];
        run_ms(48);
        snprintf(nm, sizeof nm, "anim_%03d", f);
        lint_where = nm;
        write_ppm(nm);
    }
    /* DRAMA: SEQ, from the next bar */
    press(SNU_B_SEQ);
    CHECK(g.drama_next, "SEQ: a drama next bar");
    bars(1);
    CHECK(g.sec == SN_SEC_DRAMA, "SEQ: the drama (%s)", sn_sec_name(g.sec));
    run_ms(1700);
    shot("05_drama");
    all_palettes("drama");
    while (g.sec != SN_SEC_FULL2) run_ms(16);
    CHECK(g.tr == g.s.keyup, "after the drama: KEY UP +%d (+%d)", g.s.keyup, g.tr);
    run_ms(1700);
    shot("06_keyup");
    /* the styles: PRESETS queues one for the next bar */
    snu_knob(&ui, SNU_K_PRESETS, 3);
    CHECK(g.next_style == SN_ST_STADIUM, "PRESETS +3: STADIUM queued (%d)", g.next_style);
    shot("07_styles");
    all_palettes("styles");
    bars(1);
    CHECK(g.s.style == SN_ST_STADIUM && g.s.bpm == 128, "STADIUM on the bar line (%d, %d BPM)", g.s.style, g.s.bpm);
    run_ms(2000);
    /* SOUND, TONE */
    press(SNU_B_FX);
    CHECK(ui.page == SNU_PG_SOUND, "FX: SOUND");
    bars(1);
    shot("08_sound");
    all_palettes("sound");
    int pad0 = g.s.snd[SN_PADS];
    snu_knob(&ui, SNU_K1, 1);
    CHECK(g.s.snd[SN_PADS] == (pad0 + 1) % SN_SOUNDS, "K1: the next PADS sound (%d)", g.s.snd[SN_PADS]);
    snu_knob(&ui, SNU_K1, -1);
    {                                              /* every sound's name on its card and the panel: the lint */
        uint8_t keep[4];
        memcpy(keep, g.s.snd, sizeof keep);
        for (int i = 0; i < SN_SOUNDS; ++i) {
            char nm[32];
            g.s.snd[0] = g.s.snd[1] = g.s.snd[2] = (uint8_t)i;
            g.s.snd[3] = (uint8_t)(i % SN_KIT_COUNT);
            snprintf(nm, sizeof nm, "sound_%02d", i);
            shot(nm);
        }
        memcpy(g.s.snd, keep, sizeof keep);
    }
    press(SNU_B_FX);
    CHECK(ui.page == SNU_PG_TONE, "FX again: TONE");
    shot("09_tone");
    all_palettes("tone");
    /* KEY */
    press(SNU_B_SCL);
    CHECK(ui.page == SNU_PG_KEY, "SCL: KEY");
    shot("10_key");
    all_palettes("key");
    int key0 = g.s.key;
    snu_knob(&ui, SNU_K1, 1);
    CHECK(g.s.key == (key0 + 1) % 12 && g.loop[0].root == (key0 + 1) % 12, "KEY +1: the loop follows (%d)",
          g.loop[0].root);
    snu_knob(&ui, SNU_K1, -1);
    /* GROOVE, DRUMS */
    press(SNU_B_ARP);
    CHECK(ui.page == SNU_PG_GROOVE, "ARP: GROOVE");
    bars(1);
    shot("11_groove");
    all_palettes("groove");
    int bl0 = g.s.bassline;
    snu_knob(&ui, SNU_K3, 1);
    CHECK(g.s.bassline == (bl0 + 1 < SN_BL_COUNT ? bl0 + 1 : bl0), "GROOVE BASS: the next (%d)", g.s.bassline);
    snu_knob(&ui, SNU_K3, -1);
    press(SNU_B_EDIT);
    CHECK(ui.page == SNU_PG_DRUMS, "EDIT: DRUMS");
    shot("12_drums");
    all_palettes("drums");
    g.s.snd[3] = SN_KIT_BOX;
    shot("12b_drums_machine");
    g.s.snd[3] = SN_KIT_BIG;
    /* MIX, SPACE */
    press(SNU_B_GLO);
    CHECK(ui.page == SNU_PG_MIX, "GLO: MIX");
    shot("13_mix");
    all_palettes("mix");
    press(SNU_B_GLO);
    CHECK(ui.page == SNU_PG_SPACE, "GLO again: SPACE");
    shot("14_space");
    all_palettes("space");
    /* FORM, KEYS */
    press(SNU_B_ENV);
    CHECK(ui.page == SNU_PG_FORM, "ENV: FORM");
    shot("15_form");
    all_palettes("form");
    g.s.form = 0;
    shot("15b_form_off");
    g.s.form = 1;
    press(SNU_B_ENV);
    CHECK(ui.page == SNU_PG_KEYS, "ENV again: KEYS");
    while (!sn_in_full(&g)) run_ms(16);
    run_ms(400);
    shot("16_keys");
    all_palettes("keys");
    snu_knob(&ui, SNU_K4, 1);
    CHECK(g.s.repeat == 4, "REPEAT: 4 BARS (%d)", g.s.repeat);
    snu_knob(&ui, SNU_K4, -1);
    press(SNU_B_LFO);
    CHECK(ui.page == SNU_PG_KEYS, "LFO: KEYS");
    /* NIGHTDRIVE's arpeggio on the KEYS page */
    sn_queue_style(&g, SN_ST_NIGHTDRIVE);
    bars(1);
    shot("16b_keys_arp");
    /* SETS: store into 3, load it back */
    sn_queue_style(&g, SN_ST_SLOWBURN);
    bars(1);
    press(SNU_B_SAVE);
    CHECK(ui.page == SNU_PG_SETS, "SAVE: SETS");
    snu_knob(&ui, SNU_K1, 2);
    snu_knob(&ui, SNU_K3, 1);
    CHECK((ui.set_used & 4u) && ui.sets[2].style == SN_ST_SLOWBURN, "STORE into set 3");
    ui.sets[0] = g.s; ui.sets[0].style = SN_ST_BRASSHIT; ui.set_used |= 1u;
    ui.sets[4] = g.s; ui.sets[4].style = SN_ST_RHYTHMBOX; ui.set_used |= 16u;
    ui.sets[6] = g.s; ui.sets[6].style = SN_ST_NIGHTDRIVE; ui.set_used |= 64u;
    shot("17_sets");
    all_palettes("sets");
    snu_knob(&ui, SNU_K1, -2);
    snu_knob(&ui, SNU_K2, 1);
    CHECK(g.s.style == SN_ST_BRASSHIT, "LOAD set 1: BRASSHIT at once");
    snu_knob(&ui, SNU_K4, 1); snu_knob(&ui, SNU_K4, 1);
    CHECK(ui.set_used & 1u, "CLEAR twice: not yet");
    snu_knob(&ui, SNU_K4, 1);
    CHECK(!(ui.set_used & 1u), "CLEAR three times: cleared");
    /* the keys: KEYS, snapped to the scale */
    int before = part_on[SN_KEYS];
    snu_key(&ui, 54, true);
    CHECK(part_on[SN_KEYS] == before + 1, "a key plays KEYS");
    snu_key(&ui, 54, false);
    /* MENU: HOME held */
    snu_button(&ui, SNU_B_HOME, true);
    run_ms(700);
    snu_button(&ui, SNU_B_HOME, false);
    CHECK(ui.page == SNU_PG_MENU, "HOME held: MENU");
    shot("18_menu");
    all_palettes("menu");
    press(SNU_B_HOME);
    CHECK(ui.page == SNU_PG_PLAY, "HOME: PLAY");
    /* ALGORITHM: a new song on the next bar */
    int song = g.s.song;
    snu_knob(&ui, SNU_K_ALGO, 1);
    bars(1);
    CHECK(g.s.song == song + 1, "ALGORITHM: the next song (%d)", g.s.song);
    /* stopped again */
    press(SNU_B_PLAY);
    CHECK(!g.playing, "PLAY stops");
    CHECK(g.nheld == 0, "nothing left sounding");
    CHECK(lint_findings == 0, "the layout lint: %d findings", lint_findings);
    printf("sn_ui: %d checks, %d failed, %d frames, %d notes on (pads %d, bass %d, keys %d, drums %d)\n", checks, fails,
           frames_written, notes_on, part_on[0], part_on[1], part_on[2], part_on[3]);
    return fails ? 1 : 0;
}
