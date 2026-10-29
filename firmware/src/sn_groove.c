/* SPDX-License-Identifier: GPL-3.0-only */
/* SNAPPER groove engine (see sn_groove.h). The styles, the chord loops, the parts' rhythm families, the riff, the
 * form of tension; in time, sixteenth by sixteenth, with swing and a human feel; the lift every block. */
#include "sn_groove.h"

#include "tuna_sys.h"   /* memcpy and memset: the FM-1 has no C library (TUNA_FREESTANDING) */

/* ---- small helpers ---------------------------------------------------------------------------------------- */
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static uint32_t xs(uint32_t *s)
{
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}
static int rnd_n(uint32_t *s, int n) { return n > 0 ? (int)(xs(s) % (uint32_t)n) : 0; }
static float rnd_f(uint32_t *s) { return (float)(xs(s) >> 8) * (1.0f / 16777216.0f); }   /* 0..1 */

static void cpy(char *d, const char *s, int n)
{
    int i = 0;
    for (; s[i] && i < n - 1; ++i)
        d[i] = s[i];
    d[i] = 0;
}

/* ---- names ------------------------------------------------------------------------------------------------ */
static const char *const NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const char *sn_note_name(int pc) { return NOTE_NAMES[((pc % 12) + 12) % 12]; }
static const char *const SCALE_NAMES[SN_SCALES] = {"MIN", "MAJ", "DOR", "MIXO"};
const char *sn_scale_name(int i) { return SCALE_NAMES[clampi(i, 0, SN_SCALES - 1)]; }
static const char *const SEC_NAMES[SN_SECS] = {"HUSH", "RISE", "BREAK-IN", "FULL", "DRAMA", "BREAK-IN", "FULL", "END"};
static const char *const SEC_SHORT[SN_SECS] = {"HUSH", "RISE", "IN", "FULL", "DRAMA", "IN", "FULL", "END"};
const char *sn_sec_name(int s) { return SEC_NAMES[clampi(s, 0, SN_SECS - 1)]; }
const char *sn_sec_short(int s) { return SEC_SHORT[clampi(s, 0, SN_SECS - 1)]; }

/* the scales, as semitones from the key */
static const uint8_t SCALE[SN_SCALES][7] = {
    {0, 2, 3, 5, 7, 8, 10},   /* MINOR (aeolian): the brooding home */
    {0, 2, 4, 5, 7, 9, 11},   /* MAJOR: the radio */
    {0, 2, 3, 5, 7, 9, 10},   /* DORIAN: the night drive */
    {0, 2, 4, 5, 7, 9, 10},   /* MIXOLYDIAN: the rock's flat seventh */
};
/* scale step k (any integer, 7 an octave up) of scale sc from MIDI note base (the key's tonic) */
static int step_note(int sc, int base, int k)
{
    int o = k >= 0 ? k / 7 : -((-k + 6) / 7);
    int d = k - o * 7;
    return base + o * 12 + SCALE[sc][d];
}

int sn_snap(const sn_settings *s, int note)
{
    int sc = clampi(s->scale, 0, SN_SCALES - 1), pc = ((note - s->key) % 12 + 12) % 12, best = 0, bd = 99;
    for (int i = 0; i < 7; ++i) {
        int d = SCALE[sc][i] - pc;                /* ties go down: the step below first */
        int ad = d < 0 ? -d : d;
        if (ad < bd || (ad == bd && d < 0)) {
            bd = ad;
            best = d;
        }
    }
    return note + best;
}

/* ---- the chord loops: scale degrees (0 = the key's chord), a flavour (3 triads, 7 sevenths) and whether the fifth
 * degree takes a major third (the dominant in minor) ---- */
typedef struct { uint8_t n, deg[4], flavour, dom; } sn_loopdef;
static const sn_loopdef SN_LOOP[SN_LOOPS] = {
    {4, {0, 5, 2, 6}, 3, 0},   /* 1  i VI III VII: the anthem in minor */
    {4, {0, 5, 6, 6}, 3, 0},   /* 2  i VI VII VII: the long climb */
    {4, {0, 6, 5, 6}, 3, 0},   /* 3  i VII VI VII */
    {4, {5, 6, 0, 0}, 3, 0},   /* 4  VI VII i i: the lift into the tonic */
    {4, {0, 3, 5, 4}, 3, 1},   /* 5  i iv VI V: the drama */
    {4, {0, 4, 5, 3}, 3, 0},   /* 6  I V vi IV: the radio */
    {4, {0, 3, 4, 3}, 3, 0},   /* 7  I IV V IV: the stadium */
    {4, {0, 6, 3, 0}, 3, 0},   /* 8  I bVII IV I: the rock (mixolydian's flat seven) */
    {4, {0, 5, 3, 4}, 7, 1},   /* 9  I7 vi7 IV7 V7: the ballad's sevenths */
    {4, {0, 0, 5, 5}, 3, 0},   /* 10 i i VI VI: the drone, two chords, long */
    {4, {0, 5, 0, 6}, 3, 0},   /* 11 i VI i VII: the night */
    {4, {0, 3, 0, 4}, 3, 1},   /* 12 i iv i V */
};
void sn_loop_name(const sn_settings *s, int loop, char *out, int max)
{
    (void)s;
    char b[12] = "LOOP ";
    int v = clampi(loop, 0, SN_LOOPS - 1) + 1, k = 5;
    if (v >= 10) b[k++] = (char)('0' + v / 10);
    b[k++] = (char)('0' + v % 10);
    b[k] = 0;
    cpy(out, b, max);
}

/* ---- the styles ------------------------------------------------------------------------------------------- */
/* the rhythm families: the pads in the full band, the riff, the full kit's groove, the drum machine's pattern */
enum { PAD_HELD, PAD_PULSE, PAD_STAB, PAD_COMP, PAD_COUNT };
enum { HOOK_RIFF, HOOK_ARP, HOOK_SPARSE };
enum { DRM_ROCK, DRM_STOMP, DRM_FUNK, DRM_MACHINE, DRM_HALF, DRM_TOMS, DRM_BOX, DRM_COUNT };
enum { MP_CR, MP_PULSE, MP_TICK, MP_COUNT };
#define P(x) (1u << (x))

/* name side bpm key scale | build drama energy room | snd[4] | loops | bars swing hats fills bassline |
 * kick clap crash | pad hook drums machine | speed range gate repeat | lift lift-parts | reverb delay dtime |
 * reso drive ensemble | the three more sounds of PADS, BASS and KEYS a song picks from (sn_song_sounds).
 * Sounds (sn_sounds.h): PADS 0 DRONE 1 CHOIR 2 STRINGS 3 PWM 4 TINE 5 BRASS 6 PIANO 7 GLASS 8 SOFT 9 ORGAN
 * 10 SYNTH 11 BELL 12 HORNS 13 ORCH 14 MALLET 15 VOICES; BASS 0 PUNCH 1 FM BASS 2 FRETLESS 3 SYNTH 4 SQUARE 5 SUB
 * 6 SLAP 7 OCTAVE 8 DX 9 FAT 10 SINE 11 GROWL 12 ORGAN 13 REESE 14 WOW 15 ACID; KEYS 0 TINE 1 BRASS 2 MALLET 3 SYNC
 * 4 LEAD 5 HORNS 6 PIANO 7 FLUTE 8 POP 9 ANTHEM 10 ORCH 11 CHOIR 12 WHISTLE 13 PLUCK 14 ORGAN 15 SUPER;
 * the kit 0 BIG 1 MACHINE */
