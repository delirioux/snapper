/* SPDX-License-Identifier: GPL-3.0-only */
/* SNAPPER's groove engine on its own (firmware/src/sn_groove.c): every style plays, in time, in key, every note ends;
 * the form goes hush, rise, break-in, full, drama, break-in, full (the key up), end and round again; the big kit
 * waits for the break-in and the drum machine makes way for it; REC breaks in (at the bar's half or the next bar's),
 * holds the tension while held, fills in the full band; SEQ brings a drama; a song replays exactly; a style and a
 * song come in on the bar line; the keys play in the scale; the settings survive a bad record.
 *   cc -std=c11 -O2 -Ifirmware/src -o build/host/sn_groove_test tests/sn_groove_test.c firmware/src/sn_groove.c -lm */
#include <stdio.h>
#include <string.h>
#include "sn_groove.h"

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

#define SR 44100.0f
static sn_groove g;
static int on[SN_PARTS], off[SN_PARTS], sounding[SN_PARTS][128], bad_pc, keys_out_of_full, big_in_tense, box_in_full;
static int drum_on[SN_DR_COUNT];
static int crash_at_step0, crash_other;
static uint32_t hash;
static void sound(void *ctx, const sn_event *e)
{
    (void)ctx;
    hash = (hash ^ (uint32_t)(e->on << 24 | e->part << 16 | e->note << 8 | e->vel)) * 16777619u + g.clock;
    if (e->on) {
        on[e->part]++;
        sounding[e->part][e->note]++;
        if (e->part != SN_DRUMS && e->part != SN_KEYS && sn_snap(&g.s, e->note - g.tr) != e->note - g.tr)
            bad_pc++;                              /* (in the scale but for the dominant's third and the approach) */
        if (e->part == SN_KEYS && !g.keys && g.sec != SN_SEC_FULL && g.sec != SN_SEC_FULL2)
            keys_out_of_full++;
        if (e->part == SN_DRUMS) {
            int d = e->note % SN_DR_COUNT;
            drum_on[d]++;
            if ((g.sec == SN_SEC_HUSH || g.sec == SN_SEC_RISE) && d != SN_DR_CRASH && d < SN_DR_MKICK)
                big_in_tense++;
            if ((g.sec == SN_SEC_FULL || g.sec == SN_SEC_FULL2) && d >= SN_DR_MKICK && g.s.style != SN_ST_RHYTHMBOX)
                box_in_full++;
            if (d == SN_DR_CRASH) {
                if (g.step == 0) crash_at_step0++;
                else crash_other++;
            }
        }
    } else {
        off[e->part]++;
        if (sounding[e->part][e->note] > 0) sounding[e->part][e->note]--;
    }
}
static void reset_counts(void)
{
    memset(on, 0, sizeof on);
    memset(off, 0, sizeof off);
    memset(sounding, 0, sizeof sounding);
    memset(drum_on, 0, sizeof drum_on);
    bad_pc = keys_out_of_full = big_in_tense = box_in_full = crash_at_step0 = crash_other = 0;
}
static int still_sounding(int p) { int n = 0; for (int k = 0; k < 128; ++k) n += sounding[p][k]; return n; }
static void run_bars(int n)
{
    int b = g.bar + n;
    while (g.bar < b) sn_process(&g, 32);
}
static void run_to_step(int step)                  /* to the given sixteenth of the bar playing (or the next) */
{
    for (int k = 0; k < 100000 && g.step != step; ++k) sn_process(&g, 16);
}
static void run_to_sec(int sec)
{
    for (int k = 0; k < 400 && g.sec != sec; ++k) run_bars(1);
}
static void fresh(int style)
{
    sn_play(&g, false);
    sn_defaults(&g.s);
    sn_style_apply(&g.s, style);
    sn_rebuild(&g);
    reset_counts();
}

