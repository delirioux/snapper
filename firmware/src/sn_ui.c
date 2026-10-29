/* SPDX-License-Identifier: GPL-3.0-only */
/* SNAPPER screen and controls (sn_ui.h): Snapper's pages in Tuna's frame (Daurade's layout), drawn with Felucca's gfx.c.
 * Included after gfx.c (the firmware's felucca.c unit through sn_fw.c, or tests/sn_ui_test.c). Needs sn_sounds.h
 * (the sounds' names) before it. */
#include "sn_ui.h"
#include "sn_version.h"
#include "ui_icons.h"

#define SNU_HEAD 24
#define SNU_CARD_Y 28
#define SNU_CARD_W 57
#define SNU_CARD_H 44
#define SNU_CARD_X(c) (3 + 59 * (int32_t)(c))
#define SNU_PANEL_Y 76
#define SNU_PANEL_H 122
#define SNU_FOOT_Y 202
#define SNU_FOOT_H 38
#define SNU_MENU_HOLD_MS 600

static int snu_clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int snu_wrap(int v, int n) { return n > 0 ? ((v % n) + n) % n : 0; }
static void snu_cpy(char *d, const char *s, unsigned n)
{
    unsigned i = 0;
    for (; s[i] && i + 1 < n; ++i) d[i] = s[i];
    d[i] = 0;
}
static void snu_cat(char *d, const char *s, unsigned n)
{
    unsigned k = 0;
    while (d[k]) ++k;
    snu_cpy(d + k, s, n > k ? n - k : 0);
}
static void snu_fmt_int(char *d, int v)
{
    char b[12];
    int k = 0, neg = v < 0;
    unsigned u = neg ? (unsigned)-v : (unsigned)v;
    do { b[k++] = (char)('0' + u % 10); u /= 10; } while (u && k < 11);
    int j = 0;
    if (neg) d[j++] = '-';
    while (k) d[j++] = b[--k];
    d[j] = 0;
}
static uint32_t snu_hash(uint32_t h, const char *s)
{
    while (*s) h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}
static int32_t snu_icon_cell(uint32_t *size, uint32_t id)
{
    const uint8_t *idx = *size == 24u ? AI24_IDX : *size == 16u ? AI16_IDX : AI12_IDX;
    if (id >= ICON_COUNT)
        return -1;
    if (idx[id] == 0xFFu) {
        if (*size == 12u || AI12_IDX[id] == 0xFFu)
            return -1;
        *size = 12u;
        idx = AI12_IDX;
    }
    return idx[id];
}
/* the icon's ink centred up and down on the line cy */
static int32_t snu_icon_mid(int32_t x, int32_t cy, uint32_t size, uint32_t id, uint16_t fg, uint16_t bg)
{
    uint32_t sz = size;
    int32_t k = snu_icon_cell(&sz, id), y0, y1;
    if (k < 0)
        return 0;
    if (sz == 24u) {
        uint32_t v = AI24_INK[k];
        y0 = (int32_t)(v >> 8 & 255u);
        y1 = (int32_t)(v >> 24 & 255u);
    } else {
        uint32_t v = sz == 16u ? AI16_INK[k] : AI12_INK[k];
        y0 = (int32_t)(v >> 4 & 15u);
        y1 = (int32_t)(v >> 12 & 15u) + 1;
    }
    int32_t y = cy - HALF_UP(y1 - y0) - y0;
    cv_alpha_hc(x, y, sz, sz, sz == 24u ? AI24_DATA + AI24_OFF[k] : sz == 16u ? AI16_DATA + AI16_OFF[k] :
                AI12_DATA + AI12_OFF[k], AI_HC, ramp(fg, bg));
    GFX_HOOK_TEXT(x, y + cv_oy, x + (int32_t)sz, y + cv_oy + (int32_t)sz, "icon", 4u);
    return (int32_t)sz;
}

/* ---- the cards ------------------------------------------------------------------------------------------- */
enum { SC_NUM, SC_LIST, SC_SND, SC_KEY, SC_SIGNED, SC_LOOP, SC_VALS, SC_PAL, SC_CRUSH, SC_SETSEL, SC_LOAD, SC_STORE,
       SC_CLEAR, SC_OFF };
typedef struct {
    const char *label;
    uint8_t off;                                   /* offsetof(sn_settings, the field) */
    uint8_t kind;
    int16_t lo, hi;
    uint8_t step;
    const char *const *names;
    const uint8_t *vals;                           /* SC_VALS: the values it steps through (hi + 1 of them) */
} snu_cdef;
#define SS_(f) ((uint8_t)__builtin_offsetof(sn_settings, f))
static const char *const SN_ONOFF[] = {"OFF", "ON"};
static const char *const SN_LOOK[] = {"FLAT", "LINE"};
static const char *const SN_HATS[] = {"8THS", "16THS", "OPEN"};
static const char *const SN_FILLS[] = {"OFF", "8 BARS", "4 BARS"};
static const char *const SN_BASSLINE[] = {"PULSE", "LONG", "SYNCO", "OCTAVE", "SEQ", "SLIDE"};
static const char *const SN_KICK[] = {"PUNCH", "ROUND"};
static const char *const SN_CLAP[] = {"SNARE", "CLAP", "BOTH"};
static const char *const SN_CRASH[] = {"OFF", "SECTION", "8 BARS"};
static const char *const SN_KEYUP[] = {"OFF", "+1", "+2"};
static const char *const SN_SPEED[] = {"SLOW", "MID", "FAST"};
static const char *const SN_SCALE[] = {"MIN", "MAJ", "DOR", "MIXO"};
static const char *const SN_SCALE_LONG[] = {"MINOR", "MAJOR", "DORIAN", "MIXOLYDIAN"};
static const char *const SN_BUILD[] = {"8 BARS", "16 BARS", "32 BARS", "HOLD"};
static const char *const SN_DRAMA[] = {"OFF", "AUTO"};
static const uint8_t SV_BARS[] = {1, 2, 4}, SV_SECTION[] = {8, 16}, SV_REPEAT[] = {1, 2, 4}, SV_RANGE[] = {1, 2},
                     SV_DRAMAS[] = {1, 2};