const sn_style SN_STYLE[SN_STYLES] = {
    {"SLOWBURN", SN_SIDE_LOUD, 96, 2, SN_SC_MINOR, SN_BUILD_16, SN_DRAMA_AUTO, 70, 85, {0, 1, 6, 0}, {9, 1, 4, 10}, 2,
     0, SN_HATS_8THS, SN_FILLS_8, SN_BL_LONG, SN_KICK_PUNCH, SN_CLAP_SNARE, SN_CRASH_SECTION,
     PAD_HELD, HOOK_SPARSE, DRM_ROCK, MP_CR, SN_SPEED_SLOW, 1, 90, 4, 85, P(SN_PADS), 40, 30, SN_DTIME_3_16,
     20, 5, 40, {{7, 1, 8}, {5, 10, 2}, {3, 11, 4}}},
    {"BIGROOM", SN_SIDE_LOUD, 118, 4, SN_SC_MINOR, SN_BUILD_8, SN_DRAMA_AUTO, 85, 90, {10, 3, 9, 0}, {0, 2, 1, 3}, 1,
     0, SN_HATS_8THS, SN_FILLS_8, SN_BL_PULSE, SN_KICK_PUNCH, SN_CLAP_SNARE, SN_CRASH_SECTION,
     PAD_PULSE, HOOK_RIFF, DRM_ROCK, MP_PULSE, SN_SPEED_MID, 1, 80, 2, 90, P(SN_PADS) | P(SN_KEYS), 30, 20, SN_DTIME_8,
     25, 15, 35, {{3, 2, 12}, {7, 0, 9}, {15, 4, 3}}},
    {"BRASSHIT", SN_SIDE_LOUD, 124, 5, SN_SC_MAJOR, SN_BUILD_8, SN_DRAMA_AUTO, 90, 70, {5, 6, 0, 0}, {5, 6, 7, 3}, 1,
     0, SN_HATS_16THS, SN_FILLS_4, SN_BL_SYNCO, SN_KICK_PUNCH, SN_CLAP_BOTH, SN_CRASH_SECTION,
     PAD_STAB, HOOK_RIFF, DRM_FUNK, MP_TICK, SN_SPEED_FAST, 1, 60, 2, 90, P(SN_KEYS), 25, 15, SN_DTIME_8,
     15, 10, 25, {{12, 13, 4}, {8, 0, 3}, {1, 8, 13}}},
    {"STADIUM", SN_SIDE_LOUD, 128, 7, SN_SC_MAJOR, SN_BUILD_16, SN_DRAMA_AUTO, 85, 95, {1, 7, 4, 0}, {6, 5, 7, 8}, 1,
     0, SN_HATS_8THS, SN_FILLS_8, SN_BL_OCTAVE, SN_KICK_PUNCH, SN_CLAP_SNARE, SN_CRASH_8,
     PAD_HELD, HOOK_RIFF, DRM_STOMP, MP_PULSE, SN_SPEED_MID, 2, 85, 2, 90, P(SN_PADS) | P(SN_KEYS), 45, 25, SN_DTIME_3_16,
     20, 10, 45, {{2, 7, 15}, {3, 0, 1}, {9, 1, 3}}},
    {"MACHINE", SN_SIDE_LOUD, 100, 6, SN_SC_MINOR, SN_BUILD_8, SN_DRAMA_AUTO, 75, 60, {0, 4, 3, 0}, {9, 11, 2, 0}, 2,
     0, SN_HATS_8THS, SN_FILLS_4, SN_BL_SEQ, SN_KICK_PUNCH, SN_CLAP_CLAP, SN_CRASH_SECTION,
     PAD_HELD, HOOK_SPARSE, DRM_MACHINE, MP_PULSE, SN_SPEED_SLOW, 1, 70, 4, 80, P(SN_PADS) | P(SN_BASS), 30, 35, SN_DTIME_3_16,
     45, 35, 20, {{3, 15, 10}, {11, 5, 13}, {4, 13, 12}}},
    {"NIGHTDRIVE", SN_SIDE_NIGHT, 112, 9, SN_SC_DORIAN, SN_BUILD_16, SN_DRAMA_AUTO, 65, 75, {3, 3, 6, 0}, {10, 0, 3, 1}, 1,
     0, SN_HATS_16THS, SN_FILLS_8, SN_BL_PULSE, SN_KICK_ROUND, SN_CLAP_SNARE, SN_CRASH_SECTION,
     PAD_PULSE, HOOK_ARP, DRM_ROCK, MP_TICK, SN_SPEED_MID, 2, 60, 2, 85, P(SN_PADS) | P(SN_BASS), 35, 40, SN_DTIME_3_16,
     35, 10, 50, {{7, 10, 2}, {4, 7, 8}, {11, 3, 13}}},
    {"LATENIGHT", SN_SIDE_NIGHT, 72, 3, SN_SC_MAJOR, SN_BUILD_16, SN_DRAMA_AUTO, 45, 85, {4, 2, 6, 0}, {8, 5, 6, 0}, 2,
     0, SN_HATS_8THS, SN_FILLS_8, SN_BL_SLIDE, SN_KICK_ROUND, SN_CLAP_SNARE, SN_CRASH_SECTION,
     PAD_COMP, HOOK_SPARSE, DRM_HALF, MP_CR, SN_SPEED_SLOW, 2, 95, 4, 80, P(SN_PADS), 50, 25, SN_DTIME_4,
     10, 0, 50, {{6, 8, 7}, {10, 1, 5}, {12, 11, 7}}},
    {"UNDERTOW", SN_SIDE_NIGHT, 84, 2, SN_SC_DORIAN, SN_BUILD_32, SN_DRAMA_AUTO, 35, 80, {0, 5, 11, 0}, {9, 10, 1, 4}, 2,
     0, SN_HATS_8THS, SN_FILLS_OFF, SN_BL_LONG, SN_KICK_ROUND, SN_CLAP_SNARE, SN_CRASH_SECTION,
     PAD_HELD, HOOK_SPARSE, DRM_HALF, MP_TICK, SN_SPEED_SLOW, 1, 95, 4, 70, P(SN_PADS), 60, 35, SN_DTIME_4,
     30, 5, 55, {{1, 7, 15}, {10, 2, 13}, {6, 7, 12}}},
    {"TOMTOM", SN_SIDE_NIGHT, 104, 4, SN_SC_MINOR, SN_BUILD_8, SN_DRAMA_AUTO, 80, 85, {14, 8, 2, 0}, {10, 0, 11, 2}, 2,
     0, SN_HATS_8THS, SN_FILLS_4, SN_BL_SYNCO, SN_KICK_PUNCH, SN_CLAP_CLAP, SN_CRASH_SECTION,
     PAD_PULSE, HOOK_RIFF, DRM_TOMS, MP_CR, SN_SPEED_MID, 1, 55, 2, 85, P(SN_PADS), 35, 20, SN_DTIME_8,
     20, 10, 30, {{2, 0, 8}, {1, 9, 6}, {6, 13, 0}}},
    {"RHYTHMBOX", SN_SIDE_NIGHT, 98, 0, SN_SC_MIXO, SN_BUILD_16, SN_DRAMA_AUTO, 55, 50, {8, 1, 8, 0}, {7, 6, 5, 2}, 1,
     0, SN_HATS_8THS, SN_FILLS_8, SN_BL_SEQ, SN_KICK_ROUND, SN_CLAP_SNARE, SN_CRASH_SECTION,
     PAD_HELD, HOOK_RIFF, DRM_BOX, MP_CR, SN_SPEED_MID, 1, 65, 2, 80, P(SN_PADS), 30, 30, SN_DTIME_3_16,
     15, 5, 40, {{9, 2, 3}, {10, 8, 12}, {2, 7, 13}}},
};
const char *sn_style_name(int i) { return SN_STYLE[clampi(i, 0, SN_STYLES - 1)].name; }

int sn_bars_per_chord(const sn_settings *s) { return s->bars >= 4 ? 4 : s->bars >= 2 ? 2 : 1; }
int sn_repeat_bars(const sn_settings *s) { return s->repeat >= 4 ? 4 : s->repeat >= 2 ? 2 : 1; }
int sn_build_bars(const sn_settings *s)
{
    static const uint8_t B[SN_BUILD_COUNT] = {8, 16, 32, 8};
    return B[clampi(s->build, 0, SN_BUILD_COUNT - 1)];
}

/* ---- settings --------------------------------------------------------------------------------------------- */
void sn_style_apply(sn_settings *s, int style)
{
    const sn_style *st = &SN_STYLE[clampi(style, 0, SN_STYLES - 1)];
    s->style = (uint8_t)clampi(style, 0, SN_STYLES - 1);
    s->build = st->build;
    s->drama = st->drama;
    s->energy = st->energy;
    s->room = st->room;
    for (int i = 0; i < 4; ++i)
        s->snd[i] = st->snd[i];
    s->reso = st->reso;
    s->drive = st->drive;
    s->ensemble = st->ensemble;
    s->crush = 0;
    s->key = st->key;
    s->scale = st->scale;
    s->chords = st->loops[0];
    s->bars = st->bars;
    s->swing = st->swing;
    s->human = 10;
    s->hats = st->hats;
    s->fills = st->fills;
    s->bassline = st->bassline;
    s->kick = st->kick;
    s->clap = st->clap;
    s->crash = st->crash;
    s->reverb = st->reverb;
    s->delay = st->delay;
    s->dtime = st->dtime;
    s->width = 70;
    s->speed = st->speed;
    s->range = st->range;
    s->gate = st->gate;
    s->repeat = st->repeat;
    s->bpm = st->bpm;
    s->lift = st->lift;
    s->song = 0;
}

void sn_song_sounds(sn_settings *s)
{
    const sn_style *st = &SN_STYLE[clampi(s->style, 0, SN_STYLES - 1)];
    uint32_t r = (uint32_t)s->song * 2654435761u + (uint32_t)s->style * 40503u + 7u;
    for (int p = 0; p < 3; ++p) {
        int k = s->song ? rnd_n(&r, 4) : 0;        /* 0: the style's own */
        s->snd[p] = k ? st->alt[p][k - 1] : st->snd[p];
    }
}

void sn_defaults(sn_settings *s)
{
    memset(s, 0, sizeof *s);
    sn_style_apply(s, SN_ST_SLOWBURN);
    s->lvl[SN_PADS] = 85;                          /* (the sounds are evened out: sn_sounds.h trims) */
    s->lvl[SN_BASS] = 90;
    s->lvl[SN_KEYS] = 85;
    s->lvl[SN_DRUMS] = 90;
    s->form = 1;
    s->full = 16;
    s->dramas = 1;
    s->keyup = 2;
    s->palette = 10;                               /* GATE (ui_palettes.h UI_GATE_INDEX) */
    s->look = 0;
}