int main(void)
{
    sn_init(&g, SR);
    sn_set_sound(&g, sound, 0);
    /* every style: a whole round of the form and more; its parts play, nothing hangs, in key; the keys wait for the
     * full band, the big kit for the break-in, the drum machine makes way for the big kit */
    for (int st = 0; st < SN_STYLES; ++st) {
        fresh(st);
        g.s.build = SN_BUILD_8;
        sn_play(&g, true);
        run_bars(80);
        sn_play(&g, false);
        for (int p = 0; p < SN_PARTS; ++p) {
            CHECK(on[p] > 0, "%s: part %d plays (%d)", sn_style_name(st), p, on[p]);
            if (p != SN_DRUMS)
                CHECK(!still_sounding(p), "%s: part %d left %d notes sounding", sn_style_name(st), p, still_sounding(p));
        }
        CHECK(bad_pc < on[SN_PADS] / 8 + on[SN_BASS] / 8 + 8, "%s: %d pad and bass notes outside the scale",
              sn_style_name(st), bad_pc);
        CHECK(keys_out_of_full == 0, "%s: the keys wait for the full band (%d notes before)", sn_style_name(st),
              keys_out_of_full);
        CHECK(big_in_tense == 0, "%s: the big kit waits for the break-in (%d hits before)", sn_style_name(st), big_in_tense);
        CHECK(box_in_full == 0, "%s: the drum machine makes way for the big kit (%d hits)", sn_style_name(st), box_in_full);
        CHECK(drum_on[SN_DR_MKICK] + drum_on[SN_DR_MHAT] > 0, "%s: the drum machine plays in the hush", sn_style_name(st));
        CHECK(drum_on[SN_DR_TOML] > 0, "%s: the toms break in", sn_style_name(st));
        CHECK(drum_on[SN_DR_CRASH] > 0 && crash_other == 0, "%s: crashes on downbeats only (%d, %d off it)",
              sn_style_name(st), crash_at_step0, crash_other);
        CHECK(g.loop[0].name[0], "%s: its first chord has a name (%s)", sn_style_name(st), g.loop[0].name);
    }

    /* the form: BUILD 8 (4 + 4), FULL 8, one drama, KEY UP +2 */
    {
        fresh(SN_ST_BIGROOM);
        g.s.build = SN_BUILD_8;
        g.s.full = 8;
        g.s.dramas = 1;
        g.s.keyup = 2;
        g.s.drama = SN_DRAMA_AUTO;
        sn_play(&g, true);
        static const int WANT[] = {SN_SEC_HUSH, SN_SEC_RISE, SN_SEC_IN, SN_SEC_FULL, SN_SEC_DRAMA, SN_SEC_IN2,
                                   SN_SEC_FULL2, SN_SEC_END, SN_SEC_HUSH};
        static const int LEN[] = {4, 4, 1, 8, SN_DRAMA_BARS, 1, 8, SN_END_BARS};
        int k = 0, bars_in = 0, bad = 0, tr_full2 = -1, tr_after = -1;
        run_to_step(0);
        for (int b = 0; b < 40 && k < 8; ++b) {
            if (g.sec != WANT[k]) {
                if (bars_in != LEN[k]) bad++;
                k++;
                bars_in = 0;
                if (g.sec != WANT[k]) { bad += 100; break; }
            }
            if (g.sec == SN_SEC_FULL2) tr_full2 = g.tr;
            bars_in++;
            run_bars(1);
        }
        if (g.sec == SN_SEC_HUSH) tr_after = g.tr;
        CHECK(k == 8 && !bad, "the form in order and length (reached %d, %d off)", k, bad);
        CHECK(tr_full2 == 2, "the second full band KEY UP higher (+%d)", tr_full2);
        CHECK(tr_after == 0, "round again: home (+%d)", tr_after);
        CHECK(g.round == 1, "round again (round %d)", g.round);
    }
    /* DRAMA OFF: no drama in the form */
    {
        fresh(SN_ST_BIGROOM);
        g.s.build = SN_BUILD_8;
        g.s.full = 8;
        g.s.drama = SN_DRAMA_OFF;
        sn_play(&g, true);
        int dramas = 0;
        for (int b = 0; b < 40; ++b) {
            dramas += g.sec == SN_SEC_DRAMA;
            run_bars(1);
        }
        CHECK(dramas == 0, "DRAMA OFF: no drama (%d bars of one)", dramas);
    }
    /* BUILD HOLD: the rise until REC; a tap early in the bar breaks in from its half, the full band on the next bar */
    {
        fresh(SN_ST_SLOWBURN);
        g.s.build = SN_BUILD_HOLD;
        sn_play(&g, true);
        run_bars(30);
        CHECK(g.sec == SN_SEC_RISE, "HOLD: still rising after 30 bars (%s)", sn_sec_name(g.sec));
        run_to_step(2);
        int bar = g.bar;
        reset_counts();
        sn_breakin(&g, true);
        sn_breakin(&g, false);
        CHECK(g.sec == SN_SEC_IN && g.fill_from == 8, "a tap at step 2: the break-in from this bar's half");
        run_to_step(15);
        CHECK(drum_on[SN_DR_TOMH] + drum_on[SN_DR_TOMM] + drum_on[SN_DR_TOML] >= 6, "the fill: toms (%d %d %d)",
              drum_on[SN_DR_TOMH], drum_on[SN_DR_TOMM], drum_on[SN_DR_TOML]);
        run_to_step(1);
        CHECK(g.sec == SN_SEC_FULL && g.bar == bar + 1, "the full band on the next bar (%s, bar +%d)", sn_sec_name(g.sec),
              g.bar - bar);
        CHECK(crash_at_step0 == 1, "a crash on its downbeat (%d)", crash_at_step0);
    }
    /* a tap late in the bar: the break-in on the next bar */
    {
        fresh(SN_ST_SLOWBURN);
        g.s.build = SN_BUILD_HOLD;
        sn_play(&g, true);
        run_bars(6);
        run_to_step(11);
        int bar = g.bar;
        sn_breakin(&g, true);
        sn_breakin(&g, false);
        CHECK(g.sec == SN_SEC_RISE && g.in_next, "a tap at step 11: still the rise, the break-in next bar");
        run_to_step(1);
        CHECK(g.sec == SN_SEC_IN && g.bar == bar + 1, "the break-in bar (%s)", sn_sec_name(g.sec));
        run_bars(1);
        CHECK(g.sec == SN_SEC_FULL, "then the full band (%s)", sn_sec_name(g.sec));
    }
    /* REC held in the rise: the tension holds past its end; let go: in */
    {
        fresh(SN_ST_BIGROOM);
        g.s.build = SN_BUILD_8;
        sn_play(&g, true);
        run_to_sec(SN_SEC_RISE);
        sn_breakin(&g, true);
        run_bars(12);
        CHECK(g.sec == SN_SEC_RISE && g.rec_held, "REC held: still the rise 12 bars on (%s)", sn_sec_name(g.sec));
        run_to_step(3);
        sn_breakin(&g, false);
        CHECK(g.sec == SN_SEC_IN, "let go: the break-in (%s)", sn_sec_name(g.sec));
    }
    /* SEQ in the full band: a drama on the next bar, then the way in and the full band a KEY UP higher; REC in the
     * drama: back in at once */
    {
        fresh(SN_ST_STADIUM);
        g.s.build = SN_BUILD_8;
        g.s.full = 16;
        g.s.keyup = 1;
        sn_play(&g, true);
        run_to_sec(SN_SEC_FULL);
        run_bars(2);
        run_to_step(5);
        sn_drama(&g);
        CHECK(g.sec == SN_SEC_FULL && g.drama_next, "SEQ: the drama waits for the bar line");
        run_to_step(15);
        reset_counts();
        run_to_step(0);
        CHECK(g.sec == SN_SEC_DRAMA, "SEQ: the drama (%s)", sn_sec_name(g.sec));
        run_bars(1);
        CHECK(on[SN_DRUMS] == 0 && on[SN_BASS] == 0 && on[SN_KEYS] == 0 && on[SN_PADS] > 0,
              "the drama's first bars: the pads alone (%d %d %d %d)", on[0], on[1], on[2], on[3]);
        run_to_step(4);
        sn_breakin(&g, true);
        sn_breakin(&g, false);
        run_to_step(1);
        CHECK(g.sec == SN_SEC_IN2, "REC in a drama: back in on the next bar (%s)", sn_sec_name(g.sec));
        run_bars(1);
        CHECK(g.sec == SN_SEC_FULL2 && g.tr == 1, "the full band again, +1 (%s, +%d)", sn_sec_name(g.sec), g.tr);
    }
    /* REC in the full band: a tom fill over the bar's last two beats, a crash on the next downbeat */
    {
        fresh(SN_ST_BIGROOM);
        g.s.build = SN_BUILD_8;
        g.s.fills = SN_FILLS_OFF;
        sn_play(&g, true);
        run_to_sec(SN_SEC_FULL);
        run_bars(2);
        run_to_step(3);
        reset_counts();
        sn_breakin(&g, true);
        sn_breakin(&g, false);
        run_to_step(15);
        int toms = drum_on[SN_DR_TOMH] + drum_on[SN_DR_TOMM] + drum_on[SN_DR_TOML];
        run_to_step(1);
        CHECK(toms >= 6, "REC in the full band: a tom fill (%d)", toms);
        CHECK(crash_at_step0 == 1, "and a crash on the downbeat (%d)", crash_at_step0);
    }
    /* FORM off: the full band on and on; SEQ still brings a drama */
    {
        fresh(SN_ST_BIGROOM);
        g.s.form = 0;
        sn_play(&g, true);
        run_bars(40);
        CHECK(g.sec == SN_SEC_FULL, "FORM off: the full band (%s)", sn_sec_name(g.sec));
        sn_drama(&g);
        run_bars(1 + SN_DRAMA_BARS + 1 + 20);
        CHECK(g.sec == SN_SEC_FULL2, "FORM off: after a drama, the full band on (%s)", sn_sec_name(g.sec));
    }
    /* a song replays exactly */
    {
        uint32_t h[2];
        for (int k = 0; k < 2; ++k) {
            fresh(SN_ST_NIGHTDRIVE);
            g.s.song = 41;
            sn_song_sounds(&g.s);
            sn_rebuild(&g);
            hash = 2166136261u;
            sn_play(&g, true);
            run_bars(48);
            h[k] = hash;
        }
        CHECK(h[0] == h[1], "a song replays exactly (%08x %08x)", h[0], h[1]);
    }
    /* a style queued mid-bar comes in on the bar line */
    {
        fresh(SN_ST_SLOWBURN);
        sn_play(&g, true);
        run_bars(2);
        run_to_step(6);
        sn_queue_style(&g, SN_ST_TOMTOM);
        CHECK(g.s.style == SN_ST_SLOWBURN, "queued: still SLOWBURN mid-bar");
        run_to_step(1);
        CHECK(g.s.style == SN_ST_TOMTOM, "on the bar line: TOMTOM");
    }
    /* the keys: snapped to the scale, the riff makes way */
    {
        fresh(SN_ST_BIGROOM);
        sn_play(&g, true);
        run_bars(1);
        int bad = 0;
        for (int n = 53; n <= 79; ++n) {
            sn_key(&g, n - 53, n, true);
            bad += sn_snap(&g.s, g.key_note[n - 53]) != g.key_note[n - 53];
            sn_key(&g, n - 53, n, false);
        }
        CHECK(bad == 0, "the keys in the scale (%d off)", bad);
    }
    /* a bad record: every field back in range */
    {
        sn_settings s;
        memset(&s, 0xEE, sizeof s);
        sn_settings_fix(&s);
        CHECK(s.style < SN_STYLES && s.build < SN_BUILD_COUNT && s.drama < SN_DRAMA_COUNT && s.snd[3] < SN_KIT_COUNT &&
              s.scale < SN_SCALES && s.chords < SN_LOOPS && s.hats < SN_HATS_COUNT && s.bassline < SN_BL_COUNT &&
              (s.full == 8 || s.full == 16) && s.dramas >= 1 && s.dramas <= 2 && s.keyup <= 2 && s.bpm >= 60 &&
              s.bpm <= 200 && s.song <= 999 && s.lift >= 30 && s.lift <= 100, "a bad record fixed");
        sn_defaults(&s);
        CHECK(s.style == SN_ST_SLOWBURN && s.form == 1 && s.snd[3] == SN_KIT_BIG, "the defaults: SLOWBURN, the form, BIG");
    }
    printf("sn_groove: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