#define C_NUM(l, f) {l, SS_(f), SC_NUM, 0, 100, 5, 0, 0}
#define C_LIST(l, f, n) {l, SS_(f), SC_LIST, 0, (int16_t)(sizeof n / sizeof n[0]) - 1, 1, n, 0}
#define C_VALS(l, f, v) {l, SS_(f), SC_VALS, 0, (int16_t)(sizeof v / sizeof v[0]) - 1, 1, 0, v}
#define C_SND(l, p) {l, (uint8_t)(SS_(snd) + (p)), SC_SND, 0, (p) == 3 ? SN_KIT_COUNT - 1 : SN_SOUNDS - 1, 1, 0, 0}
#define C_OFF {"", 0, SC_OFF, 0, 0, 0, 0, 0}
static const snu_cdef SNU_CARDS[SNU_PAGES][4] = {
    [SNU_PG_PLAY] = {C_LIST("BUILD", build, SN_BUILD), C_LIST("DRAMA", drama, SN_DRAMA), C_NUM("ENERGY", energy),
                    C_NUM("ROOM", room)},
    [SNU_PG_SOUND] = {C_SND("PADS", 0), C_SND("BASS", 1), C_SND("KEYS", 2), C_SND("DRUMS", 3)},
    [SNU_PG_TONE] = {C_NUM("RESO", reso), C_NUM("DRIVE", drive), C_NUM("DETUNE", ensemble),
                    {"CRUSH", SS_(crush), SC_CRUSH, 0, 100, 5, 0, 0}},
    [SNU_PG_KEY] = {{"KEY", SS_(key), SC_KEY, 0, 11, 1, 0, 0}, C_LIST("SCALE", scale, SN_SCALE),
                   {"CHORDS", SS_(chords), SC_LOOP, 0, SN_LOOPS - 1, 1, 0, 0}, C_VALS("BARS", bars, SV_BARS)},
    [SNU_PG_GROOVE] = {{"SWING", SS_(swing), SC_NUM, 0, 60, 2, 0, 0}, C_LIST("HATS", hats, SN_HATS),
                      C_LIST("BASS", bassline, SN_BASSLINE), C_LIST("FILLS", fills, SN_FILLS)},
    [SNU_PG_DRUMS] = {C_SND("KIT", 3), C_LIST("KICK", kick, SN_KICK), C_LIST("SNARE", clap, SN_CLAP),
                     C_LIST("CRASH", crash, SN_CRASH)},
    [SNU_PG_MIX] = {C_NUM("PADS", lvl[0]), C_NUM("BASS", lvl[1]), C_NUM("KEYS", lvl[2]), C_NUM("DRUMS", lvl[3])},
    [SNU_PG_SPACE] = {C_NUM("REVERB", reverb), C_NUM("DELAY", delay),
                     {"TIME", SS_(dtime), SC_LIST, 0, SN_DTIME_COUNT - 1, 1, SNS_DTIME_NAMES, 0}, C_NUM("WIDTH", width)},
    [SNU_PG_FORM] = {C_LIST("FORM", form, SN_ONOFF), C_VALS("FULL", full, SV_SECTION), C_VALS("DRAMAS", dramas, SV_DRAMAS),
                    C_LIST("KEY UP", keyup, SN_KEYUP)},
    [SNU_PG_KEYS] = {C_LIST("SPEED", speed, SN_SPEED), C_VALS("RANGE", range, SV_RANGE),
                    {"GATE", SS_(gate), SC_NUM, 10, 100, 5, 0, 0}, C_VALS("REPEAT", repeat, SV_REPEAT)},
    [SNU_PG_SETS] = {{"SET", 0, SC_SETSEL, 0, SN_SETS - 1, 1, 0, 0}, {"LOAD", 0, SC_LOAD, 0, 0, 1, 0, 0},
                    {"STORE", 0, SC_STORE, 0, 0, 1, 0, 0}, {"CLEAR", 0, SC_CLEAR, 0, 0, 1, 0, 0}},
    [SNU_PG_MENU] = {{"COLOR", SS_(palette), SC_PAL, 0, 10, 1, 0, 0}, C_LIST("LOOK", look, SN_LOOK), C_OFF, C_OFF},
};
static const char *const SNU_PAGE_NAMES[SNU_PAGES] = {"PLAY", "SOUND", "TONE", "KEY", "GROOVE", "DRUMS", "MIX", "SPACE",
                                                    "FORM", "KEYS", "SETS", "MENU"};
const char *snu_page_name(int page) { return page >= 0 && page < SNU_PAGES ? SNU_PAGE_NAMES[page] : ""; }

static int snu_get(const sn_ui *u, const snu_cdef *c)
{
    const uint8_t *b = (const uint8_t *)&u->g->s + c->off;
    if (c->kind == SC_SIGNED)
        return (int)*(const int8_t *)b;
    if (c->kind == SC_VALS) {                      /* the index of the value it holds */
        for (int i = 0; i <= c->hi; ++i)
            if (c->vals[i] == *b) return i;
        return 0;
    }
    if (c->kind == SC_SETSEL)
        return u->set_sel;
    return (int)*b;
}
static void snu_set(sn_ui *u, const snu_cdef *c, int v)
{
    uint8_t *b = (uint8_t *)&u->g->s + c->off;
    if (c->kind == SC_SIGNED) *(int8_t *)b = (int8_t)v;
    else if (c->kind == SC_VALS) *b = c->vals[snu_clampi(v, c->lo, c->hi)];
    else if (c->kind == SC_SETSEL) u->set_sel = v;
    else *b = (uint8_t)v;
}
static void snu_card_value(const sn_ui *u, const snu_cdef *c, char *t, unsigned n)
{
    int v = snu_get(u, c);
    t[0] = 0;
    switch (c->kind) {
    case SC_LIST: snu_cpy(t, c->names[snu_clampi(v, c->lo, c->hi)], n); break;
    case SC_SND: snu_cpy(t, sns_name(c->off - SS_(snd), v), n); break;
    case SC_KEY: snu_cpy(t, sn_note_name(v), n); break;
    case SC_SIGNED: if (v > 0) { snu_cpy(t, "+", n); } snu_fmt_int(t + (v > 0), v); break;
    case SC_LOOP: sn_loop_name(&u->g->s, v, t, (int)n); break;
    case SC_VALS: {
        int x = c->vals[snu_clampi(v, c->lo, c->hi)];
        snu_fmt_int(t, x);
        if (c->off == SS_(full)) snu_cat(t, " BARS", n);
        if (c->off == SS_(repeat)) snu_cat(t, x == 1 ? " BAR" : " BARS", n);
        if (c->off == SS_(range)) snu_cat(t, " OCT", n);
        break;
    }
    case SC_PAL: snu_cpy(t, UI_PALETTES[snu_wrap(v, (int)NPALETTES)].name, n); break;
    case SC_CRUSH: if (v) snu_fmt_int(t, v); else snu_cpy(t, "OFF", n); break;
    case SC_SETSEL: snu_fmt_int(t, v + 1); break;
    case SC_LOAD: snu_cpy(t, (u->set_used >> u->set_sel) & 1u ? ">" : "EMPTY", n); break;
    case SC_STORE: snu_cpy(t, ">", n); break;
    case SC_CLEAR:
        if (u->clear_turns == 1) snu_cpy(t, ">>", n);
        else if (u->clear_turns == 2) snu_cpy(t, ">", n);
        else snu_cpy(t, ">>>", n);
        break;
    case SC_OFF: break;
    default: snu_fmt_int(t, v); break;
    }
}