void sn_settings_fix(sn_settings *s)
{
    s->style = (uint8_t)clampi(s->style, 0, SN_STYLES - 1);
    s->build = (uint8_t)clampi(s->build, 0, SN_BUILD_COUNT - 1);
    s->drama = (uint8_t)clampi(s->drama, 0, SN_DRAMA_COUNT - 1);
    s->energy = (uint8_t)clampi(s->energy, 0, 100);
    s->room = (uint8_t)clampi(s->room, 0, 100);
    for (int i = 0; i < 3; ++i)
        s->snd[i] = (uint8_t)clampi(s->snd[i], 0, SN_SOUNDS - 1);
    s->snd[3] = (uint8_t)clampi(s->snd[3], 0, SN_KIT_COUNT - 1);
    s->reso = (uint8_t)clampi(s->reso, 0, 100);
    s->drive = (uint8_t)clampi(s->drive, 0, 100);
    s->ensemble = (uint8_t)clampi(s->ensemble, 0, 100);
    s->crush = (uint8_t)clampi(s->crush, 0, 100);
    s->key = (uint8_t)clampi(s->key, 0, 11);
    s->scale = (uint8_t)clampi(s->scale, 0, SN_SCALES - 1);
    s->chords = (uint8_t)clampi(s->chords, 0, SN_LOOPS - 1);
    s->bars = (uint8_t)(s->bars >= 4 ? 4 : s->bars >= 2 ? 2 : 1);
    s->swing = (uint8_t)clampi(s->swing, 0, 60);
    s->human = (uint8_t)clampi(s->human, 0, 100);
    s->hats = (uint8_t)clampi(s->hats, 0, SN_HATS_COUNT - 1);
    s->fills = (uint8_t)clampi(s->fills, 0, SN_FILLS_COUNT - 1);
    s->bassline = (uint8_t)clampi(s->bassline, 0, SN_BL_COUNT - 1);
    s->kick = (uint8_t)clampi(s->kick, 0, SN_KICK_COUNT - 1);
    s->clap = (uint8_t)clampi(s->clap, 0, SN_CLAP_COUNT - 1);
    s->crash = (uint8_t)clampi(s->crash, 0, SN_CRASH_COUNT - 1);
    for (int i = 0; i < 4; ++i)
        s->lvl[i] = (uint8_t)clampi(s->lvl[i], 0, 100);
    s->reverb = (uint8_t)clampi(s->reverb, 0, 100);
    s->delay = (uint8_t)clampi(s->delay, 0, 100);
    s->dtime = (uint8_t)clampi(s->dtime, 0, SN_DTIME_COUNT - 1);
    s->width = (uint8_t)clampi(s->width, 0, 100);
    s->form = (uint8_t)(s->form ? 1 : 0);
    s->full = (uint8_t)(s->full >= 16 ? 16 : 8);
    s->dramas = (uint8_t)clampi(s->dramas, 1, 2);
    s->keyup = (uint8_t)clampi(s->keyup, 0, 2);
    s->speed = (uint8_t)clampi(s->speed, 0, SN_SPEED_COUNT - 1);
    s->range = (uint8_t)clampi(s->range, 1, 2);
    s->gate = (uint8_t)clampi(s->gate, 10, 100);
    s->repeat = (uint8_t)(s->repeat >= 4 ? 4 : s->repeat >= 2 ? 2 : 1);
    s->bpm = (uint8_t)clampi(s->bpm ? s->bpm : 96, 60, 200);
    s->look = (uint8_t)(s->look ? 1 : 0);
    s->lift = (uint8_t)clampi(s->lift ? s->lift : 85, 30, 100);
    if (s->song > 999)
        s->song = 0;
}

/* ---- the song: the chords, the patterns, the riff --------------------------------------------------------- */
static void chord_name(sn_chord *c, int tr, int third, int fifth, int seventh)
{
    const char *q;
    if (third == 3 && fifth == 6)
        q = seventh ? "M7B5" : "DIM";
    else if (third == 3)
        q = seventh ? "MIN7" : "MIN";
    else if (fifth == 8)
        q = "AUG";
    else if (seventh == 10)
        q = "7";
    else if (seventh == 11)
        q = "MAJ7";
    else
        q = "";
    char b[10];
    cpy(b, NOTE_NAMES[(c->root + tr) % 12], sizeof b);
    int k = 0;
    while (b[k]) k++;
    for (int i = 0; q[i] && k < 7; ++i)
        b[k++] = q[i];
    b[k] = 0;
    cpy(c->name, b, sizeof c->name);
}

/* the chord on scale degree deg: its tones and a voicing near center (MIDI), the voice leading of a player's hand:
 * of the chord's inversions, the one whose middle is closest. Its name with the key up (tr) in it; its notes without
 * (the key up is added as each note sounds) */
static void build_chord(sn_chord *c, const sn_settings *s, int tr, int deg, int flavour, bool dom, int *center)
{
    int sc = clampi(s->scale, 0, SN_SCALES - 1);
    int r = SCALE[sc][deg % 7];
    int iv[4];
    for (int k = 0; k < 4; ++k) {                  /* root, third, fifth, seventh: stacked thirds */
        int d = deg + 2 * k;
        iv[k] = SCALE[sc][d % 7] + 12 * (d / 7) - r;
    }
    if (dom && deg % 7 == 4) {                     /* the dominant in minor: a major third, a minor seventh */
        iv[1] = 4;
        iv[3] = 10;
    }
    int third = iv[1], fifth = iv[2], seventh = flavour >= 7 ? iv[3] : 0;
    c->root = (uint8_t)((s->key + r) % 12);
    c->tone[0] = c->root;
    c->tone[1] = (uint8_t)((c->root + third) % 12);
    c->tone[2] = (uint8_t)((c->root + fifth) % 12);
    c->tone[3] = (uint8_t)((c->root + (seventh ? seventh : 12)) % 12);
    chord_name(c, tr, third, fifth, seventh);
    /* the voicing: four notes, close (a triad doubles its root on top) */
    int pcs[4] = {c->root, c->tone[1], c->tone[2], seventh ? c->tone[3] : (c->root + 12) % 12};
    int best_d = 1 << 20;
    int bestv[4] = {60, 64, 67, 72};
    for (int inv = 0; inv < 4; ++inv) {            /* each inversion: its lowest note, the others above in order */
        for (int base = 50; base <= 68; ++base) {
            if ((base % 12) != pcs[inv])
                continue;
            int v[4], p = base;
            v[0] = p;
            for (int j = 1; j < 4; ++j) {
                int pc = pcs[(inv + j) % 4];
                int x = p + 1;
                while (x % 12 != pc)
                    ++x;
                v[j] = x;
                p = x;
            }
            int mid = (v[0] + v[3]) / 2;
            int d = (mid - *center) * (mid - *center) + (v[3] > 79 ? 400 : 0);
            if (d < best_d) {
                best_d = d;
                memcpy(bestv, v, sizeof bestv);
            }
        }
    }
    for (int j = 0; j < 4; ++j)
        c->note[j] = (uint8_t)bestv[j];
    c->note[4] = 0;
    c->n = 4;
    *center = (bestv[0] + bestv[3]) / 2;
}

/* the riff's one-bar rhythms, by SPEED: per step the length in sixteenths of the note starting there (0 none) */
static const uint8_t HOOK_RHY[SN_SPEED_COUNT][4][16] = {
    [SN_SPEED_SLOW] = {{6, 0, 0, 0, 0, 0, 2, 0, 8, 0, 0, 0, 0, 0, 0, 0},
                       {4, 0, 0, 0, 4, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0},
                       {3, 0, 0, 3, 0, 0, 2, 0, 8, 0, 0, 0, 0, 0, 0, 0},
                       {8, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0}},
    [SN_SPEED_MID] = {{2, 0, 2, 0, 2, 0, 2, 0, 4, 0, 0, 0, 2, 0, 2, 0},
                      {3, 0, 0, 3, 0, 0, 2, 0, 4, 0, 0, 0, 4, 0, 0, 0},
                      {2, 0, 1, 0, 2, 0, 1, 0, 2, 0, 2, 0, 4, 0, 0, 0},
                      {1, 0, 2, 0, 1, 0, 2, 0, 1, 0, 2, 0, 3, 0, 0, 1}},
    [SN_SPEED_FAST] = {{2, 0, 0, 2, 0, 0, 2, 0, 1, 0, 2, 0, 1, 0, 2, 0},   /* the 3-3-2 of a horn line */
                       {1, 0, 1, 2, 0, 0, 1, 1, 2, 0, 0, 2, 0, 0, 2, 0},
                       {3, 0, 0, 3, 0, 0, 2, 0, 3, 0, 0, 3, 0, 0, 2, 0},
                       {1, 1, 2, 0, 1, 0, 2, 0, 1, 1, 2, 0, 1, 0, 2, 0}},
};

/* a bar of the riff into dst (16 steps) from rhythm rhy, its pitches a walk from *note within lo..hi */
static void hook_bar(int8_t *dst, uint8_t *len, const uint8_t *rhy, uint32_t *r, int *note, int lo, int hi)
{
    for (int i = 0; i < 16; ++i) {
        dst[i] = -128;
        len[i] = 0;
        if (!rhy[i])
            continue;
        dst[i] = (int8_t)*note;
        len[i] = rhy[i];
        int roll = rnd_n(r, 10), mv;                /* repeated notes, steps, now and then a leap */
        if (roll < 3) mv = 0;
        else if (roll < 7) mv = rnd_n(r, 2) ? 1 : -1;
        else if (roll < 9) mv = rnd_n(r, 2) ? 2 : -2;
        else mv = rnd_n(r, 2) ? 4 : -3;
        int nn = *note + mv;
        if (nn > hi || nn < lo)
            nn = *note - mv;
        *note = clampi(nn, lo, hi);
    }
}

/* the riff: a one-bar motif (A), repeated over REPEAT bars as a phrase: A A' (2), A A' A A'' (4), A' and A'' its
 * answers (the first half again, a new second half landing on the tonic or the fifth). Scale steps from the tonic */
static void make_hook(sn_groove *g, uint32_t seed)
{
    const sn_settings *s = &g->s;
    uint32_t r = seed * 2654435761u + 0x51ED27u;
    int sp = clampi(s->speed, 0, SN_SPEED_COUNT - 1);
    int lo = s->range >= 2 ? -3 : 0, hi = s->range >= 2 ? 10 : 7;
    for (int i = 0; i < SN_HOOK_LEN; ++i) {
        g->hook[i] = -128;
        g->hook_len[i] = 0;
    }
    int note = rnd_n(&r, 3) * 2;                   /* starts on the tonic, the third or the fifth */
    const uint8_t *rhy = HOOK_RHY[sp][rnd_n(&r, 4)];
    int8_t a[16], b[16];
    uint8_t al[16], bl[16];
    hook_bar(a, al, rhy, &r, &note, lo, hi);
    /* the answer: A's first half, a new second half; its last note on the tonic or the fifth, held */
    memcpy(b, a, sizeof b);
    memcpy(bl, al, sizeof bl);
    note = a[0] != -128 ? a[0] : 0;
    {
        int8_t t[16];
        uint8_t tl[16];
        hook_bar(t, tl, rhy, &r, &note, lo, hi);
        for (int i = 8; i < 16; ++i) {
            b[i] = t[i];
            bl[i] = tl[i];
        }
    }
    for (int i = 15; i >= 8; --i)
        if (b[i] != -128) {
            b[i] = (int8_t)(rnd_n(&r, 2) ? 0 : 4);
            break;
        }
    int rep = sn_repeat_bars(s);
    for (int bar = 0; bar < rep; ++bar) {
        bool answer = bar % 2 == 1;
        for (int i = 0; i < 16; ++i) {
            g->hook[bar * 16 + i] = answer ? b[i] : a[i];
            g->hook_len[bar * 16 + i] = answer ? bl[i] : al[i];
        }
    }
    if (rep == 4) {                                /* the phrase's last bar: lands home, long */
        for (int i = 63; i >= 56; --i)
            if (g->hook[i] != -128) {
                g->hook[i] = 0;
                g->hook_len[i] = (uint8_t)(64 - i);
                break;
            }
    }
}

