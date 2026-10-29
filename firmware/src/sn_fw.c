/* SPDX-License-Identifier: GPL-3.0-only */
/* SNAPPER on the FM-1: the glue between Snapper (the groove engine and its screen) and Felucca's firmware (the
 * engines, voices and effects, the panel, the screen driver, USB MIDI, the flash). Included by felucca.c after
 * Felucca's own sources when FELUCCA_SNAPPER is 1; the groove engine (sn_groove.c) and the maths (tuna_libm.c, from
 * Tuna) are their own translation units, linked beside felucca.o (tools/build.py). Shaped as Flyfish's ff_fw.c.
 *
 * Who runs what:
 *   the audio ISR (audio.c audio_block -> sn_audio_pre): the groove in time, its notes straight into Felucca's
 *     tracks (trk_note_on / off) and out as USB MIDI; the lift and the pump into the tracks (sns_block);
 *   the main loop (main.c -> sn_frame, sn_input): the panel into the controls (the audio ISR held off while one
 *     acts), a part's sound loaded when it changed (a preset load is the main loop's), the cards into the tracks,
 *     the LEDs, the settings and sets saved while stopped, the screen.
 * Tracks: 1 PADS, 2 BASS, 3 KEYS, 4 DRUMS (sn_sounds.h). */
#include "sn_groove.h"
#include "sn_sounds.h"
#include "sn_ui.h"
#include "sn_ui.c"
#include "sn_version.h"

static sn_groove sn_g;
static sn_ui sn_u;

/* what the tracks were last set to */
static sn_settings sn_applied;
static bool sn_applied_ok;
static uint8_t sn_loaded_sig = 0xFF;
static int8_t sn_loaded_snd[SN_PARTS] = {-1, -1, -1, -1};

/* ---- the audio ISR --------------------------------------------------------------------------------------------- */
static void sn_sound(void *ctx, const sn_event *e)
{
    (void)ctx;
    uint32_t p = e->part % SN_PARTS;
    int n = p == SN_DRUMS ? sns_drum_note(&sn_g.s, e->note) : e->note;
    if (e->on)
        trk_note_on(&trk[p], (uint32_t)n, e->vel);
    else
        trk_note_off(&trk[p], (uint32_t)n);
    /* MIDI out: PADS on channel 1, BASS 2, KEYS 3, the drums 10 (General MIDI's) */
    uint32_t ch = p == SN_DRUMS ? 9u : p, st = (e->on ? 0x90u : 0x80u) | ch, cin = st >> 4;
    int mn = p == SN_DRUMS ? (int)SNS_GM[e->note % SN_DR_COUNT] : n;
    midi_out_event(cin | st << 8 | (uint32_t)mn << 16 | (uint32_t)(e->on ? e->vel : 0u) << 24);
}

static void sn_audio_pre(uint32_t n)
{
    sn_process(&sn_g, (int)n);
    sns_block(&sn_g);
}

static void sn_audio_post(int32_t *out, uint32_t n)
{
    (void)out;
    (void)n;
}

/* ---- the settings: one record in Felucca's first project object, the sets in the second; saved only while stopped
 * (the flash is the code's: XIP, writing it while playing would stall the audio) ---- */