static void snu_draw_card(sn_ui *u, int i, const snu_cdef *c)
{
    bool hot = u->hot == i && u->hot_ms > 0;
    uint16_t lc = hot ? T_ACCENT : T_MID, vc = hot ? T_ACCENT : T_THEME;
    char val[24];
    snu_card_value(u, c, val, sizeof val);
    int ratio = -1;
    if ((c->kind == SC_NUM || c->kind == SC_CRUSH || c->kind == SC_SIGNED || c->kind == SC_VALS || c->kind == SC_SETSEL ||
         c->kind == SC_LOOP || c->kind == SC_KEY || c->kind == SC_SND) && c->hi > c->lo)
        ratio = (snu_get(u, c) - c->lo) * 1000 / (c->hi - c->lo);
    uint32_t sig = snu_hash(2166136261u + (uint32_t)i * 7u + ux.gen * 131u + (uint32_t)hot * 3u + (uint32_t)u->page * 977u,
                           c->label);
    sig = snu_hash(sig, val) + (uint32_t)(ratio + 1) * 31u;
    if (!u->force && sig == u->sig_card[i])
        return;
    u->sig_card[i] = sig;
    cv_begin(SNU_CARD_W, SNU_CARD_H, T_BG);
    cv_rrect(0, 0, SNU_CARD_W, SNU_CARD_H, 4, T_SURF, T_BG);
    if (c->kind == SC_OFF) {
        cv_rect(SNU_CARD_W / 2 - 6, SNU_CARD_H / 2, 12, 1, T_DIM);
    } else {
        cv_text_fit(5, 3, &AF_S, c->label, lc, T_SURF, SNU_CARD_W - 8);
        const aafont_t *vf = text_w(&AF_M, val) <= SNU_CARD_W - 8 ? &AF_M : &AF_S;
        cv_text_fit(5, vf == &AF_M ? 17 : 19, vf, val, vc, T_SURF, SNU_CARD_W - 8);
        if (ratio >= 0) {
            int32_t w = SNU_CARD_W - 10, x0 = 5, y = 37, r = snu_clampi(ratio, 0, 1000) * w / 1000;
            cv_rect(x0, y, w, 3, T_LINE);
            if (c->kind == SC_SIGNED) {
                int32_t mid = x0 + w / 2;
                if (r >= w / 2) cv_rect(mid, y, r - w / 2 + 1, 3, vc);
                else cv_rect(x0 + r, y, mid - x0 - r + 1, 3, vc);
                cv_rect(mid, y - 2, 1, 7, hot ? T_ACCENT : T_MID);
            } else {
                cv_rect(x0, y, r > 0 ? r : 1, 3, vc);
            }
        }
    }
    cv_blit((uint32_t)SNU_CARD_X(i), SNU_CARD_Y);
}

/* ---- the header: playing, the tempo, a word (the section, NEXT), a badge (HOLD, the key up), the battery ------- */
static void snu_head_msg(const sn_ui *u, char *t, unsigned n)
{
    const sn_groove *g = u->g;
    t[0] = 0;
    if (u->msg_ms > 0) {
        snu_cpy(t, u->msg, n);
    } else if (g->next_style >= 0) {
        snu_cpy(t, "NEXT ", n);
        snu_cat(t, sn_style_name(g->next_style), n);
    } else if (g->next_song >= 0) {
        char a[8];
        snu_cpy(t, "NEXT SONG ", n);
        snu_fmt_int(a, g->next_song + 1);
        snu_cat(t, a, n);
    } else if (g->playing) {
        snu_cpy(t, sn_sec_name(g->sec), n);
    }
}
static void snu_head_badge(const sn_ui *u, char *t, unsigned n)
{
    const sn_groove *g = u->g;
    t[0] = 0;
    if (!g->playing)
        return;
    if (g->rec_held) {
        snu_cpy(t, "HOLD", n);
    } else if (g->tr > 0) {
        char a[4];
        snu_cpy(t, "+", n);
        snu_fmt_int(a, g->tr);
        snu_cat(t, a, n);
    } else if (sn_tense(g)) {
        snu_cpy(t, g->s.build == SN_BUILD_HOLD ? "HOLD" : "AUTO", n);
    }
}

static void snu_draw_head(sn_ui *u)
{
    const sn_groove *g = u->g;
    char b[16], m[40], bd[8];
    bool hot = u->hot == 9 && u->hot_ms > 0;
    snu_head_msg(u, m, sizeof m);
    snu_head_badge(u, bd, sizeof bd);
    uint32_t sig = snu_hash(snu_hash((uint32_t)g->playing * 3u + (uint32_t)g->s.bpm * 101u + ux.gen * 13u +
                                   (uint32_t)u->batt * 7777u + (uint32_t)hot * 23u, m), bd);
    if (!u->force && sig == u->sig_head)
        return;
    u->sig_head = sig;
    cv_begin(240, SNU_HEAD, T_BG);
    snu_icon_mid(8, SNU_HEAD / 2, 16, g->playing ? ICON_X_PLAY : ICON_X_STOP, g->playing ? T_THEME : T_MID, T_BG);
    snu_icon_mid(54, SNU_HEAD / 2, 16, ICON_TEMPO, T_MID, T_BG);
    snu_fmt_int(b, g->s.bpm);
    cv_text_on(72, CAP_IN(M, SNU_HEAD), &AF_M, b, hot ? T_ACCENT : T_THEME, T_BG);
    int32_t right = 208;
    if (bd[0]) {                                   /* the badge: the key up or HELD, in the accent */
        int32_t w = text_w(&AF_S, bd) + 10;
        cv_rrect(right - w, 4, w, SNU_HEAD - 8, 3, T_ACCENT, T_BG);
        cv_text_in(right - w, 4 + CAP_IN(S, SNU_HEAD - 8), w, &AF_S, bd, T_INK, T_ACCENT);
        right -= w + 6;
    }
    if (m[0]) {
        bool sec = g->playing && u->msg_ms <= 0 && g->next_style < 0 && g->next_song < 0;
        if (m[0] == 'N' && m[1] == 'E' && m[2] == 'X' && m[3] == 'T' && m[4] == ' ') {   /* NEXT in the accent */
            int32_t x = cv_text_on(106, CAP_IN(S, SNU_HEAD), &AF_S, "NEXT", T_ACCENT, T_BG) + 5;
            cv_free_text(x, CAP_IN(S, SNU_HEAD), &AF_S, m + 5, T_TEXT, T_BG, right - x);
        } else {
            cv_free_text(106, CAP_IN(S, SNU_HEAD), &AF_S, m, sec ? T_ACCENT : T_TEXT, T_BG, right - 106);
        }
    }
    {
        static const uint8_t BAT[5] = {ICON_X_BAT0, ICON_X_BAT1, ICON_X_BAT3, ICON_X_BAT4, ICON_X_BAT_CHG};
        int lv = snu_clampi(u->batt, 0, 4);
        snu_icon_mid(214, SNU_HEAD / 2, 24, BAT[lv], lv == 1 ? T_ACCENT : lv == 0 ? T_MID : T_THEME, T_BG);
    }
    cv_blit(0, 0);
}

/* ---- the footer: the section's bars as 16 dashes, the style and its kit, the page ------------------------------ */
static void snu_draw_foot(sn_ui *u)
{
    const sn_groove *g = u->g;
    int filled = 0;
    if (g->playing) {
        int len = sn_sec_len(g, g->sec);
        filled = len > 0 ? g->sec_bar * 16 / len : g->sec_bar % 16;
    }
    const char *style = sn_style_name(g->s.style);
    const char *kit = sns_name(SN_DRUMS, g->s.snd[3]);
    uint32_t sig = snu_hash(snu_hash(snu_hash((uint32_t)filled * 7u + (uint32_t)g->playing * 3u + ux.gen * 11u +
                                           (uint32_t)u->page * 13u, style), snu_page_name(u->page)), kit);
    if (!u->force && sig == u->sig_foot)
        return;
    u->sig_foot = sig;
    cv_begin(240, SNU_FOOT_H, T_BG);
    for (int i = 0; i < 16; ++i)
        cv_rect(10 + i * 14, 6, 10, 2, i < filled ? T_THEME : i == filled && g->playing ? T_ACCENT : T_LINE);
    cv_free_text(10, 17, &AF_M, style, T_THEME, T_BG, 130);
    int32_t nx = 10 + text_w(&AF_M, style);
    if (nx > 140) nx = 140;
    int32_t yb = 17 + AF_M_CAP_Y + AF_M_CAP_H - AF_S_CAP_Y - AF_S_CAP_H;
    int32_t px = 230 - text_w(&AF_S, snu_page_name(u->page)) - 8;
    if (px - nx - 6 > 24)
        cv_free_text(nx + 6, yb, &AF_S, kit, T_MID, T_BG, px - nx - 6);
    cv_text_r(230, yb, &AF_S, snu_page_name(u->page), T_MID, T_BG);
    cv_blit(0, SNU_FOOT_Y);
}