void sn_rebuild(sn_groove *g)
{
    sn_settings *s = &g->s;
    const sn_loopdef *L = &SN_LOOP[clampi(s->chords, 0, SN_LOOPS - 1)];
    int center = 62;
    g->nloop = L->n;
    for (int i = 0; i < L->n; ++i)
        build_chord(&g->loop[i], s, g->tr, L->deg[i], L->flavour, L->dom != 0, &center);
    uint32_t seed = (uint32_t)s->style * 7919u + (uint32_t)s->song * 104729u + 17u;
    uint32_t r = seed;
    g->pad_pat = (uint8_t)rnd_n(&r, 4);
    g->bass_pat = (uint8_t)rnd_n(&r, 2);
    g->drum_var = (uint8_t)rnd_n(&r, 4);
    make_hook(g, seed + (uint32_t)g->hook_var * 31u);
}

/* ---- the parts' rhythms ------------------------------------------------------------------------------------ */
/* the pads in the full band: 16 steps, the length in sixteenths of the chord starting there (0 none) */
static const uint8_t PAD_PATS[PAD_COUNT][4][16] = {
    [PAD_HELD] = {{16}, {16}, {16}, {16}},
    [PAD_PULSE] = {{2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0},   /* the night's eighths */
                   {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0},
                   {2, 0, 2, 0, 2, 0, 2, 1, 0, 2, 0, 2, 0, 2, 2, 0},
                   {3, 0, 0, 3, 0, 0, 2, 0, 3, 0, 0, 3, 0, 0, 2, 0}},
    [PAD_STAB] = {{2, 0, 0, 2, 0, 0, 2, 0, 0, 0, 2, 0, 2, 0, 0, 0},   /* the horn section's hits */
                  {0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 2, 0, 0, 0, 0, 0},
                  {1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 2, 0, 0, 0},
                  {2, 0, 0, 0, 0, 0, 0, 2, 0, 0, 2, 0, 0, 2, 0, 0}},
    [PAD_COMP] = {{6, 0, 0, 0, 0, 0, 2, 0, 6, 0, 0, 0, 0, 0, 2, 0},   /* the ballad's piano hand */
                  {4, 0, 0, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 0, 0},
                  {8, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0},
                  {3, 0, 0, 3, 0, 0, 2, 0, 8, 0, 0, 0, 0, 0, 0, 0}},
};
/* the bass: per step a tone (0 none, 1 root, 2 octave, 3 fifth, 4 seventh, 5 approach to the next chord's root,
 * 6 the scale step above the root) and its length */
enum { BR_NONE, BR_ROOT, BR_OCT, BR_FIFTH, BR_SEV, BR_APP, BR_PASS };
typedef struct { uint8_t tone[16], len[16]; } bass_pat_t;
static const bass_pat_t BASS_PATS[SN_BL_COUNT][2] = {
    [SN_BL_PULSE] = {{{1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}, {2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0}},
                     {{1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 3, 0, 5, 0}, {2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0}}},
    [SN_BL_LONG] = {{{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
                    {{1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0}, {8, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0}}},
    [SN_BL_SYNCO] = {{{1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 3, 0, 2, 0}, {2, 0, 0, 2, 0, 0, 2, 0, 0, 0, 2, 0, 2, 0, 2, 0}},
                     {{1, 0, 0, 2, 0, 0, 1, 0, 0, 1, 0, 0, 3, 0, 5, 0}, {2, 0, 0, 1, 0, 0, 2, 0, 0, 2, 0, 0, 2, 0, 2, 0}}},
    [SN_BL_OCTAVE] = {{{1, 0, 2, 0, 1, 0, 2, 0, 1, 0, 2, 0, 1, 0, 2, 0}, {2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0}},
                      {{1, 0, 1, 0, 2, 0, 1, 0, 1, 0, 1, 0, 2, 0, 5, 0}, {2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0}}},
    [SN_BL_SEQ] = {{{1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0}, {1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0}},
                   {{1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 3, 0}, {1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0}}},
    [SN_BL_SLIDE] = {{{1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 5, 0}, {8, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 2, 0}},
                     {{1, 0, 0, 0, 0, 0, 6, 0, 3, 0, 0, 0, 0, 0, 0, 0}, {6, 0, 0, 0, 0, 0, 2, 0, 8, 0, 0, 0, 0, 0, 0, 0}}},
};
/* the full kit's grooves: per drum the velocity at each step (0 none): KICK SNARE CLAP HAT OPEN, and a row of toms
 * (TOMTOM's: 1 high, 2 mid, 3 low, its velocity in TOM_VEL) */
typedef struct { uint8_t v[5][16]; uint8_t tom[16]; } drum_pat_t;
static const drum_pat_t DRUM_PATS[DRM_COUNT] = {
    [DRM_ROCK] = {{{120, 0, 0, 0, 0, 0, 0, 0, 116, 0, 96, 0, 0, 0, 0, 0},
                   {0, 0, 0, 0, 124, 0, 0, 0, 0, 0, 0, 0, 124, 0, 0, 0},
                   {0},
                   {80, 0, 60, 0, 80, 0, 60, 0, 80, 0, 60, 0, 80, 0, 60, 0},
                   {0}}, {0}},
    [DRM_STOMP] = {{{122, 0, 0, 0, 112, 0, 0, 0, 122, 0, 0, 0, 112, 0, 0, 0},
                    {0, 0, 0, 0, 126, 0, 0, 0, 0, 0, 0, 0, 126, 0, 0, 0},
                    {0},
                    {84, 0, 62, 0, 84, 0, 62, 0, 84, 0, 62, 0, 84, 0, 62, 0},
                    {0}}, {0}},
    [DRM_FUNK] = {{{120, 0, 0, 92, 0, 0, 104, 0, 0, 0, 112, 0, 0, 0, 0, 0},
                   {0, 0, 0, 0, 124, 0, 0, 34, 0, 0, 0, 0, 124, 0, 0, 30},
                   {0},
                   {78, 40, 60, 40, 78, 40, 60, 40, 78, 40, 60, 40, 78, 40, 60, 40},
                   {0}}, {0}},
    [DRM_MACHINE] = {{{124, 0, 0, 0, 0, 0, 98, 0, 0, 0, 118, 0, 0, 0, 0, 0},
                      {0},
                      {0, 0, 0, 0, 124, 0, 0, 0, 0, 0, 0, 0, 124, 0, 0, 0},
                      {70, 0, 70, 0, 70, 0, 70, 0, 70, 0, 70, 0, 70, 0, 70, 0},
                      {0}}, {0}},
    [DRM_HALF] = {{{116, 0, 0, 0, 0, 0, 0, 0, 0, 0, 90, 0, 0, 0, 0, 0},
                   {0, 0, 0, 0, 0, 0, 0, 0, 124, 0, 0, 0, 0, 0, 0, 0},
                   {0},
                   {66, 0, 46, 0, 66, 0, 46, 0, 66, 0, 46, 0, 66, 0, 46, 0},
                   {0}}, {0}},
    [DRM_TOMS] = {{{118, 0, 0, 0, 0, 0, 0, 0, 110, 0, 0, 0, 0, 0, 0, 0},
                   {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 124, 0, 0, 0},
                   {0},
                   {60, 0, 0, 0, 60, 0, 0, 0, 60, 0, 0, 0, 60, 0, 0, 0},
                   {0}}, {3, 0, 3, 2, 0, 3, 0, 1, 3, 0, 3, 0, 0, 2, 1, 0}},
    [DRM_BOX] = {{{104, 0, 0, 0, 0, 0, 0, 0, 98, 0, 0, 0, 0, 0, 0, 0},
                  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 124, 0, 0, 0},
                  {0},
                  {0},
                  {0}}, {0}},
};
/* the drum machine (the hush, the rise, the end; RHYTHMBOX's full band too): MKICK MRIM MHAT */
typedef struct { uint8_t v[3][16]; } mach_pat_t;
static const mach_pat_t MACH_PATS[MP_COUNT] = {
    [MP_CR] = {{{100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 92, 0, 0, 0, 0, 0},
                {0, 0, 0, 74, 0, 0, 82, 0, 0, 0, 0, 70, 0, 0, 80, 0},
                {64, 0, 44, 0, 64, 0, 44, 0, 64, 0, 44, 0, 64, 0, 44, 0}}},
    [MP_PULSE] = {{{96, 0, 0, 0, 88, 0, 0, 0, 96, 0, 0, 0, 88, 0, 0, 0},
                   {0},
                   {0, 0, 58, 0, 0, 0, 58, 0, 0, 0, 58, 0, 0, 0, 58, 0}}},
    [MP_TICK] = {{{92, 0, 0, 0, 0, 0, 0, 0, 86, 0, 0, 0, 0, 0, 0, 0},
                  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 72, 0, 0, 0},
                  {54, 30, 42, 30, 54, 30, 42, 30, 54, 30, 42, 30, 54, 30, 42, 30}}},
};

/* ---- the engine ------------------------------------------------------------------------------------------- */
void sn_init(sn_groove *g, float sample_rate)
{
    memset(g, 0, sizeof *g);
    g->sr = sample_rate;
    sn_defaults(&g->s);
    g->step = -1;
    g->next_style = g->next_song = -1;
    g->fill_from = 8;
    for (int k = 0; k < 27; ++k)
        g->key_note[k] = -1;
    for (int p = 0; p < SN_PARTS; ++p)
        g->cut[p] = 127;
    sn_rebuild(g);
}

void sn_set_sound(sn_groove *g, void (*fn)(void *ctx, const sn_event *e), void *ctx)
{
    g->sound = fn;
    g->sound_ctx = ctx;
}