#define SN_REC_MAGIC 0x31504E53u                             /* "SNP1" */
#define SN_SETS_MAGIC 0x31544553u                            /* "SET1" (Snapper's) */
typedef struct { uint32_t magic; uint16_t size, ver; sn_settings s; } sn_record;
typedef struct { uint32_t magic; uint16_t size; uint8_t used, pad; sn_settings s[SN_SETS]; } sn_sets_rec;
static uint32_t sn_save_ms;
static bool sn_dirty;
static void sn_settings_load(void)
{
#if FELUCCA_FLASH
    sn_record r;
    memset(&r, 0, sizeof r);
    int n = st_load(OBJ_PROJECT0, &r, sizeof r);
    if (n >= (int)(sizeof r - sizeof r.s) && r.magic == SN_REC_MAGIC) {
        sn_settings s;
        sn_defaults(&s);                                     /* (a shorter, older record: the new fields their defaults) */
        uint32_t k = r.size < sizeof s ? r.size : sizeof s;
        memcpy(&s, &r.s, k);
        sn_settings_fix(&s);
        sn_g.s = s;
    }
#endif
}
static void sn_settings_save(void)
{
#if FELUCCA_FLASH
    sn_record r;
    memset(&r, 0, sizeof r);
    r.magic = SN_REC_MAGIC;
    r.size = (uint16_t)sizeof r.s;
    r.ver = 1;
    r.s = sn_g.s;
    st_save(OBJ_PROJECT0, &r, sizeof r);
#endif
}
static void sn_sets_load(void)
{
#if FELUCCA_FLASH
    static sn_sets_rec r;                                    /* (static: 8 records, off the stack) */
    memset(&r, 0, sizeof r);
    if (st_load(OBJ_PROJECT0 + 1u, &r, sizeof r) == (int)sizeof r && r.magic == SN_SETS_MAGIC && r.size == sizeof r.s[0]) {
        for (int i = 0; i < SN_SETS; ++i) {
            sn_u.sets[i] = r.s[i];
            sn_settings_fix(&sn_u.sets[i]);
        }
        sn_u.set_used = r.used;
    }
#endif
}
static void sn_sets_save(void)
{
#if FELUCCA_FLASH
    static sn_sets_rec r;
    memset(&r, 0, sizeof r);
    r.magic = SN_SETS_MAGIC;
    r.size = (uint16_t)sizeof r.s[0];
    r.used = sn_u.set_used;
    memcpy(r.s, sn_u.sets, sizeof r.s);
    st_save(OBJ_PROJECT0 + 1u, &r, sizeof r);
#endif
}

/* ---- the sounds into the tracks (main loop) ---------------------------------------------------------------------- */
/* a sound of Snapper's own over the factory preset just loaded (as ui.c apply_preset_to puts a preset in) */
static void sn_apply_own(int16_t *p, const preset_t *pr)
{
    static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
    for (int i = 0; i < 8; i++)
        p[P_E0 + i] = pr->e[i];
    p[P_ATK] = pr->env[0];
    p[P_DEC] = pr->env[1];
    p[P_SUS] = pr->env[2];
    p[P_REL] = pr->env[3];
    p[P_ED_FLT] = pr->fenv;
    p[P_VOICE] = pr->mono ? V_LEGATO : V_POLY;
    for (int i = 0; i < 4; i++)
        p[P_DIST + i] = (int16_t)(pr->fx[i] ? pr->fx[i] - 1 : FX_DEF[i]);
}
/* one of Tuna's FM6 patches as the track's own (SLOT OWN), so nothing reloads a factory one over it (eng_fm6.c) */
static void sn_fm6_load(track_t *t, const uint8_t *packed)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK;
    uint8_t v[FP_SIZE + 1u];
    fm6_unpack(packed, v);
    fm6_set_patch(tr, v);
    t->p[P_E7] = FM6_OWN;
    fm6_slot[tr] = FM6_OWN;
    fm6_own_ok &= (uint8_t)~(1u << tr);
}
static void sn_load_part(uint32_t k)
{
    undo_depth++;                                            /* (no undo copy of Snapper's loads) */
    if (k == SN_DRUMS) {
        set_engine_of(&trk[k], sns_kit_engine(sn_g.s.snd[3]));
        apply_preset_to(&trk[k], 0u);
        undo_depth--;
        return;
    }
    const sns_sound *snd = sns_of(&sn_g, (int)k);
    set_engine_of(&trk[k], snd->engine);
    apply_preset_to(&trk[k], snd->preset);
    undo_depth--;
    fm1_irq_off();
    if (snd->own)
        sn_apply_own(trk[k].p, snd->own);
    if (snd->fm6)
        sn_fm6_load(&trk[k], snd->fm6);
    fm1_irq_on();
}

static void sn_apply(void)
{
    const sn_settings *s = &sn_g.s;
    bool all = !sn_applied_ok, every = all || sn_loaded_sig != sn_g.sound_sig;   /* (a style or a set: every part) */
    for (uint32_t k = 0; k < SN_PARTS; ++k)                  /* a sound changed (a card, a style, a set): load it */
        if (every || sn_loaded_snd[k] != (int8_t)s->snd[k]) {
            sn_load_part(k);
            sn_loaded_snd[k] = (int8_t)s->snd[k];
            all = true;
        }
    sn_loaded_sig = sn_g.sound_sig;
    if (all || memcmp(s, &sn_applied, sizeof *s)) {
        fm1_irq_off();
        for (uint32_t k = 0; k < SN_PARTS; ++k)
            sns_track(&trk[k], &sn_g, (int)k);
        sns_song(s);
        fm1_irq_on();
    }
    if (s->palette != settings.palette % NPALETTES) {        /* (the menu's colour, kept as Felucca keeps its own) */
        settings.palette = s->palette;
        settings_save();
    }
    sn_applied = *s;
    sn_applied_ok = true;
}