/* ---- the pictures -------------------------------------------------------------------------------------------- */
static void snu_panel_begin(void)
{
    cv_begin(240, SNU_PANEL_H, T_BG);
    cv_rrect(3, 0, 234, SNU_PANEL_H, 4, T_SURF, T_BG);
}
static int32_t snu_yb_s(int32_t y_m) { return y_m + AF_M_CAP_Y + AF_M_CAP_H - AF_S_CAP_Y - AF_S_CAP_H; }
static void snu_label(int32_t x, int32_t y, const char *s, uint16_t c) { cv_text_on(x, y, &AF_S, s, c, T_SURF); }
static uint16_t snu_well(void) { return ux_mix(T_SURF, T_LINE, 100); }

/* the loop's chords, the one sounding filled */
static void snu_chord_strip(const sn_groove *g, int32_t y, int now)
{
    int n = g->nloop < 4 ? g->nloop : 4;
    int32_t gap = 5, w = (224 - gap * (n - 1)) / (n > 0 ? n : 1);
    for (int i = 0; i < n; ++i) {
        int32_t x = 10 + i * (w + gap);
        if (i == now) {
            cv_rrect(x, y, w, 20, 3, T_THEME, T_SURF);
            cv_text_in(x, y + CAP_IN(S, 20), w, &AF_S, g->loop[i].name, T_INK, T_THEME);
        } else {
            cv_rrect(x, y, w, 20, 3, snu_well(), T_SURF);
            cv_rrect(x + 1, y + 1, w - 2, 18, 3, T_SURF, snu_well());
            cv_text_in(x, y + CAP_IN(S, 20), w, &AF_S, g->loop[i].name, T_TEXT, T_SURF);
        }
    }
}

/* the form's eight sections as words, the one playing in the accent and underlined as far as it has gone; the words
 * spread over the width (short ones when they do not fit) */
static const char *const SNU_SEC_TINY[SN_SECS] = {"H", "R", "IN", "F", "D", "IN", "F", "E"};
static void snu_form_xs(int32_t x[SN_SECS], int32_t w[SN_SECS], bool *tiny)
{
    int32_t sum = 0;
    *tiny = false;
    for (int k = 0; k < SN_SECS; ++k)
        sum += w[k] = text_w(&AF_S, sn_sec_short(k));
    if (sum + 2 * (SN_SECS - 1) > 224) {
        *tiny = true;
        sum = 0;
        for (int k = 0; k < SN_SECS; ++k)
            sum += w[k] = text_w(&AF_S, SNU_SEC_TINY[k]);
    }
    int32_t gap = (224 - sum) / (SN_SECS - 1), xx = 10;
    for (int k = 0; k < SN_SECS; ++k) {
        x[k] = xx;
        xx += w[k] + gap;
    }
}
static void snu_form_strip(const sn_groove *g, int32_t y)
{
    int32_t x[SN_SECS], w[SN_SECS];
    bool tiny;
    snu_form_xs(x, w, &tiny);
    for (int k = 0; k < SN_SECS; ++k) {
        bool now = g->playing && (g->s.form ? g->sec == k : k == SN_SEC_FULL);
        cv_text_on(x[k], y, &AF_S, tiny ? SNU_SEC_TINY[k] : sn_sec_short(k), now ? T_ACCENT : T_MID, T_SURF);
        if (now) {                                 /* how far into it */
            int len = sn_sec_len(g, k);
            int32_t p = len > 0 ? (g->sec_bar * 16 + (g->step < 0 ? 0 : g->step)) * w[k] / (len * 16) : w[k];
            cv_rect(x[k], y + 14, w[k], 2, snu_well());
            cv_rect(x[k], y + 14, p > 0 ? p : 1, 2, T_ACCENT);
        }
    }
}

/* the tension: sixteen columns rising to the break-in, as many lit as the tension has reached, the front one in the
 * accent */
static void snu_tension_meter(const sn_groove *g, int32_t y, int32_t h)
{
    int lit = (int)(g->tension * 16.0f + 0.5f);
    for (int k = 0; k < 16; ++k) {
        int32_t bh = 6 + (int32_t)((float)(h - 6) * ((float)k / 15.0f) * ((float)k / 15.0f));
        uint16_t c = k < lit - 1 ? T_THEME : k == lit - 1 ? T_ACCENT : T_LINE;
        cv_rect(10 + k * 14, y + h - bh, 10, bh, c);
    }
}

/* the bar as it plays: a column a sixteenth, as tall as the parts that sounded on it; the one playing lit */
static void snu_step_meter(const sn_groove *g, int32_t y, int32_t h)
{
    int now = g->playing && g->step >= 0 ? g->step : -1;
    for (int k = 0; k < 16; ++k) {
        int n = 0;
        for (int p = 0; p < SN_PARTS; ++p) {
            uint16_t bits = k <= now ? g->hits[p] : g->hits_last[p];
            n += (bits >> k) & 1u;
        }
        int32_t x = 10 + k * 14, bh = n ? 6 + n * (h - 6) / 4 : 2;
        uint16_t c = k == now ? T_ACCENT : k % 4 == 0 ? T_THEME : ux_mix(T_SURF, T_THEME, 75);
        cv_rect(x, y + h - bh, 10, bh, n ? c : T_LINE);
    }
}

/* the low-pass's curve: flat, the resonant peak at the cutoff, then the fall; cutoff and reso 0..1000 */
static void snu_filter_curve(int32_t x0, int32_t y0, int32_t w, int32_t h, int cutoff, int reso, uint16_t c, bool fill)
{
    int32_t cx = x0 + 8 + cutoff * (w - 16) / 1000, base = y0 + h * 45 / 100, py = -1;
    int32_t pk = reso * (h * 40 / 100) / 1000;
    for (int32_t x = x0; x < x0 + w; ++x) {
        int32_t d = x - cx, y;
        if (d < -24) y = base;
        else if (d <= 0) y = base - pk * (24 + d) * (24 + d) / (24 * 24);
        else y = base - pk + d * d / 6;
        if (d > 0 && y < base - pk + d) y = base - pk + d;
        if (y > y0 + h) y = y0 + h;
        if (y < y0) y = y0;
        if (fill)
            cv_rect(x, y + 1, 1, y0 + h - y, ux_mix(T_SURF, c, 18));
        if (py >= 0)
            cv_line_t(x - 1, py, x, y, c, 2);
        py = y;
    }
    cv_rect(cx, y0 + h + 3, 1, 4, T_ACCENT);
}