static void emit(sn_groove *g, int on, int part, int note, int vel)
{
    if (!g->sound)
        return;
    sn_event e = {(uint8_t)on, (uint8_t)part, (uint8_t)clampi(note, 0, 127), (uint8_t)clampi(vel, 1, 127)};
    g->sound(g->sound_ctx, &e);
}

static void feed(sn_groove *g, int part, int note, int vel)
{
    uint8_t w = (uint8_t)((g->feed_w + 1u) % SN_FEED);
    if (w == g->feed_r)
        return;
    sn_seen *n = &g->feed[g->feed_w];
    n->part = (uint8_t)part;
    n->note = (uint8_t)note;
    n->vel = (uint8_t)vel;
    g->feed_w = w;
}

bool sn_feed_pop(sn_groove *g, sn_seen *n)
{
    if (g->feed_r == g->feed_w)
        return false;
    *n = g->feed[g->feed_r];
    g->feed_r = (uint8_t)((g->feed_r + 1u) % SN_FEED);
    return true;
}

static void msg(sn_groove *g, const char *m)
{
    cpy(g->msg, m, sizeof g->msg);
    g->msg_new = 1;
}

static void note_off_now(sn_groove *g, int i)
{
    emit(g, 0, g->held[i].part, g->held[i].note, 0);
    g->held[i] = g->held[--g->nheld];
}

/* a note now (the key up added to a pitched part's), its note-off after len frames; a note of the part still
 * sounding at that pitch is cut first */
static void note(sn_groove *g, int part, int n, int vel, float len_frames)
{
    if (part == SN_DRUMS) {                        /* a drum rings for its own length: then its voice is free again */
        static const float RING[SN_DR_COUNT] = {0.70f, 0.32f, 0.26f, 0.09f, 0.45f, 1.0f, 0.38f, 0.38f, 0.38f,
                                                0.20f, 0.06f, 0.05f};
        len_frames = RING[n % SN_DR_COUNT] * g->sr;
    } else {
        n += g->tr;
    }
    for (int i = 0; i < g->nheld; ++i)
        if (g->held[i].part == part && g->held[i].note == n) {
            note_off_now(g, i);
            break;
        }
    if (g->nheld >= SN_HELD) {                     /* full: the oldest note goes */
        int o = 0;
        for (int i = 1; i < g->nheld; ++i)
            if ((int32_t)(g->held[i].off - g->held[o].off) < 0)
                o = i;
        note_off_now(g, o);
    }
    emit(g, 1, part, n, vel);
    feed(g, part, n, vel);
    if (g->step >= 0) {
        if (part == SN_DRUMS)
            g->drum_hits[n % SN_DR_COUNT] |= (uint16_t)(1u << g->step);
        else
            g->hits[part] |= (uint16_t)(1u << g->step);
    }
    if (part == SN_KEYS)
        g->hook_on_clock = g->clock;
    g->held[g->nheld].note = (uint8_t)n;
    g->held[g->nheld].part = (uint8_t)part;
    g->held[g->nheld].off = g->clock + (uint32_t)(len_frames > 1.0f ? len_frames : 1.0f);
    g->nheld++;
}

void sn_all_off(sn_groove *g)
{
    while (g->nheld > 0)
        note_off_now(g, g->nheld - 1);
}

static void part_off(sn_groove *g, int part)
{
    for (int i = g->nheld - 1; i >= 0; --i)
        if (g->held[i].part == part)
            note_off_now(g, i);
}

/* the pending notes: a swung or late sixteenth's notes, played when the clock reaches them */
#define SN_PEND 48
static struct { uint32_t at; uint8_t part, note, vel, pad; float len; } pend[SN_PEND];
static int npend;
static void schedule(sn_groove *g, uint32_t delay, int part, int n, int vel, float len)
{
    if (delay == 0 || npend >= SN_PEND) {
        if (part == SN_DRUMS && (n & 0x80))       /* a choke: the open hat's note-off */
            emit(g, 0, SN_DRUMS, n & 0x7F, 0);
        else
            note(g, part, n, vel, len);
        return;
    }
    pend[npend].at = g->clock + delay;
    pend[npend].part = (uint8_t)part;
    pend[npend].note = (uint8_t)n;
    pend[npend].vel = (uint8_t)vel;
    pend[npend].len = len;
    npend++;
}

static float step_frames(const sn_groove *g) { return g->sr * 60.0f / (float)clampi(g->s.bpm, 60, 200) / 4.0f; }

/* ---- the form ---------------------------------------------------------------------------------------------- */
int sn_sec_len(const sn_groove *g, int sec)
{
    int build = sn_build_bars(&g->s);
    bool hold = g->s.build == SN_BUILD_HOLD;
    switch (sec) {
    case SN_SEC_HUSH: return hold ? 4 : build / 2;
    case SN_SEC_RISE: return hold ? 0 : build / 2;  /* (HOLD: open, until REC) */
    case SN_SEC_IN:
    case SN_SEC_IN2: return 1;
    case SN_SEC_DRAMA: return SN_DRAMA_BARS;
    case SN_SEC_END: return SN_END_BARS;
    default: return g->s.form ? (g->s.full >= 16 ? 16 : 8) : 0;   /* (FORM off: the full band on and on) */
    }
}
bool sn_in_full(const sn_groove *g) { return g->sec == SN_SEC_FULL || g->sec == SN_SEC_FULL2; }
bool sn_tense(const sn_groove *g) { return g->sec == SN_SEC_HUSH || g->sec == SN_SEC_RISE; }

/* bars since the tension began (the hush and the rise together), for the chords there */
static int tense_bar(const sn_groove *g)
{
    return g->sec == SN_SEC_RISE ? sn_sec_len(g, SN_SEC_HUSH) + g->sec_bar : g->sec_bar;
}

/* the loop's chord now: the full band starts its loop on the downbeat after the break-in; the break-ins and the
 * drama hold the loop's last chord (it leads home), the end the first */
static int chord_idx(const sn_groove *g, int bar_in)
{
    int bpc = sn_bars_per_chord(&g->s), n = g->nloop > 0 ? g->nloop : 1;
    switch (g->sec) {
    case SN_SEC_IN:
    case SN_SEC_IN2:
    case SN_SEC_DRAMA: return n - 1;
    case SN_SEC_END: return 0;
    case SN_SEC_HUSH:
    case SN_SEC_RISE: return (bar_in / bpc) % n;
    default: return (bar_in / bpc) % n;
    }
}
static int sec_chord_bar(const sn_groove *g) { return sn_tense(g) ? tense_bar(g) : g->sec_bar; }
static const sn_chord *chord_now(const sn_groove *g) { return &g->loop[chord_idx(g, sec_chord_bar(g))]; }
static const sn_chord *chord_next(const sn_groove *g)
{
    int bpc = sn_bars_per_chord(&g->s), n = g->nloop > 0 ? g->nloop : 1;
    if (!sn_in_full(g) && !sn_tense(g))
        return &g->loop[0];
    return &g->loop[((sec_chord_bar(g) / bpc) + 1) % n];
}
int sn_chord_now(const sn_groove *g) { return chord_idx(g, sec_chord_bar(g)); }

/* the key up sounding: KEY UP for each drama done; a change cuts the held chords and riff (their old key) */
static void key_to(sn_groove *g, int tr)
{
    tr = clampi(tr, 0, SN_TR_MAX);
    if (tr != g->tr) {
        part_off(g, SN_PADS);
        part_off(g, SN_KEYS);
        part_off(g, SN_BASS);
        g->tr = tr;
        sn_rebuild(g);                             /* (the chords' names: the key up in them) */
    }
}

/* into a section, from its first bar */
static void enter(sn_groove *g, int sec)
{
    g->sec = sec;
    g->sec_bar = 0;
    if (sec == SN_SEC_IN)
        g->fill_from = 8;
    if (sec == SN_SEC_IN2)
        g->fill_from = 0;
    if ((sec == SN_SEC_FULL || sec == SN_SEC_FULL2) && g->s.crash != SN_CRASH_OFF)
        g->crash_next = true;
    if (sec == SN_SEC_FULL2) {                     /* a drama done: the key up */
        g->dramas_done++;
        key_to(g, (int)g->s.keyup * g->dramas_done);
    }
    if (sec == SN_SEC_HUSH) {                      /* round again: home, a new riff */
        g->round++;
        g->hook_var++;
        g->dramas_done = 0;
        key_to(g, 0);
        sn_rebuild(g);
    }
}

/* after the full band: a drama while the form wants one, else the end */
static int after_full(const sn_groove *g)
{
    if (g->s.drama == SN_DRAMA_AUTO && g->dramas_done < g->s.dramas)
        return SN_SEC_DRAMA;
    return SN_SEC_END;
}