/* ---- the power-on screen: Snapper's logo (tools/sn_logo.py: 176 px, 8 greys, rows run-length coded) and the
 * Felucca it runs on; main.c holds it 0.75 s ---- */
#include "sn_logo.inc"
#define SN_SPLASH_MS 750u
static void sn_splash(void)
{
    uint16_t row[2][SN_LOGO_SIZE];                           /* (two: one fills while the other goes out by DMA) */
    uint32_t y, x0 = (240u - SN_LOGO_SIZE) / 2u, y0 = 16u;
    char line[40] = "Snapper " SN_VERSION " on Felucca ";
    uint32_t k = sizeof "Snapper " SN_VERSION " on Felucca " - 1u;
    lcd_fill(0, 0, 240, 240, SN_LOGO_BG);
    for (y = 0; y < SN_LOGO_SIZE; y++) {
        const uint8_t *p = SN_LOGO_RLE + SN_LOGO_ROW[y];
        uint16_t *r = row[y & 1u];
        uint32_t x = 0;
        while (x < SN_LOGO_SIZE) {
            uint32_t n = (*p >> 3) + 1u, c = SN_LOGO_PAL[*p & 7u];
            p++;
            while (n-- && x < SN_LOGO_SIZE)
                r[x++] = (uint16_t)c;
        }
        lcd_blit(x0, y0 + y, SN_LOGO_SIZE, 1, r);
    }
    for (const char *v = FELUCCA_VERSION; *v && k < sizeof line - 1u; v++)
        line[k++] = *v;
    line[k] = 0;
    draw_text_line(0, y0 + SN_LOGO_SIZE + 18u, 240, &AF_S, line, 0xA535u, SN_LOGO_BG, 1);   /* (grey, 160 165 170) */
    lcd_sync();
}

/* ---- start ------------------------------------------------------------------------------------------------------ */
static void sn_fw_init(void)
{
    uint32_t i;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    undo_depth++;
    fm6_init();
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        set_engine_of(t, i == 3u ? sns_kit_engine(SN_KIT_BIG) : SNS_ANALOG);
        apply_preset_to(t, 0u);
        t->engine = t->eng_req;
        track_defaults_steps(t);                             /* (Felucca's sequencer stays empty) */
    }
    undo_depth--;
    song.sel = 0;
    song.master_q12 = 2048;
    sn_init(&sn_g, (float)FS);
    sn_settings_load();
    sn_g.s.palette = (uint8_t)(settings.palette % NPALETTES);
    sn_rebuild(&sn_g);
    sn_set_sound(&sn_g, sn_sound, 0);
    snu_init(&sn_u, &sn_g);
    sn_sets_load();
    sn_apply();
}

/* ---- the main loop ----------------------------------------------------------------------------------------------- */
static uint32_t sn_notes_prev;
static void sn_input(void)
{
    uint32_t released = 0, pressed = fm1_input_edges(&released), b, k;
    static const uint8_t KNOB[NE] = {SNU_K_SELECT, SNU_K_ALGO, SNU_K_PRESETS, SNU_K1, SNU_K2, SNU_K3, SNU_K4};
    fm1_input_note_edges();                                  /* (the keys: from their state below) */
    for (b = 0; b < NB; b++) {                               /* (Felucca's B_* are Snapper's SNU_B_*, in the same order) */
        uint32_t bit = 1u << panel.btn[b];
        if (pressed & bit) {
            fm1_irq_off();
            snu_button(&sn_u, (int)b, true);
            fm1_irq_on();
        }
        if (released & bit) {
            fm1_irq_off();
            snu_button(&sn_u, (int)b, false);
            fm1_irq_on();
        }
    }
    for (k = 0; k < NE; k++) {
        int32_t s = panel_enc(k);
        if (s) {
            fm1_irq_off();
            snu_knob(&sn_u, KNOB[k], s);
            fm1_irq_on();
        }
    }
    uint32_t notes = fm1_in.notes, ch = notes ^ sn_notes_prev;
    for (k = 0; ch && k < 27u; k++)
        if ((ch >> k) & 1u) {
            fm1_irq_off();
            snu_key(&sn_u, SNU_KEY_LO + (int)k, (notes >> k) & 1u);
            fm1_irq_on();
        }
    sn_notes_prev = notes;
}