static void snu_panel_play(sn_ui *u)
{
    const sn_groove *g = u->g;
    if (!g->playing) {
        char line[40], a[8];
        int32_t w = kc_w(KC_PLAY) + 5 + text_w(&AF_S, "TO START");
        int32_t x = 3 + HALF_UP(234 - w), y = SNU_PANEL_H / 2 - KC_H / 2 - 8;
        cv_keycap(x, y, KC_PLAY, T_KEY, T_INK, T_SURF);
        cv_text_on(x + kc_w(KC_PLAY) + 5, y + CAP_IN(S, KC_H), &AF_S, "TO START", T_MID, T_SURF);
        snu_cpy(line, "SONG ", sizeof line);
        snu_fmt_int(a, g->s.song + 1);
        snu_cat(line, a, sizeof line);
        snu_cat(line, "   ", sizeof line);
        snu_cat(line, sn_note_name(g->s.key), sizeof line);
        snu_cat(line, " ", sizeof line);
        snu_cat(line, SN_SCALE_LONG[g->s.scale % SN_SCALES], sizeof line);
        int32_t lw = text_w(&AF_S, line);
        cv_text_on(3 + HALF_UP(234 - lw), SNU_PANEL_H / 2 + 14, &AF_S, line, T_DIM, T_SURF);
        return;
    }
    snu_chord_strip(g, 6, sn_chord_now(g));
    if (sn_in_full(g))                             /* the full band: the bar as it plays */
        snu_step_meter(g, 32, 50);
    else                                           /* the hush, the rise, the drama, the end: the tension */
        snu_tension_meter(g, 32, 50);
    snu_form_strip(g, 92);
}

/* PRESETS turned: the ten styles, the queued one marked */
static void snu_panel_styles(sn_ui *u)
{
    const sn_groove *g = u->g;
    int sel = g->next_style >= 0 ? g->next_style : g->s.style;
    snu_label(10, 5, "LOUD", T_MID);
    snu_label(124, 5, "NIGHT", T_MID);
    for (int i = 0; i < 10; ++i) {
        int col = SN_STYLE[i].side, row = 0;
        for (int k = 0; k < i; ++k) row += SN_STYLE[k].side == col;
        int32_t x = col ? 120 : 6, y = 22 + row * 19;
        bool now = i == g->s.style, q = i == sel && g->next_style >= 0;
        if (q || (i == sel && !g->playing)) {
            cv_rrect(x, y - 3, 110, 17, 3, T_ACCENT, T_SURF);
            cv_text_fit(x + 6, y, &AF_S, SN_STYLE[i].name, T_INK, T_ACCENT, 100);
        } else {
            cv_text_fit(x + 6, y, &AF_S, SN_STYLE[i].name, now ? T_THEME : T_TEXT, T_SURF, 100);
        }
        if (now)
            cv_rect(x - 1, y + 2, 2, 8, T_THEME);
    }
}

static void snu_panel_sound(sn_ui *u)
{
    const sn_groove *g = u->g;
    static const char *const nm[4] = {"PADS", "BASS", "KEYS", "DRUMS"};
    for (int i = 0; i < 4; ++i) {
        int32_t y = 8 + i * 28;
        bool hot = u->hot == i && u->hot_ms > 0;
        cv_text_on(10, y, &AF_S, nm[i], hot ? T_ACCENT : T_MID, T_SURF);
        cv_text_fit(64, y, &AF_S, sns_name(i, g->s.snd[i]), T_TEXT, T_SURF, 164);
        uint16_t bits = g->playing ? g->hits_last[i] : 0;
        for (int k = 0; k < 16; ++k) {
            bool on = (bits >> k) & 1;
            cv_rect(64 + k * 10, y + 18, 7, 2, on ? (hot ? T_ACCENT : T_THEME) : T_LINE);
        }
    }
}

static void snu_panel_tone(sn_ui *u)
{
    const sn_groove *g = u->g;
    snu_label(10, 6, "THE FILTER THE LIFT MOVES", T_MID);
    snu_filter_curve(10, 30, 220, 56, (int)(g->lift_now * 1000.0f), 150 + g->s.reso * 8, T_THEME, true);
    char t[24], a[8];
    snu_cpy(t, "RESO ", sizeof t);
    snu_fmt_int(a, g->s.reso);
    snu_cat(t, a, sizeof t);
    cv_text_r(230, 100, &AF_S, t, T_THEME, T_SURF);
    if (g->s.crush) {
        snu_cpy(t, "CRUSH", sizeof t);
    } else {
        snu_cpy(t, "DETUNE ", sizeof t);
        snu_fmt_int(a, g->s.ensemble);
        snu_cat(t, a, sizeof t);
    }
    snu_label(10, 100, t, T_DIM);
}

static void snu_panel_key(sn_ui *u)
{
    const sn_groove *g = u->g;
    char t[32], a[8];
    int bpc = sn_bars_per_chord(&g->s), now = g->playing ? sn_chord_now(g) : -1;
    snu_cpy(t, "THE LOOP, ", sizeof t);
    snu_fmt_int(a, bpc);
    snu_cat(t, a, sizeof t);
    snu_cat(t, bpc == 1 ? " BAR A CHORD" : " BARS A CHORD", sizeof t);
    snu_label(10, 6, t, T_MID);
    int n = g->nloop < 4 ? g->nloop : 4;
    int32_t gap = 4, w = (224 - gap * (n - 1)) / (n > 0 ? n : 1);
    for (int i = 0; i < n; ++i) {
        int32_t x = 10 + i * (w + gap);
        uint16_t f = i == now ? T_THEME : snu_well();
        cv_rrect(x, 30, w, 52, 4, f, T_SURF);
        const aafont_t *ft = text_w(&AF_M, g->loop[i].name) <= w - 4 ? &AF_M : &AF_S;
        cv_text_in(x, 30 + (ft == &AF_M ? CAP_IN(M, 52) : CAP_IN(S, 52)), w, ft, g->loop[i].name, i == now ? T_INK : T_TEXT, f);
        int b0 = i * bpc + 1, b1 = b0 + bpc - 1;
        snu_fmt_int(t, b0);
        if (b1 > b0) { snu_cat(t, "-", sizeof t); snu_fmt_int(a, b1); snu_cat(t, a, sizeof t); }
        cv_text_in(x, 92, w, &AF_S, t, T_MID, T_SURF);
    }
}

static void snu_panel_groove(sn_ui *u)
{
    const sn_groove *g = u->g;
    static const char *const rows[4] = {"KICK", "SNARE", "HAT", "BASS"};
    uint16_t m[4];
    m[0] = (uint16_t)(g->drum_last[SN_DR_KICK] | g->drum_last[SN_DR_MKICK]);
    m[1] = (uint16_t)(g->drum_last[SN_DR_SNARE] | g->drum_last[SN_DR_CLAP] | g->drum_last[SN_DR_MRIM]);
    m[2] = (uint16_t)(g->drum_last[SN_DR_HAT] | g->drum_last[SN_DR_OPEN] | g->drum_last[SN_DR_MHAT]);
    m[3] = g->hits_last[SN_BASS];
    for (int r = 0; r < 4; ++r) {
        int32_t y = 10 + r * 26;
        snu_label(10, y + 2, rows[r], T_MID);
        for (int k = 0; k < 16; ++k) {
            bool on = g->playing && ((m[r] >> k) & 1);
            int32_t x = 54 + k * 11;
            cv_rrect(x, y, 9, 16, 2, on ? (r == 3 ? T_ACCENT : T_THEME) : snu_well(), T_SURF);
        }
    }
    if (g->playing && g->step >= 0)
        cv_rect(54 + g->step * 11, 112, 9, 2, T_ACCENT);
}