/* a new bar: what waited for the bar line comes in, the form moves on */
static void bar_line(sn_groove *g)
{
    for (int p = 0; p < SN_PARTS; ++p) {
        g->hits_last[p] = g->hits[p];
        g->hits[p] = 0;
    }
    for (int d = 0; d < SN_DR_COUNT; ++d) {
        g->drum_last[d] = g->drum_hits[d];
        g->drum_hits[d] = 0;
    }
    if (g->next_style >= 0) {
        int keep_song = g->next_song;
        sn_settings old = g->s;
        sn_style_apply(&g->s, g->next_style);
        for (int i = 0; i < 4; ++i)                /* MIX FORM MENU stay the player's */
            g->s.lvl[i] = old.lvl[i];
        g->s.form = old.form;
        g->s.full = old.full;
        g->s.dramas = old.dramas;
        g->s.keyup = old.keyup;
        g->s.palette = old.palette;
        g->s.look = old.look;
        if (keep_song >= 0) {
            g->s.song = (uint16_t)keep_song;
            sn_song_sounds(&g->s);
        }
        g->next_style = -1;
        g->next_song = -1;
        g->sound_sig++;
        sn_all_off(g);
        sn_rebuild(g);
    } else if (g->next_song >= 0) {
        g->s.song = (uint16_t)g->next_song;
        uint32_t r = (uint32_t)g->s.song * 2246822519u + (uint32_t)g->s.style;
        g->s.chords = SN_STYLE[g->s.style].loops[rnd_n(&r, 4)];
        sn_song_sounds(&g->s);                     /* each song its own sounds from the style's pool */
        g->sound_sig++;
        g->next_song = -1;
        sn_rebuild(g);
    }
    if (g->fill_bar && g->fill_bar_at == g->bar - 1 && g->s.crash != SN_CRASH_OFF)
        g->crash_next = true;                      /* (REC's fill over: a crash on this downbeat) */
    g->fill_bar = g->fill_bar && g->fill_bar_at == g->bar;
    if (g->in_next) {                              /* REC: the break-in now */
        g->in_next = false;
        enter(g, SN_SEC_IN);
        return;
    }
    if (g->drama_next) {                           /* SEQ: the drama now */
        g->drama_next = false;
        enter(g, SN_SEC_DRAMA);
        msg(g, "DRAMA");
        return;
    }
    if (g->out_next) {                             /* REC in a drama: the way back in now */
        g->out_next = false;
        enter(g, SN_SEC_IN2);
        return;
    }
    int len = sn_sec_len(g, g->sec);
    if (len <= 0 || ++g->sec_bar < len) {
        if (len <= 0)
            g->sec_bar++;
        if (g->s.crash == SN_CRASH_8 && sn_in_full(g) && g->sec_bar % 8 == 0 && g->sec_bar > 0)
            g->crash_next = true;                  /* (CRASH 8 BARS: every eighth bar of the full band too) */
        return;
    }
    switch (g->sec) {
    case SN_SEC_HUSH: enter(g, SN_SEC_RISE); break;
    case SN_SEC_RISE:
        if (g->rec_held)                           /* REC held: the tension holds */
            g->sec_bar = len - 1;
        else
            enter(g, SN_SEC_IN);
        break;
    case SN_SEC_IN: enter(g, SN_SEC_FULL); break;
    case SN_SEC_DRAMA: enter(g, SN_SEC_IN2); break;
    case SN_SEC_IN2: enter(g, SN_SEC_FULL2); break;
    case SN_SEC_FULL:
    case SN_SEC_FULL2: enter(g, after_full(g)); break;
    default: enter(g, SN_SEC_HUSH); break;         /* (END: round again) */
    }
}

/* is part p out (the form, its level 0) */
static bool part_out(const sn_groove *g, int p)
{
    if (g->s.lvl[p] == 0)
        return true;
    int half_rise = sn_sec_len(g, SN_SEC_RISE) > 0 ? sn_sec_len(g, SN_SEC_RISE) / 2 : 2;
    switch (g->sec) {
    case SN_SEC_HUSH:                              /* the drone; the machine from the second bar */
        return p == SN_BASS || p == SN_KEYS || (p == SN_DRUMS && g->sec_bar < 1);
    case SN_SEC_RISE:                              /* the bass in halfway */
        return p == SN_KEYS || (p == SN_BASS && g->sec_bar < half_rise);
    case SN_SEC_IN: return p == SN_KEYS;
    case SN_SEC_DRAMA:                             /* one chord held; the toms in for the last two bars */
        return p == SN_BASS || p == SN_KEYS || (p == SN_DRUMS && g->sec_bar < SN_DRAMA_BARS - 2);
    case SN_SEC_IN2: return p == SN_BASS || p == SN_KEYS;
    case SN_SEC_END:                               /* down to the drone */
        return p == SN_KEYS || (p == SN_BASS && g->sec_bar >= 2) || (p == SN_DRUMS && g->sec_bar >= 2);
    default: return false;
    }
}

/* the tension, 0..1: through the hush and the rise to the break-in; the drama's own climb */
static float tension_now(const sn_groove *g)
{
    float pos = (float)(g->step < 0 ? 0 : g->step) / 16.0f;
    switch (g->sec) {
    case SN_SEC_HUSH: {
        int total = sn_sec_len(g, SN_SEC_HUSH) + (sn_sec_len(g, SN_SEC_RISE) > 0 ? sn_sec_len(g, SN_SEC_RISE) : 8);
        return 0.05f + 0.9f * ((float)g->sec_bar + pos) / (float)total;
    }
    case SN_SEC_RISE: {
        int h = sn_sec_len(g, SN_SEC_HUSH), r = sn_sec_len(g, SN_SEC_RISE);
        if (r <= 0) {                              /* HOLD: it creeps up, never quite there */
            float t = ((float)g->sec_bar + pos) / 8.0f;
            return clampf(0.4f + 0.5f * t / (1.0f + t), 0.0f, 0.95f);
        }
        return 0.05f + 0.9f * ((float)(h + g->sec_bar) + pos) / (float)(h + r);
    }
    case SN_SEC_IN:
    case SN_SEC_IN2: return 1.0f;
    case SN_SEC_DRAMA: return 0.3f + 0.65f * ((float)g->sec_bar + pos) / (float)SN_DRAMA_BARS;
    case SN_SEC_END: return clampf(0.6f - 0.6f * ((float)g->sec_bar + pos) / (float)SN_END_BARS, 0.0f, 1.0f);
    default: return 1.0f;
    }
}

/* a tom fill: from sixteenth `from` to the bar's end, high toms to low, louder as it goes, the kick under the last
 * beat; whole (from 0): a bar of it, each beat a tom lower, the last beat all of them at once */
static void tom_fill(int i, int from, int vel[SN_DR_COUNT])
{
    if (i < from)
        return;
    int n = 16 - from, k = i - from;
    int t = k * 3 / (n > 0 ? n : 1);                /* 0 high, 1 mid, 2 low */
    int v = 86 + k * 41 / (n > 0 ? n : 1);
    if (from == 0 && i >= 12) {                    /* the last beat of a whole bar: the three, a flam down */
        vel[SN_DR_TOMH] = i == 12 ? v : 0;
        vel[SN_DR_TOMM] = i == 13 ? v : 0;
        vel[SN_DR_TOML] = v;
        vel[SN_DR_SNARE] = i == 15 ? v : 0;
        return;
    }
    if (from == 0 && (i % 2)) {                    /* (a whole bar: eighths, then sixteenths in its second half) */
        if (i < 8)
            return;
    }
    vel[t == 0 ? SN_DR_TOMH : t == 1 ? SN_DR_TOMM : SN_DR_TOML] = v;
    if (i == 15 && from > 0)
        vel[SN_DR_SNARE] = v;
}

/* the drums of step i: the drum machine in the hush, the rise and the end, the fills, the full kit's groove */
static void play_drums(sn_groove *g, int i, uint32_t delay, float hum)
{
    const sn_style *st = &SN_STYLE[g->s.style];
    const drum_pat_t *dp = &DRUM_PATS[st->drums];
    const mach_pat_t *mp = &MACH_PATS[st->machine];
    float e = (float)g->s.energy / 100.0f;
    int vel[SN_DR_COUNT] = {0};
    bool machine = sn_tense(g) || g->sec == SN_SEC_END || (g->sec == SN_SEC_IN && i < g->fill_from) ||
                   (st->drums == DRM_BOX && sn_in_full(g));
    if (machine) {
        float t = tension_now(g);
        vel[SN_DR_MKICK] = mp->v[0][i];
        vel[SN_DR_MRIM] = mp->v[1][i];
        vel[SN_DR_MHAT] = mp->v[2][i];
        if (g->sec == SN_SEC_RISE && e > 0.45f && (i % 2) && t > 0.5f)   /* the rise: the machine's hat busier */
            vel[SN_DR_MHAT] = 30 + (int)(t * 20.0f);
        if (g->sec == SN_SEC_RISE && t > 0.75f && i == 14 && (g->drum_var & 1))   /* a pickup into each bar */
            vel[SN_DR_MKICK] = 80;
        for (int d = SN_DR_MKICK; d <= SN_DR_MHAT; ++d)                  /* softer in the hush, louder as it rises */
            if (vel[d])
                vel[d] = (int)((float)vel[d] * (0.7f + 0.3f * t));
        if (g->sec == SN_SEC_HUSH && g->sec_bar < 2)                     /* the hush's first bars: the hat alone */
            vel[SN_DR_MKICK] = vel[SN_DR_MRIM] = 0;
    }
    if (g->sec == SN_SEC_IN || g->sec == SN_SEC_IN2) {
        tom_fill(i, g->fill_from, vel);
    } else if (g->sec == SN_SEC_DRAMA) {           /* the last two bars: the low tom rolling in, eighths then sixteenths */
        int b = g->sec_bar - (SN_DRAMA_BARS - 2);
        bool on = b == 0 ? i % 2 == 0 : true;
        if (on && b >= 0)
            vel[SN_DR_TOML] = 60 + (b * 16 + i) * 50 / 32;
    } else if (sn_in_full(g)) {
        {
            vel[SN_DR_KICK] = dp->v[0][i];
            int back = dp->v[1][i] > dp->v[2][i] ? dp->v[1][i] : dp->v[2][i];
            if (back >= 90) {                      /* the backbeat, as CLAP has it */
                vel[SN_DR_SNARE] = g->s.clap == SN_CLAP_CLAP ? 0 : back;
                vel[SN_DR_CLAP] = g->s.clap == SN_CLAP_SNARE ? 0 : back;
            } else if (back) {                     /* a ghost: the snare's */
                vel[SN_DR_SNARE] = back;
            }
            int h = dp->v[3][i], o = dp->v[4][i];
            if (h || st->drums != DRM_BOX) {
                switch (g->s.hats) {
                case SN_HATS_8THS: h = i % 2 == 0 ? (i % 4 == 0 ? 80 : 62) : 0; o = 0; break;
                case SN_HATS_16THS: h = i % 4 == 0 ? 80 : i % 2 == 0 ? 62 : 40; o = 0; break;
                default: h = i % 4 == 2 ? 0 : (i % 2 ? 0 : 66); o = i % 4 == 2 ? 92 : 0; break;
                }
                if (st->drums == DRM_TOMS || st->drums == DRM_HALF)
                    h = h * 75 / 100;
            }
            if (e < 0.35f && (i % 2)) h = 0;      /* low ENERGY: no sixteenth hats */
            if (e < 0.25f && i % 4 == 2) h = 0;
            vel[SN_DR_HAT] = h;
            vel[SN_DR_OPEN] = o;
            if (dp->tom[i]) {                      /* TOMTOM's toms */
                static const uint8_t T[4] = {0, SN_DR_TOMH, SN_DR_TOMM, SN_DR_TOML};
                vel[T[dp->tom[i]]] = 96 + (i % 4 == 0 ? 16 : 0);
            }
            if (e > 0.8f && i == 15 && (g->drum_var & 2) && st->drums != DRM_HALF)
                vel[SN_DR_KICK] = vel[SN_DR_KICK] ? vel[SN_DR_KICK] : 82;   /* high: a pickup kick */
        }
        /* a fill: every FILLS bars over the last beat, REC's over the last two, the full band's last bar before a
         * drama or the end over its last two too */
        static const uint8_t EVERY[SN_FILLS_COUNT] = {0, 8, 4};
        int ev = EVERY[g->s.fills], len = sn_sec_len(g, g->sec);
        bool last = len > 0 && g->sec_bar == len - 1;
        int from = (g->fill_bar && g->fill_bar_at == g->bar) || last ? 8 : ev && g->sec_bar % ev == ev - 1 ? 12 : 16;
        if (i >= from) {
            vel[SN_DR_HAT] = vel[SN_DR_OPEN] = 0;
            if (i > from)
                vel[SN_DR_SNARE] = vel[SN_DR_CLAP] = 0;
            int keep_kick = vel[SN_DR_KICK];
            tom_fill(i, from, vel);
            vel[SN_DR_KICK] = i == 15 ? 0 : keep_kick;
        }
    }
    if (i == 0 && g->crash_next) {
        vel[SN_DR_CRASH] = 112;
        vel[SN_DR_KICK] = vel[SN_DR_KICK] ? vel[SN_DR_KICK] : 120;
        g->crash_next = false;
    }
    for (int d = 0; d < SN_DR_COUNT; ++d) {
        if (!vel[d])
            continue;
        int v = vel[d];
        if (d != SN_DR_KICK && d != SN_DR_CRASH && d < SN_DR_MKICK)
            v = (int)((float)v * (0.78f + 0.22f * e));
        v += (int)(hum * 10.0f * (float)g->s.human / 100.0f);
        if (d == SN_DR_HAT && g->s.hats != SN_HATS_OPEN)   /* a closed hat chokes the open one */
            schedule(g, delay, SN_DRUMS, 0x80 | SN_DR_OPEN, 1, 0);
        schedule(g, delay, SN_DRUMS, d, clampi(v, 1, 127), 0);
    }
}