/* the LEDs: the page's button, REC while it holds or breaks in, SEQ in a drama, PLAY green while playing; the keys: the
 * ones held, else the chord's tones (the notes that fit right now) */
static void sn_leds(void)
{
    static const uint8_t PAGE_BTN[SNU_PAGES] = {
        [SNU_PG_PLAY] = B_HOME, [SNU_PG_SOUND] = B_FX, [SNU_PG_TONE] = B_FX, [SNU_PG_KEY] = B_SCL,
        [SNU_PG_GROOVE] = B_ARP, [SNU_PG_DRUMS] = B_EDIT, [SNU_PG_MIX] = B_GLO, [SNU_PG_SPACE] = B_GLO, [SNU_PG_FORM] = B_ENV,
        [SNU_PG_KEYS] = B_LFO, [SNU_PG_SETS] = B_SAVE, [SNU_PG_MENU] = B_HOME};
    static uint8_t ready;
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0};
    uint32_t k, c;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    led_put(nl, panel.btn[PAGE_BTN[sn_u.page % SNU_PAGES]], 1);
    led_put(nl, panel.btn[B_REC], sn_g.rec_held || sn_g.sec == SN_SEC_IN || sn_g.sec == SN_SEC_IN2);
    led_put(nl, panel.btn[B_SEQ], sn_g.sec == SN_SEC_DRAMA || sn_g.drama_next);
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);
    uint32_t lit = 0;
    if (sn_g.playing && sn_g.nloop > 0) {
        const sn_chord *chd = &sn_g.loop[sn_chord_now(&sn_g)];
        for (k = 0; k < 27u; k++)
            for (int t = 0; t < 4; ++t)
                if ((SNU_KEY_LO + k) % 12u == (uint32_t)(chd->tone[t] + sn_g.tr) % 12u) lit |= 1u << k;
    }
    for (k = 0; k < 27u; k++) {
        led_put(nl, 14u + k, (int)((fm1_in.notes >> k) & 1u));
        led_put(nd, 14u + k, (int)((lit >> k) & 1u) || !((fm1_in.notes >> k) & 1u));
    }
    if (sn_g.playing)
        led_clear(nd, panel.btn[B_PLAY]);
    for (c = 0; c < FM1_NCOL; c++)
        nd[c] = settings_leds == LEDS_DIM || settings_leds == LEDS_DIM_LO ? (uint8_t)(nd[c] & ~nl[c]) : 0u;
    /* the chord's keys glow dim even with the dim LEDs off: they are music, not decoration */
    for (k = 0; k < 27u; k++)
        if ((lit >> k) & 1u && !((fm1_in.notes >> k) & 1u))
            led_put(nd, 14u + k, 1);
    if (sn_g.playing)
        nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    fm1_led_dim_level(settings_leds == LEDS_DIM_LO);
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = nd[c];
        fm1_led_breath[c] = 0;
        fm1_led[c] = nl[c];
    }
}

/* a frame: the settings into the tracks, the LEDs, the screen; the settings and sets saved a while after the last
 * change, stopped */
static uint32_t sn_last_ms;
static void sn_frame(void)
{
    uint32_t now = fm1_ms, dt = now - sn_last_ms;
    static uint8_t first = 1;
    sn_last_ms = now;
    if (sn_u.changed) {
        sn_u.changed = false;
        sn_dirty = true;
        sn_save_ms = now;
    }
    sn_apply();
    if (sn_dirty && !sn_g.playing && now - sn_save_ms > 2000u) {
        sn_settings_save();
        sn_dirty = false;
    }
    if (sn_u.sets_changed && !sn_g.playing) {                /* (stopped: the flash is the code's while playing) */
        sn_u.sets_changed = false;
        sn_sets_save();
    }
    song.playing = sn_g.playing;                             /* (PLAY's green LED, Felucca's meters) */
    sn_leds();
    sn_u.batt = (int)batt_shown();
    snu_tick(&sn_u, dt > 100u ? 100 : (int)dt);
    if (first) {
        snu_draw_all(&sn_u);
        first = 0;
    } else {
        snu_draw(&sn_u);
    }
}