/* the kit: each drum's shape, the gated ones a block held and cut dead; lit as it hits */
static void snu_panel_drums(sn_ui *u)
{
    const sn_groove *g = u->g;
    snu_label(10, 6, g->s.snd[3] == SN_KIT_BIG ? "GATED, AND A BOX FOR THE HUSH" : "FELUCCA'S DRUM ENGINE", T_MID);
    static const char *const nm[5] = {"KICK", "SNARE", "TOMS", "HAT", "CRASH"};
    static const int LEN[5] = {60, 0, 0, 12, 100};          /* (0: gated) */
    static const int GATE[5] = {0, 28, 34, 0, 0};
    bool gated = g->s.snd[3] == SN_KIT_BIG;
    int st = g->step < 0 ? 0 : g->step;
    uint16_t hits[5] = {g->drum_hits[SN_DR_KICK], (uint16_t)(g->drum_hits[SN_DR_SNARE] | g->drum_hits[SN_DR_CLAP]),
                        (uint16_t)(g->drum_hits[SN_DR_TOMH] | g->drum_hits[SN_DR_TOMM] | g->drum_hits[SN_DR_TOML]),
                        (uint16_t)(g->drum_hits[SN_DR_HAT] | g->drum_hits[SN_DR_OPEN]), g->drum_hits[SN_DR_CRASH]};
    for (int i = 0; i < 5; ++i) {
        int32_t x = 10 + i * 45, y = 30, h = 56;
        bool hit = g->playing && (hits[i] >> st) & 1u;
        uint16_t body = ux_mix(T_SURF, T_THEME, hit ? 100 : 70);
        for (int k = 0; k < 40; ++k) {             /* the hit: a hard edge, then its decay or its gated block */
            int32_t a;
            if (!LEN[i] && gated)
                a = k < 2 ? h : k < GATE[i] ? h * (60 - k) / 100 : 0;
            else {
                int len = LEN[i] ? LEN[i] : 30;
                a = k < 2 ? h : h * (len - k * len / 40) / (len > 0 ? len : 1);
                a = a * snu_clampi(len * 2 - k * 3, 0, 100) / 100;
            }
            if (a > 0) cv_rect(x + k, y + h - a, 1, a, k < 2 ? T_ACCENT : body);
        }
        cv_text_on(x, 94, &AF_S, nm[i], T_TEXT, T_SURF);
    }
}

static void snu_panel_mix(sn_ui *u)
{
    const sn_groove *g = u->g;
    for (int i = 0; i < 4; ++i) {
        int32_t x = 32 + i * 50, top = 12, h = 90, v = g->s.lvl[i] * h / 100;
        cv_rrect(x, top, 18, h, 4, snu_well(), T_SURF);
        if (v > 0)
            cv_rrect(x, top + h - v, 18, v, 4, g->muted[i] && g->playing ? T_DIM : i == 3 ? T_TEXT : T_THEME, snu_well());
    }
}

static void snu_panel_space(sn_ui *u)
{
    const sn_groove *g = u->g;
    snu_label(10, 6, "ECHOES ON THE BEAT", T_MID);
    static const int GAP[SN_DTIME_COUNT] = {12, 24, 36, 48};   /* px a sixteenth .. a quarter, a beat 48 px */
    int gap = GAP[g->s.dtime % SN_DTIME_COUNT];
    int n = 1 + g->s.delay / 20;
    for (int k = 0; k <= n && 14 + k * gap < 226; ++k) {
        int32_t x = 14 + k * gap, h = 60 * (n + 1 - k) / (n + 1);
        cv_rrect(x, 90 - h, 8, h, 2, k == 0 ? T_THEME : ux_mix(T_SURF, T_THEME, 80 - k * 10), T_SURF);
    }
    for (int b = 0; b < 5; ++b)
        cv_rect(14 + b * 48, 96, 1, 6, T_MID);
}

/* the form: a column a section, as tall as it is loud, the one playing in the accent, the key up over the second full */
static void snu_panel_form(sn_ui *u)
{
    const sn_groove *g = u->g;
    if (!g->s.form) {
        snu_label(10, 30, "FORM OFF", T_THEME);
        snu_label(10, 50, "THE FULL BAND ON AND ON:", T_MID);
        snu_label(10, 66, "REC FILLS, SEQ BRINGS A DRAMA", T_MID);
        return;
    }
    static const int LVL[SN_SECS] = {20, 42, 70, 100, 34, 80, 100, 24};
    int32_t x[SN_SECS], w[SN_SECS];
    bool tiny;
    snu_form_xs(x, w, &tiny);
    for (int k = 0; k < SN_SECS; ++k) {
        int32_t h = LVL[k] * 70 / 100, top = 90 - h, ww = w[k] > 12 ? w[k] : 12;
        bool now = g->playing && g->sec == k, past = g->playing && k < g->sec;
        uint16_t c = now ? T_ACCENT : past || !g->playing ? T_THEME : ux_mix(T_SURF, T_THEME, 45);
        cv_rect(x[k], top, ww, h, c);
        if (now) {                                 /* how far into it */
            int len = sn_sec_len(g, k);
            int32_t p = len > 0 ? (g->sec_bar * 16 + (g->step < 0 ? 0 : g->step)) * h / (len * 16) : h;
            cv_rect(x[k], 90 - p, ww, 2, T_TEXT);
        }
        cv_text_on(x[k], 98, &AF_S, tiny ? SNU_SEC_TINY[k] : sn_sec_short(k), now ? T_ACCENT : T_MID, T_SURF);
        if (k == SN_SEC_FULL2 && g->s.keyup) {
            char t[4] = "+1";
            t[1] = (char)('0' + g->s.keyup);
            cv_text_in(x[k], top - 15, ww, &AF_S, t, T_THEME, T_SURF);
        }
    }
}

/* the riff as a roll: REPEAT bars of its notes, low to high, the step playing marked; an arpeggio as a zigzag */
static void snu_panel_keys(sn_ui *u)
{
    const sn_groove *g = u->g;
    char t[32], a[8];
    int rep = sn_repeat_bars(&g->s);
    bool arp = SN_STYLE[g->s.style].hook == 1;     /* (HOOK_ARP) */
    if (arp) {
        snu_cpy(t, "AN ARPEGGIO THROUGH THE CHORD", sizeof t);
    } else {
        snu_cpy(t, "THE RIFF, ", sizeof t);
        snu_fmt_int(a, rep);
        snu_cat(t, a, sizeof t);
        snu_cat(t, rep == 1 ? " BAR" : " BARS", sizeof t);
    }
    snu_label(10, 4, t, T_MID);
    int32_t x0 = 10, w = 220, y0 = 24, h = 76, n = rep * 16;
    int lo = g->s.range >= 2 ? -3 : 0, hi = g->s.range >= 2 ? 10 : 7;
    for (int b = 0; b <= rep; ++b)
        cv_rect(x0 + b * w / rep - (b == rep), y0, 1, h, T_LINE);
    int pos = -1;
    if (g->playing && g->step >= 0 && sn_in_full(g))
        pos = (g->sec_bar % rep) * 16 + g->step;
    if (arp) {
        static const uint8_t ORD[8] = {0, 1, 2, 3, 4, 3, 2, 1};
        for (int k = 0; k < n; ++k) {
            int32_t x = x0 + k * w / n, y = y0 + h - 8 - ORD[k % 8] * (h - 12) / 4;
            cv_rrect(x + 1, y, w / n - 2 > 2 ? w / n - 2 : 2, 5, 2, k == pos ? T_ACCENT : T_THEME, T_SURF);
        }
    } else {
        for (int k = 0; k < n; ++k) {
            if (g->hook[k] == -128)
                continue;
            int len = g->hook_len[k];
            int32_t x = x0 + k * w / n, ww = len * w / n - 2;
            int32_t y = y0 + h - 7 - (snu_clampi(g->hook[k], lo, hi) - lo) * (h - 10) / (hi - lo > 0 ? hi - lo : 1);
            bool on = pos >= k && pos < k + len;
            cv_rrect(x + 1, y, ww > 2 ? ww : 2, 5, 2, on ? T_ACCENT : T_THEME, T_SURF);
        }
    }
    if (pos >= 0)
        cv_rect(x0 + pos * w / n, y0 + h + 3, w / n > 2 ? w / n : 2, 2, T_ACCENT);
    if (g->playing && !sn_in_full(g))
        cv_text_r(230, 4, &AF_S, "WAITS FOR THE FULL BAND", T_DIM, T_SURF);
}