static int bass_note(const sn_groove *g, int tone, const sn_chord *c, const sn_chord *nx)
{
    int root = 36 + ((c->root - 36) % 12 + 12) % 12;   /* C2 .. B2 */
    if (root > 43)
        root -= 12;                                     /* (G#1 .. G2: the bass sits low) */
    switch (tone) {
    case BR_OCT: return root + 12;
    case BR_FIFTH: return root + ((c->tone[2] - c->root + 12) % 12);
    case BR_SEV: return root + ((c->tone[3] - c->root + 12) % 12);
    case BR_APP: {
        int nr = 36 + ((nx->root - 36) % 12 + 12) % 12;
        if (nr > 43) nr -= 12;
        return nx == c ? root + 12 : nr - 1;            /* a semitone below the next root */
    }
    case BR_PASS: return sn_snap(&g->s, root + 2);
    default: return root;
    }
}

/* the chord's triad (root, third, fifth) as three close notes, the lowest the first of them at or above the
 * voicing's lowest note: an inversion near where the voice leading put the chord */
static void triad(const sn_chord *c, int out[3])
{
    int lo = c->note[0], best = -1;
    for (int k = 0; k < 3; ++k) {
        int n = lo;
        while ((n % 12) != c->tone[k])
            ++n;
        if (best < 0 || n < out[0]) {
            best = k;
            out[0] = n;
        }
    }
    for (int j = 1; j < 3; ++j) {
        int n = out[j - 1] + 1;
        while ((n % 12) != c->tone[(best + j) % 3])
            ++n;
        out[j] = n;
    }
}

/* the chord held for len sixteenths: the drone, the break-ins' and the drama's chord */
static void pads_held(sn_groove *g, uint32_t delay, float sf, int vel, int len)
{
    const sn_chord *c = chord_now(g);
    for (int k = 0; k < c->n; ++k)
        schedule(g, delay + (uint32_t)((float)k * sf * 0.05f), SN_PADS, c->note[k], vel, sf * (float)len - 40.0f);
}

static void play_pads(sn_groove *g, int i, uint32_t delay, float sf, float hum)
{
    const sn_style *st = &SN_STYLE[g->s.style];
    int bpc = sn_bars_per_chord(&g->s);
    bool chord_top = i == 0 && (sec_chord_bar(g) % bpc) == 0;
    if (sn_tense(g) || g->sec == SN_SEC_END) {     /* the drone: each chord held as long as it lasts, soft, swelling */
        if (g->sec == SN_SEC_END ? i == 0 && g->sec_bar % 2 == 0 : chord_top)
            pads_held(g, delay, sf, clampi(70 + (int)(tension_now(g) * 24.0f) + (int)(hum * 6.0f), 1, 127),
                      g->sec == SN_SEC_END ? 32 : 16 * bpc);
        return;
    }
    if (g->sec == SN_SEC_IN || g->sec == SN_SEC_IN2) {   /* the break-in: the leading chord, a bar */
        if (i == 0 || (i == g->fill_from && g->fill_from > 0 && g->sec == SN_SEC_IN && !g->hits[SN_PADS]))
            pads_held(g, delay, sf, 104, 16 - i);
        return;
    }
    if (g->sec == SN_SEC_DRAMA) {                  /* one chord, held, louder each two bars */
        if (i == 0 && g->sec_bar % 2 == 0)
            pads_held(g, delay, sf, 96 + g->sec_bar * 8, 32);
        return;
    }
    const sn_chord *c = chord_now(g);
    if (st->pad == PAD_HELD) {                     /* the full band's held chord: the triad (the voices stay for the drums) */
        if (chord_top) {
            int t[3];
            triad(c, t);
            for (int k = 0; k < 3; ++k)
                schedule(g, delay + (uint32_t)((float)k * sf * 0.04f), SN_PADS, t[k], clampi(96 + (int)(hum * 8.0f), 1, 127),
                         sf * (float)(16 * bpc) - 40.0f);
        }
        return;
    }
    int len = PAD_PATS[st->pad][g->pad_pat][i];
    if (!len)
        return;
    float e = (float)g->s.energy / 100.0f;
    int vel = (i % 4 == 0 ? 106 : 94) + (int)(hum * 10.0f) - (e < 0.5f ? 8 : 0);
    float gate = st->pad == PAD_STAB ? 0.55f : st->pad == PAD_PULSE ? 0.7f : 0.9f;
    int t[3];
    triad(c, t);
    if (st->pad == PAD_COMP) {                     /* the ballad's hand: the full voicing, the top a little louder */
        for (int k = 0; k < c->n; ++k)
            schedule(g, delay + (uint32_t)((float)k * sf * 0.03f), SN_PADS, c->note[k], clampi(vel - (k == c->n - 1 ? 0 : 8), 1, 127),
                     sf * (float)len * gate);
        return;
    }
    for (int k = st->pad == PAD_PULSE ? 1 : 0; k < 3; ++k)   /* the stabs: the triad; the pulse its top two (the */
        schedule(g, delay, SN_PADS, t[k], clampi(vel - (k == 2 ? 0 : 6), 1, 127), sf * (float)len * gate);   /* drums' voices) */
}

static void play_bass(sn_groove *g, int i, uint32_t delay, float sf, float hum)
{
    const sn_chord *c = chord_now(g);
    if (g->sec == SN_SEC_IN) {                     /* the break-in: the leading chord's root, held to the downbeat */
        if (i == 0)
            schedule(g, delay, SN_BASS, bass_note(g, BR_ROOT, c, c), 104, sf * 16.0f - 30.0f);
        return;
    }
    bool tense = sn_tense(g) || g->sec == SN_SEC_END;
    const bass_pat_t *bp = &BASS_PATS[clampi(tense ? SN_BL_LONG : g->s.bassline, 0, SN_BL_COUNT - 1)]
                                     [tense ? 0 : g->bass_pat];
    int tone = bp->tone[i], len = bp->len[i];
    if (!tone)
        return;
    int bpc = sn_bars_per_chord(&g->s);
    bool last_bar = (sec_chord_bar(g) % bpc) == bpc - 1;
    const sn_chord *nx = last_bar ? chord_next(g) : c;
    if (tone == BR_APP && !last_bar)               /* an approach only leads into a new chord */
        tone = BR_OCT;
    int n = bass_note(g, tone, c, nx);
    int vel = (i % 4 == 0 ? 110 : i % 4 == 2 ? 102 : 94) + (int)(hum * 8.0f) - (tense ? 14 : 0);
    float gate = g->s.bassline == SN_BL_PULSE || g->s.bassline == SN_BL_OCTAVE || g->s.bassline == SN_BL_SEQ ? 0.7f
               : g->s.bassline == SN_BL_SYNCO ? 0.8f : 0.95f;
    schedule(g, delay, SN_BASS, n, clampi(vel, 1, 127), sf * (float)len * (tense ? 0.95f : gate));
}