static void snu_panel_sets(sn_ui *u)
{
    for (int i = 0; i < SN_SETS; ++i) {
        int32_t col = i / 4, row = i % 4, x = 10 + col * 112, y = 8 + row * 27;
        bool sel = i == u->set_sel, used = (u->set_used >> i) & 1u;
        if (sel) cv_rrect(x - 4, y - 4, 110, 21, 3, T_THEME, T_SURF);
        char n[4];
        snu_fmt_int(n, i + 1);
        uint16_t bg = sel ? T_THEME : T_SURF;
        cv_text_on(x, y, &AF_S, n, sel ? T_INK : T_MID, bg);
        cv_text_fit(x + 14, y, &AF_S, used ? sn_style_name(u->sets[i].style) : "EMPTY", sel ? T_INK : used ? T_TEXT : T_DIM, bg, 88);
    }
}

static void snu_panel_menu(sn_ui *u)
{
    (void)u;
    cv_text_on(30, 20, &AF_M, "SNAPPER " SN_VERSION, T_TEXT, T_SURF);
    cv_text_on(30, 44, &AF_S, "80S DRUM-HEAVY POP, FM-1", T_MID, T_SURF);
    cv_text_on(30, 64, &AF_S, "ON FELUCCA, AFTER TUNA", T_MID, T_SURF);
    cv_text_on(30, 84, &AF_S, "GPL-3.0, HOME CLOSES", T_DIM, T_SURF);
}

static void snu_draw_panel(sn_ui *u)
{
    snu_panel_begin();
    if (u->browse_ms > 0) {
        snu_panel_styles(u);
    } else {
        switch (u->page) {
        case SNU_PG_PLAY: snu_panel_play(u); break;
        case SNU_PG_SOUND: snu_panel_sound(u); break;
        case SNU_PG_TONE: snu_panel_tone(u); break;
        case SNU_PG_KEY: snu_panel_key(u); break;
        case SNU_PG_GROOVE: snu_panel_groove(u); break;
        case SNU_PG_DRUMS: snu_panel_drums(u); break;
        case SNU_PG_MIX: snu_panel_mix(u); break;
        case SNU_PG_SPACE: snu_panel_space(u); break;
        case SNU_PG_FORM: snu_panel_form(u); break;
        case SNU_PG_KEYS: snu_panel_keys(u); break;
        case SNU_PG_SETS: snu_panel_sets(u); break;
        case SNU_PG_MENU: snu_panel_menu(u); break;
        }
    }
    cv_blit(0, SNU_PANEL_Y);
}

static void snu_background(void)
{
    cv_begin(240, 40, T_BG);
    for (int y = 0; y < 240; y += 40)
        cv_blit(0, (uint32_t)y);
    if (ux.style == ST_LINE) {                    /* LINE: no cards, 1 px rules between the areas */
        cv_begin(240, 1, T_RULE);
        cv_blit(0, SNU_HEAD + 1);
        cv_blit(0, SNU_CARD_Y + SNU_CARD_H + 1);
        cv_blit(0, SNU_FOOT_Y - 3);
        cv_begin(1, SNU_CARD_H, T_RULE);
        for (int c = 1; c < 4; ++c)
            cv_blit((uint32_t)(SNU_CARD_X(c) - 2), SNU_CARD_Y);
    }
}

void snu_draw(sn_ui *u)
{
    if (u->force)
        snu_background();
    snu_draw_head(u);
    for (int i = 0; i < 4; ++i)
        snu_draw_card(u, i, &SNU_CARDS[u->page][i]);
    snu_draw_panel(u);
    snu_draw_foot(u);
    u->force = false;
    u->frame++;
}

void snu_draw_all(sn_ui *u)
{
    u->force = true;
    snu_draw(u);
}

/* ---- input --------------------------------------------------------------------------------------------------- */
static void snu_msg(sn_ui *u, const char *m)
{
    snu_cpy(u->msg, m, sizeof u->msg);
    u->msg_ms = 1500;
}

static void snu_goto_page(sn_ui *u, int pg)
{
    if (pg != u->page) {
        u->page = pg;
        u->force = true;
    }
    u->browse_ms = 0;
}

static void snu_apply_style(sn_ui *u)
{
    ux.style = (uint8_t)(u->g->s.look ? ST_LINE : ST_FLAT);
    palette_set((uint32_t)u->g->s.palette % NPALETTES);
    u->force = true;
}

void snu_init(sn_ui *u, sn_groove *g)
{
    memset(u, 0, sizeof *u);
    u->g = g;
    u->page = SNU_PG_PLAY;
    u->hot = -1;
    u->batt = 3;
    for (int k = 0; k < 27; ++k)
        u->key_sent[k] = -1;
    snu_apply_style(u);
}

/* a set loaded: its settings, the menu's colour and look stay; at once (the sound follows in the glue) */
static void snu_load_set(sn_ui *u, int i)
{
    sn_groove *g = u->g;
    sn_settings s = u->sets[i];
    s.palette = g->s.palette;
    s.look = g->s.look;
    sn_settings_fix(&s);
    sn_all_off(g);
    g->s = s;
    g->next_style = g->next_song = -1;
    g->tr = 0;
    sn_rebuild(g);
    g->sound_sig++;
}