static void play_keys(sn_groove *g, int i, uint32_t delay, float sf, float hum)
{
    const sn_style *st = &SN_STYLE[g->s.style];
    if (g->keys)                                   /* the player has the keys */
        return;
    if (!sn_in_full(g))
        return;
    const sn_chord *c = chord_now(g);
    float gate = (float)g->s.gate / 100.0f;
    if (st->hook == HOOK_ARP) {                    /* an arpeggiator through the chord, up and down */
        int every = g->s.speed == SN_SPEED_SLOW ? 2 : 1;
        if (i % every)
            return;
        static const uint8_t ORD[8] = {0, 1, 2, 3, 4, 3, 2, 1};
        int k = ORD[(i / every) % 8], oct = k / 4, t = k % 4;
        if (g->s.range < 2)
            oct = 0;
        int n = 60 + ((c->tone[t] - 60) % 12 + 12) % 12 + 12 * oct;
        if (n > 84)
            n -= 12;
        schedule(g, delay, SN_KEYS, n, clampi(82 + (i % 4 == 0 ? 20 : 0) + (int)(hum * 6.0f), 1, 127),
                 sf * (float)every * gate);
        return;
    }
    int rep = sn_repeat_bars(&g->s);
    if (st->hook == HOOK_SPARSE && (g->sec_bar % rep) % 2 == 0 && rep > 1)   /* sparse: only the answers sing */
        return;
    int hp = (g->sec_bar % rep) * 16 + i;
    if (g->hook[hp] == -128)
        return;
    int len = g->hook_len[hp];
    int base = 60 + g->s.key - (g->s.key > 6 ? 12 : 0);
    int n = step_note(g->s.scale, base, g->hook[hp]);
    if (len >= 2 || i % 4 == 0) {                  /* the long and strong notes onto the chord */
        int best = n, bd = 99;
        for (int t = 0; t < 3; ++t)
            for (int o = -1; o <= 1; ++o) {
                int cand = n - ((n % 12) - c->tone[t]) + o * 12;
                int d = cand - n < 0 ? n - cand : cand - n;
                if (d < bd) {
                    bd = d;
                    best = cand;
                }
            }
        n = best;
    }
    int vel = 100 + (i % 4 == 0 ? 10 : 0) + (int)(hum * 6.0f);
    schedule(g, delay, SN_KEYS, n, clampi(vel, 1, 127), sf * (float)len * gate);
}

/* one sixteenth: the parts, swung and humanized */
static void do_step(sn_groove *g)
{
    if (++g->step >= 16) {
        g->step = 0;
        g->bar++;
        bar_line(g);
    }
    int i = g->step;
    if (i == 0)
        g->rnd = (uint32_t)g->bar * 2654435761u + (uint32_t)g->s.song * 97u + 1u;
    float sf = step_frames(g);
    uint32_t delay = 0;
    if (i % 2 && g->s.swing)                        /* swing: the off sixteenths late */
        delay = (uint32_t)(sf * (float)g->s.swing / 100.0f * 0.66f);
    for (int p = 0; p < SN_PARTS; ++p) {
        bool out = part_out(g, p);
        if (out != g->muted[p] && out)
            part_off(g, p);
        g->muted[p] = out;
    }
    for (int p = 0; p < SN_PARTS; ++p) {
        if (g->muted[p])
            continue;
        float hum = (rnd_f(&g->rnd) - 0.5f) * 2.0f * (float)g->s.human / 100.0f;
        uint32_t d = delay + (uint32_t)((hum > 0 ? hum : 0.0f) * g->sr * 0.010f);   /* late only, up to 10 ms */
        switch (p) {
        case SN_PADS: play_pads(g, i, d, sf, hum); break;
        case SN_BASS: play_bass(g, i, d, sf, hum); break;
        case SN_KEYS: play_keys(g, i, d, sf, hum); break;
        default: play_drums(g, i, d, hum); break;
        }
    }
}

void sn_play(sn_groove *g, bool on)
{
    if (on == g->playing)
        return;
    sn_all_off(g);
    npend = 0;
    g->playing = on;
    g->clock = 0;
    g->wait = 0;
    g->step = -1;
    g->bar = 0;
    g->rec_held = g->in_next = g->drama_next = g->out_next = g->fill_bar = false;
    g->crash_next = false;
    g->sec = g->s.form ? SN_SEC_HUSH : SN_SEC_FULL;
    g->sec_bar = 0;
    g->round = 0;
    g->hook_var = 0;
    g->dramas_done = 0;
    g->fill_from = 8;
    g->tr = 0;
    if (on) {
        if (g->next_style >= 0 || g->next_song >= 0) {   /* (queued while stopped: at once) */
            g->bar = -1;
            g->step = 15;
            bar_line(g);
            g->bar = 0;
            g->step = -1;
            g->sec = g->s.form ? SN_SEC_HUSH : SN_SEC_FULL;
            g->sec_bar = 0;
        }
        if (!g->s.form && g->s.crash != SN_CRASH_OFF)
            g->crash_next = true;
    }
    sn_rebuild(g);
}

/* a queue while stopped: at once, the form where it was */
static void apply_now(sn_groove *g)
{
    int b = g->bar, sb = g->sec_bar, sc = g->sec;
    g->bar = -1;
    bar_line(g);
    g->bar = b;
    g->sec = sc;
    g->sec_bar = sb;
}

void sn_queue_style(sn_groove *g, int style)
{
    style = ((style % SN_STYLES) + SN_STYLES) % SN_STYLES;
    g->next_style = style;
    if (!g->playing) {
        apply_now(g);
    } else {
        char m[24] = "NEXT ";
        cpy(m + 5, SN_STYLE[style].name, (int)sizeof m - 5);
        msg(g, m);
    }
}

void sn_queue_song(sn_groove *g, int song)
{
    g->next_song = ((song % 1000) + 1000) % 1000;
    if (!g->playing)
        apply_now(g);
}

/* the break-in, fired: from this bar's half when it is still to come, else the next bar's */
static void fire_in(sn_groove *g)
{
    if (g->step < 8) {
        g->sec = SN_SEC_IN;
        g->sec_bar = 0;
        g->fill_from = 8;
        g->in_next = false;
    } else {
        g->in_next = true;
    }
    msg(g, "BREAK-IN");
}

void sn_breakin(sn_groove *g, bool down)
{
    if (!g->playing)
        return;
    if (down) {
        if (sn_tense(g)) {                         /* the hush or the rise: held, the tension holds; let go: in */
            g->rec_held = true;
            msg(g, "HOLD");
        } else if (sn_in_full(g)) {                /* the full band: a tom fill over this bar's last two beats */
            g->fill_bar = true;
            g->fill_bar_at = g->step < 8 ? g->bar : g->bar + 1;
        } else if (g->sec == SN_SEC_DRAMA) {       /* the drama: the way back in on the next bar */
            g->out_next = true;
            msg(g, "BACK IN");
        }
    } else if (g->rec_held) {
        g->rec_held = false;
        if (sn_tense(g))
            fire_in(g);
    }
}

void sn_drama(sn_groove *g)
{
    if (!g->playing)
        return;
    if (sn_in_full(g)) {
        g->drama_next = true;
        msg(g, "DRAMA NEXT BAR");
    } else {
        msg(g, "DRAMA FROM FULL");
    }
}

void sn_key(sn_groove *g, int k, int note_in, bool down)
{
    if (k < 0 || k >= 27)
        return;
    if (down) {
        int n = sn_snap(&g->s, note_in) + g->tr;   /* in the key, the key up in it */
        if (g->key_note[k] >= 0)
            emit(g, 0, SN_KEYS, g->key_note[k], 0);
        if (!g->keys)                              /* the riff makes way */
            part_off(g, SN_KEYS);
        g->keys |= 1u << k;
        g->key_note[k] = n;
        emit(g, 1, SN_KEYS, n, 104);
        feed(g, SN_KEYS, n, 104);
        g->hook_on_clock = g->clock;
    } else {
        g->keys &= ~(1u << k);
        if (g->key_note[k] >= 0)
            emit(g, 0, SN_KEYS, g->key_note[k], 0);
        g->key_note[k] = -1;
    }
}

/* the lift: how open the lifted parts' filter is in the form, 0..1 of its ceiling */
static float lift_open(const sn_groove *g)
{
    if (!g->playing)
        return 1.0f;
    float t = tension_now(g);
    switch (g->sec) {
    case SN_SEC_HUSH: return 0.25f + 0.2f * t;
    case SN_SEC_RISE: return 0.3f + 0.6f * t;
    case SN_SEC_DRAMA: return 0.45f + 0.5f * t;
    case SN_SEC_END: return 0.3f + 0.6f * t;
    default: return 1.0f;
    }
}

/* the block: notes in time, then the lift for the glue */
void sn_process(sn_groove *g, int frames)
{
    if (g->playing) {
        float sf = step_frames(g);
        int left = frames;
        while (left > 0) {
            if (g->wait <= 0.0f) {
                do_step(g);
                g->wait += sf;
            }
            int chunk = (int)g->wait + 1;
            if (chunk > left)
                chunk = left;
            if (chunk < 1)
                chunk = 1;
            g->clock += (uint32_t)chunk;
            g->wait -= (float)chunk;
            left -= chunk;
            for (int k = 0; k < npend;) {          /* the swung and late notes */
                if ((int32_t)(g->clock - pend[k].at) >= 0) {
                    int nn = pend[k].note;
                    if (pend[k].part == SN_DRUMS && (nn & 0x80))
                        emit(g, 0, SN_DRUMS, nn & 0x7F, 0);   /* a choke */
                    else if (!g->muted[pend[k].part])
                        note(g, pend[k].part, nn, pend[k].vel, pend[k].len);
                    pend[k] = pend[--npend];
                } else {
                    ++k;
                }
            }
            for (int k = 0; k < g->nheld;) {       /* the note-offs */
                if ((int32_t)(g->clock - g->held[k].off) >= 0)
                    note_off_now(g, k);
                else
                    ++k;
            }
        }
    }
    g->tension = g->playing ? tension_now(g) : 0.0f;
    float cut = clampf((float)g->s.lift / 100.0f * clampf(lift_open(g), 0.0f, 1.0f), 0.0f, 1.0f);
    g->lift_now = cut;
    uint8_t mask = SN_STYLE[g->s.style].lift_parts;
    for (int p = 0; p < SN_PARTS; ++p)
        g->cut[p] = (uint8_t)((mask >> p) & 1u ? (int)(cut * 127.0f + 0.5f) : 127);
}