void snu_knob(sn_ui *u, int k, int d)
{
    sn_groove *g = u->g;
    char m[24], a[8];
    switch (k) {
    case SNU_K_SELECT:
        g->s.bpm = (uint8_t)snu_clampi(g->s.bpm + d, 60, 200);
        u->hot = 9;
        u->hot_ms = 700;
        u->changed = true;
        return;
    case SNU_K_PRESETS: {
        int cur = g->next_style >= 0 ? g->next_style : g->s.style;
        sn_queue_style(g, snu_wrap(cur + d, SN_STYLES));
        u->browse_ms = 1800;
        u->changed = true;
        return;
    }
    case SNU_K_ALGO: {
        int cur = g->next_song >= 0 ? g->next_song : g->s.song;
        sn_queue_song(g, snu_wrap(cur + d, 1000));
        snu_cpy(m, g->playing ? "NEXT SONG " : "SONG ", sizeof m);
        snu_fmt_int(a, snu_wrap(cur + d, 1000) + 1);
        snu_cat(m, a, sizeof m);
        snu_msg(u, m);
        u->changed = true;
        return;
    }
    case SNU_K1: case SNU_K2: case SNU_K3: case SNU_K4: {
        const snu_cdef *c = &SNU_CARDS[u->page][k - SNU_K1];
        u->hot = k - SNU_K1;
        u->hot_ms = 700;
        if (c->kind == SC_OFF)
            return;
        if (c->kind == SC_LOAD) {                  /* turned right: the set, at once */
            if (d > 0 && ((u->set_used >> u->set_sel) & 1u)) {
                snu_load_set(u, u->set_sel);
                snu_cpy(m, "SET ", sizeof m);
                snu_fmt_int(a, u->set_sel + 1);
                snu_cat(m, a, sizeof m);
                snu_cat(m, " LOADED", sizeof m);
                snu_msg(u, m);
                snu_apply_style(u);
                u->changed = true;
            } else if (d > 0) {
                snu_msg(u, "THAT SET IS EMPTY");
            }
            return;
        }
        if (c->kind == SC_STORE) {                 /* turned right: what plays now into the set */
            if (d > 0) {
                u->sets[u->set_sel] = g->s;
                u->set_used |= (uint8_t)(1u << u->set_sel);
                u->sets_changed = true;
                snu_cpy(m, "STORED IN SET ", sizeof m);
                snu_fmt_int(a, u->set_sel + 1);
                snu_cat(m, a, sizeof m);
                snu_msg(u, m);
            }
            return;
        }
        if (c->kind == SC_CLEAR) {                 /* three turns right within a second */
            if (d > 0) {
                u->clear_turns++;
                u->clear_ms = 1000;
                if (u->clear_turns >= 3) {
                    u->set_used &= (uint8_t)~(1u << u->set_sel);
                    u->sets_changed = true;
                    u->clear_turns = 0;
                    snu_msg(u, "SET CLEARED");
                } else {
                    snu_msg(u, "TURN ON TO CLEAR");
                }
            }
            return;
        }
        int v = snu_get(u, c) + d * c->step;
        if (c->kind == SC_PAL)
            v = snu_wrap(v, (int)NPALETTES);
        else if (c->kind == SC_SND || c->kind == SC_KEY || c->kind == SC_LOOP)
            v = c->lo + snu_wrap(v - c->lo, c->hi - c->lo + 1);
        v = snu_clampi(v, c->lo, c->hi);
        snu_set(u, c, v);
        if (c->off == SS_(key) || c->off == SS_(scale) || c->off == SS_(chords) || c->off == SS_(bars) ||
            c->off == SS_(speed) || c->off == SS_(range) || c->off == SS_(repeat))
            sn_rebuild(g);
        if (c->off == SS_(scale))
            snu_msg(u, SN_SCALE_LONG[g->s.scale % SN_SCALES]);
        if (c->off == SS_(palette) || c->off == SS_(look))
            snu_apply_style(u);
        if (c->off == SS_(form) && g->playing && g->s.form && !sn_in_full(g)) {   /* FORM on while playing: */
            g->sec = SN_SEC_FULL;                  /* from the full band */
            g->sec_bar = 0;
        }
        u->changed = true;
        return;
    }
    default:
        return;
    }
}

void snu_button(sn_ui *u, int b, bool down)
{
    sn_groove *g = u->g;
    if (b == SNU_B_HOME) {
        if (down) {
            u->home_down = true;
            u->home_ms = 0;
        } else if (u->home_down) {
            u->home_down = false;
            if (u->home_ms < SNU_MENU_HOLD_MS) snu_goto_page(u, SNU_PG_PLAY);
        }
        return;
    }
    if (b == SNU_B_REC) {                           /* BREAK-IN: down and up both count */
        if (!g->playing) {
            if (down) snu_msg(u, "PLAY FIRST");
            return;
        }
        sn_breakin(g, down);
        return;
    }
    if (!down)
        return;
    switch (b) {
    case SNU_B_PLAY: sn_play(g, !g->playing); break;
    case SNU_B_SEQ:
        if (g->playing) sn_drama(g);
        else snu_msg(u, "PLAY FIRST");
        break;
    case SNU_B_OCTDN: case SNU_B_OCTUP: {
        char m[16], a[8];
        u->oct = snu_clampi(u->oct + (b == SNU_B_OCTUP ? 1 : -1), -2, 2);
        snu_cpy(m, "KEYS OCT ", sizeof m);
        if (u->oct > 0) snu_cat(m, "+", sizeof m);
        snu_fmt_int(a, u->oct);
        snu_cat(m, a, sizeof m);
        snu_msg(u, m);
        break;
    }
    case SNU_B_FX: snu_goto_page(u, u->page == SNU_PG_SOUND ? SNU_PG_TONE : SNU_PG_SOUND); break;
    case SNU_B_LFO: snu_goto_page(u, SNU_PG_KEYS); break;
    case SNU_B_SCL: snu_goto_page(u, SNU_PG_KEY); break;
    case SNU_B_ARP: snu_goto_page(u, SNU_PG_GROOVE); break;
    case SNU_B_EDIT: snu_goto_page(u, SNU_PG_DRUMS); break;
    case SNU_B_GLO: snu_goto_page(u, u->page == SNU_PG_MIX ? SNU_PG_SPACE : SNU_PG_MIX); break;
    case SNU_B_ENV: snu_goto_page(u, u->page == SNU_PG_FORM ? SNU_PG_KEYS : SNU_PG_FORM); break;
    case SNU_B_SAVE: snu_goto_page(u, SNU_PG_SETS); break;
    default: break;
    }
}

void snu_key(sn_ui *u, int note, bool down)
{
    if (note < SNU_KEY_LO || note > SNU_KEY_HI)
        return;
    int k = note - SNU_KEY_LO;
    uint32_t bit = 1u << k;
    if (down) {
        u->keys_down |= bit;
        u->key_sent[k] = note + 12 * u->oct;      /* (the octave it was pressed in: its release lets go of the same) */
        sn_key(u->g, k, u->key_sent[k], true);
    } else {
        u->keys_down &= ~bit;
        sn_key(u->g, k, u->key_sent[k], false);
        u->key_sent[k] = -1;
    }
}

void snu_tick(sn_ui *u, int ms)
{
    sn_groove *g = u->g;
    sn_seen n;
    while (sn_feed_pop(g, &n))
        ;                                          /* (the panels read the groove's own state) */
    if (g->msg_new) {
        g->msg_new = 0;
        snu_msg(u, g->msg);
    }
    if (u->hot_ms > 0) u->hot_ms -= ms;
    if (u->clear_ms > 0 && (u->clear_ms -= ms) <= 0) u->clear_turns = 0;
    if (u->browse_ms > 0 && (u->browse_ms -= ms) <= 0) {
        u->browse_ms = 0;
        u->force = true;
    }
    if (u->msg_ms > 0) u->msg_ms -= ms;
    if (u->home_down) {
        u->home_ms += ms;
        if (u->home_ms >= SNU_MENU_HOLD_MS && u->page != SNU_PG_MENU) {
            snu_goto_page(u, SNU_PG_MENU);
            u->home_down = false;
        }
    }
}
