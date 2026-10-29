/* SPDX-License-Identifier: GPL-3.0-only */
/* SNAPPER's sounds on Felucca's engines, and how the cards set a track: shared by the firmware (sn_fw.c) and the host
 * render (tests/sn_felucca.c), so both play the same. Needs core.h (track_t, P_*, the song) and the matrix's
 * MS_* / MD_* (mod.c) first.
 *
 * Tracks: 1 PADS, 2 BASS, 3 KEYS, 4 DRUMS. Each music part has a list of sixteen sounds (the SOUND page): an engine
 * and its factory preset, or a sound of Snapper's own on that engine (sns_own), or one of the FM6 patches
 * (sn_fm6.inc). The PIANO is the SAMPLE engine's set 1: six notes of a Kawai grand (VCSL, CC0) made bright
 * (tools/sn_piano.py). The drums are a kit: BIG, Snapper's own one-shots (the SAMPLE engine, set DRUMS:
 * tools/sn_drums.py at 44.1 kHz: the snare, the clap and the toms with their gated room in them, and the drum
 * machine's small kick, rim and hat for the hush), or BOX, the DRUM engine's 80 kit for all of it.
 *
 * One of the matrix's slots carries the groove's control, set every block by the glue:
 *   slot 1  MODW -> the sound's brightness (ANALOG and TRIO CUT, FM6 MLVL, ...) by -32: the LIFT (t->mw is
 *           127 - the groove's cut, so an open filter leaves the sound as its preset has it, and a closed one takes
 *           half the range off)
 *   slot 4  VEL -> AMP: the drums' velocities on the SAMPLE engine (it has none of its own) */
#ifndef SN_SOUNDS_H
#define SN_SOUNDS_H

#include "sn_groove.h"

enum { SNS_ANALOG = 0, SNS_PHASE = 2, SNS_LOFI = 3, SNS_SAMPLE = 4, SNS_VOICE = 5, SNS_TRIO = 6, SNS_WHEEL = 7,
       SNS_PHYS = 9, SNS_DRUM = 10, SNS_FM6 = 12 };
enum { SNS_KEEP = -1 };                   /* dist, glide, voice: as the preset leaves them */
enum { SNS_POLY = 0, SNS_MONO = 1, SNS_LEGATO = 2, SNS_UNISON = 3 };

typedef struct {
    const char *name;
    uint8_t engine, preset;
    int8_t dist, glide, voice;
    const preset_t *own;
    const uint8_t *fm6;
    int8_t bright;                        /* the E parameter the LIFT takes down (0..7), -1: none */
    int8_t trim;                          /* added to the level (F_DB steps), to even the sounds out: each part's
                                           * sounds at one loudness (tests/sn_audition.sh measures them) */
} sns_sound;

/* ---- Snapper's own sounds: Felucca's preset layout (core.h preset_t): {name, E0..E7, ATK DEC SUS REL, ENV->FLT,
 * mono, FX(dist, chorus, delay, reverb)}. The ANALOG E values: WAVE DTN MIX NOIS CUT RES DRV KTR; VOICE: VOWL VOWL2
 * TALK SHIFT BUZZ BRTH Q RAND. SAW LEAD is Tuna's (tuna_sounds_own.inc), OCTAVE Daurade's DISCO BASS ---- */
/* PADS: a poly synth of two saws a little apart, bright, a short pluck of the filter: the big chords of the anthems */
static const preset_t SNS_SYNTH = {"SYNTH", {0, 34, 64, 0, 108, 22, 12, 64}, {0, 64, 84, 30}, 12, 0, FX(0, 60, 25, 35)};
/* PADS and KEYS: a choir on the VOICE engine, between AH and OH */
static const preset_t SNS_CHOIR = {"CHOIR", {0, 0, 40, 0, 40, 26, 64, 6}, {24, 90, 112, 70}, 0, 0, FX(0, 70, 15, 70)};
static const preset_t SNS_CHOIRLEAD = {"CHOIR", {0, 0, 40, 0, 50, 22, 70, 6}, {10, 80, 112, 50}, 0, 1, FX(0, 50, 30, 60)};
/* KEYS: the anthem's lead: saws on every free voice (UNISON), a touch of glide */
static const preset_t SNS_SUPERLEAD = {"SUPER LEAD", {0, 30, 64, 0, 112, 18, 16, 64}, {1, 60, 104, 30}, 8, 1,
                                       FX(0, 50, 35, 30)};
/* BASS: the octave bass (Daurade's) */
static const preset_t SNS_OCTAVE = {"OCTAVE", {6, 0, 40, 0, 62, 35, 20, 64}, {0, 52, 60, 14}, 30, 1, FX(5, 0, 5, 10)};
/* BASS: two saws beating slowly against each other, low and dark */
static const preset_t SNS_REESE = {"REESE", {0, 22, 64, 0, 58, 20, 15, 64}, {0, 80, 110, 18}, 6, 1, FX(0, 0, 0, 8)};
/* BASS: a punchy analogue saw, short and dry */
static const preset_t SNS_SYNTHBASS = {"SYNTH BASS", {0, 0, 0, 0, 70, 40, 12, 64}, {0, 46, 30, 10}, 34, 1, FX(0, 0, 0, 6)};
/* BASS: a deep sine with a click on top */
static const preset_t SNS_SINEBASS = {"SINE BASS", {3, 0, 0, 0, 127, 0, 25, 0}, {0, 70, 100, 12}, 0, 1, FX(0, 0, 0, 4)};
/* KEYS: the pop lead: a square, plucked by the filter */
static const preset_t SNS_POP = {"POP", {1, 6, 64, 0, 100, 20, 5, 64}, {0, 55, 60, 20}, 25, 1, FX(0, 20, 35, 30)};
/* KEYS: the anthem lead: saws wide and driven, a slow glide */
static const preset_t SNS_ANTHEM = {"ANTHEM", {0, 26, 64, 0, 108, 15, 10, 64}, {4, 70, 110, 35}, 10, 1, FX(0, 40, 40, 40)};
/* KEYS: the whistle: a sine with a breath of noise */
static const preset_t SNS_WHISTLE = {"WHISTLE", {3, 0, 0, 6, 127, 0, 0, 64}, {6, 70, 110, 25}, 0, 1, FX(0, 20, 40, 45)};
static const preset_t SNS_SAWLEAD = {"SAW LEAD", {0, 0, 12, 12, 0, 96, 25, 64}, {2, 70, 100, 35}, 20, 1,
                                     FX(0, 10, 35, 25)};

#include "sn_fm6.inc"

#define S(n, e, p, b, tr) {(n), (uint8_t)(e), (uint8_t)(p), SNS_KEEP, SNS_KEEP, SNS_KEEP, 0, 0, (int8_t)(b), (int8_t)(tr)}
#define SX(n, e, p, d, gl, v, b, tr) {(n), (uint8_t)(e), (uint8_t)(p), (int8_t)(d), (int8_t)(gl), (int8_t)(v), 0, 0, (int8_t)(b), (int8_t)(tr)}
#define SO(n, e, p, own, d, gl, v, b, tr) {(n), (uint8_t)(e), (uint8_t)(p), (int8_t)(d), (int8_t)(gl), (int8_t)(v), &(own), 0, (int8_t)(b), (int8_t)(tr)}
#define SF(n, patch, gl, v, tr) {(n), (uint8_t)SNS_FM6, 0, SNS_KEEP, (int8_t)(gl), (int8_t)(v), 0, (patch), 2, (int8_t)(tr)}

/* the three music parts' lists, in the SOUND card's order (sn_groove.c names them in the styles). FM6's factory
 * presets: 0 TINE EP 1 BELL 2 FM BASS 3 BRASS 4 PAD 5 MARIMBA 6 ORGAN 7 PLUCK */
static const sns_sound SNS_SOUND[3][SN_SOUNDS] = {
    {   /* PADS */
        SF("DRONE", SN_FM6_DRONE, SNS_KEEP, SNS_POLY, 14),
        SO("CHOIR", SNS_VOICE, 0, SNS_CHOIR, SNS_KEEP, SNS_KEEP, SNS_POLY, -1, -1),
        S("STRINGS", SNS_ANALOG, 11, 4, -6),
        S("PWM", SNS_ANALOG, 3, 4, -5),                   /* PWM STR: pulse-width strings, wide and soft */
        S("TINE", SNS_FM6, 0, 2, -2),                     /* TINE EP: the ballad's electric piano */
        S("BRASS", SNS_FM6, 3, 2, -4),                    /* BRASS SECT: the horn section's stabs */
        S("PIANO", SNS_SAMPLE, 1, 4, -18),
        SF("GLASS", SN_FM6_GLASS, SNS_KEEP, SNS_POLY, 10),
        S("SOFT", SNS_FM6, 4, 2, 16),                     /* SOFT PAD */
        S("ORGAN", SNS_FM6, 6, 2, -6),                    /* DRAWBARS */
        SO("SYNTH", SNS_ANALOG, 0, SNS_SYNTH, SNS_KEEP, SNS_KEEP, SNS_POLY, 4, 0),
        S("BELL", SNS_FM6, 1, 2, -2),                     /* GLASS BELL */
        S("HORNS", SNS_PHASE, 0, 2, -2),                  /* PHASE's BRASS: a heroic section */
        SF("ORCH", SN_FM6_ORCHHIT, SNS_KEEP, SNS_POLY, 6),
        S("MALLET", SNS_FM6, 5, 2, 0),                    /* WOOD BARS */
        S("VOICES", SNS_PHASE, 2, 2, -3),                 /* PHASE's STRING: wide, orchestral */
    },
    {   /* BASS */
        SF("PUNCH", SN_FM6_PUNCH, SNS_KEEP, SNS_MONO, 0),
        S("FM BASS", SNS_FM6, 2, 2, -4),
        SF("SLIDE", SN_FM6_FRETLESS, 22, SNS_LEGATO, -5),       /* FRETLESS */
        SO("SYNTH", SNS_ANALOG, 0, SNS_SYNTHBASS, SNS_KEEP, SNS_KEEP, SNS_MONO, 4, 2),
        S("SQUARE", SNS_ANALOG, 2, 4, -1),
        S("SUB", SNS_ANALOG, 7, 4, -13),
        SF("SLAP", SN_FM6_SLAPBASS, SNS_KEEP, SNS_POLY, 0),
        SO("OCTAVE", SNS_ANALOG, 0, SNS_OCTAVE, SNS_KEEP, SNS_KEEP, SNS_MONO, 4, -1),
        SX("DX", SNS_FM6, 7, SNS_KEEP, SNS_KEEP, SNS_MONO, 2, -1),        /* FM6's PLUCK, low and hard */
        S("FAT", SNS_TRIO, 0, 5, 2),
        SO("SINE", SNS_ANALOG, 0, SNS_SINEBASS, SNS_KEEP, SNS_KEEP, SNS_MONO, -1, -14),
        SX("GROWL", SNS_PHASE, 0, 6, SNS_KEEP, SNS_MONO, 2, -9),          /* PHASE's BRASS in the low end */
        SX("ORGAN", SNS_WHEEL, 1, SNS_KEEP, SNS_KEEP, SNS_MONO, 3, -4),
        SO("REESE", SNS_ANALOG, 0, SNS_REESE, SNS_KEEP, SNS_KEEP, SNS_MONO, 4, -8),
        SX("WOW", SNS_VOICE, 2, SNS_KEEP, 24, SNS_LEGATO, -1, -3),
        SX("ACID", SNS_ANALOG, 4, SNS_KEEP, 18, SNS_LEGATO, 4, 6),
    },
    {   /* KEYS */
        S("TINE", SNS_FM6, 0, 2, -2),
        S("BRASS", SNS_FM6, 3, 2, -4),
        S("MALLET", SNS_FM6, 5, 2, 0),
        S("SYNC", SNS_TRIO, 2, 5, -6),
        SO("LEAD", SNS_TRIO, 0, SNS_SAWLEAD, SNS_KEEP, 10, SNS_KEEP, 5, -8),
        S("HORNS", SNS_PHASE, 0, 2, -2),                  /* PHASE's BRASS: a horn line */
        S("PIANO", SNS_SAMPLE, 1, 4, -18),
        S("FLUTE", SNS_WHEEL, 3, 3, -7),                  /* SOFT FLUTE */
        SO("POP", SNS_ANALOG, 0, SNS_POP, SNS_KEEP, 4, SNS_MONO, 4, -3),
        SO("ANTHEM", SNS_ANALOG, 0, SNS_ANTHEM, SNS_KEEP, 14, SNS_LEGATO, 4, -12),
        SF("ORCH", SN_FM6_ORCHHIT, SNS_KEEP, SNS_POLY, 6),
        SO("CHOIR", SNS_VOICE, 0, SNS_CHOIRLEAD, SNS_KEEP, 10, SNS_LEGATO, -1, -12),
        SO("WHISTLE", SNS_ANALOG, 0, SNS_WHISTLE, SNS_KEEP, 16, SNS_LEGATO, -1, -16),
        S("PLUCK", SNS_ANALOG, 8, 4, 2),
        S("ORGAN", SNS_FM6, 6, 2, -6),
        SO("SUPER", SNS_ANALOG, 0, SNS_SUPERLEAD, SNS_KEEP, 6, SNS_UNISON, 4, -8),
    },
};
#undef S
#undef SX
#undef SO
#undef SF

static const char *const SNS_KIT_NAMES[SN_KIT_COUNT] = {"BIG", "BOX"};
static const int8_t SNS_KIT_TRIM[SN_KIT_COUNT] = {-14, -1};   /* (tests/sn_audition.sh, as the sounds' trims) */
static inline const char *sns_name(int part, int i)
{
    if (part == SN_DRUMS)
        return SNS_KIT_NAMES[(unsigned)i % SN_KIT_COUNT];
    return SNS_SOUND[part % 3][(unsigned)i % SN_SOUNDS].name;
}

/* the sound a part plays now: its card's */
static inline const sns_sound *sns_of(const sn_groove *g, int part)
{
    return &SNS_SOUND[part % 3][(unsigned)g->s.snd[part] % SN_SOUNDS];
}

/* ---- the drums: the kit's engine and each drum's note ---- */
#define SNS_KIT_BASE 53                   /* the DRUMS set's first key (tools/gen_samples.py KIT_BASE) */
static inline uint32_t sns_kit_engine(int kit) { return kit == SN_KIT_BIG ? SNS_SAMPLE : SNS_DRUM; }
/* the General MIDI note of each drum (the BOX kit's, and MIDI out's) */
static const uint8_t SNS_GM[SN_DR_COUNT] = {36, 38, 39, 42, 46, 49, 50, 47, 43, 35, 37, 42};
/* the note a drum plays on the kit */
static inline int sns_drum_note(const sn_settings *s, int drum)
{
    drum = (unsigned)drum % SN_DR_COUNT;
    if (s->snd[3] == SN_KIT_BIG) {        /* the DRUMS set: KICK, KICK ROUND, SNARE, CLAP, HAT, OPEN HAT, CRASH,
                                           * TOM HI, TOM MID, TOM LO, BOX KICK, BOX RIM, BOX HAT */
        static const uint8_t K[SN_DR_COUNT] = {0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
        int k = K[drum];
        if (drum == SN_DR_KICK && s->kick == SN_KICK_ROUND)
            k = 1;
        return SNS_KIT_BASE + k;
    }
    return SNS_GM[drum];
}
/* the DRUM engine's eight (E0 KIT, E1 TUNE, E2 TONE, E3 DECY, E4 SNAP, E5 ACC, E6 KICK, E7 DRV) for
 * the BOX kit: its 80 kit, ROOM its decay */
static inline void sns_kit_params(const sn_settings *s, int16_t *e)
{
    e[0] = 4;                              /* DK_80 (eng_drum.c) */
    e[1] = 64;
    e[2] = 64;
    e[3] = (int16_t)(40 + s->room * 50 / 100);
    e[4] = 90;
    e[5] = 100;
    e[6] = (int16_t)(s->kick == SN_KICK_ROUND ? 1 : 0);
    e[7] = 10;
}

static inline int16_t sns_clamp(int v, int lo, int hi) { return (int16_t)(v < lo ? lo : v > hi ? hi : v); }
#define SNS_PIANO_CUT 55                  /* the piano's CUT (eng_sample.c: a gentle one-pole, 55: about 5 kHz) */
#define SNS_PIANO_ROOM 35                 /* the piano's reverb send over SPACE's (0..127) */

/* the TIME card in Felucca's delay times (params.c N_DTIME): 1/16, 1/8, 1/8D, 1/4 */
static const uint8_t SNS_DTIME[SN_DTIME_COUNT] = {2, 1, 10, 0};
static const char *const SNS_DTIME_NAMES[SN_DTIME_COUNT] = {"1/16", "1/8", "3/16", "1/4"};

/* a VOICE sound's glide between its vowels: an eighth of the tempo, as the VOICE engine's TALK time (gen_tables.py:
 * a value v is 10000^(v/127) ms) */
static inline int16_t sns_talk(const sn_settings *s)
{
    float ms = 60000.0f / (float)(s->bpm ? s->bpm : 120) * 0.5f;
    /* v = 127 log10(ms) / 4, log10 without libm: by the powers of ten and a few steps of the series */
    int dec = 0;
    float x = ms;
    while (x >= 10.0f) { x *= 0.1f; dec++; }
    float y = (x - 1.0f) / (x + 1.0f), y2 = y * y;   /* ln(x) = 2 atanh(y) */
    float ln = 2.0f * y * (1.0f + y2 / 3.0f + y2 * y2 / 5.0f + y2 * y2 * y2 / 7.0f);
    float v = 127.0f * ((float)dec + ln * 0.4342945f) / 4.0f;
    return sns_clamp((int)(v + 0.5f), 0, 127);
}

/* a part's track from the cards, over its sound's preset: the level, the place, the sends (ROOM the drums'), TONE on
 * the parts the lift filters, ENSEMBLE on the pads and the keys, a VOICE sound's vowels (AH to OH), and the matrix's
 * slot for the groove (above). Main loop, the audio held off (or the host) */
static inline void sns_track(track_t *t, const sn_groove *g, int part)
{
    const sn_settings *s = &g->s;
    const sn_style *st = &SN_STYLE[s->style];
    const sns_sound *snd = part == SN_DRUMS ? 0 : sns_of(g, part);
    bool lifted = (st->lift_parts >> part) & 1u;
    /* MIX: 100 is -2 dB (headroom: the master limiter stays out of it), each step down 0.3 dB (MIX 50: -17 dB), 0 off;
     * then the sound's trim (F_DB: half-dB steps); the pads sit 2 dB lower, under the keys */
    int lvl = 108 - (100 - s->lvl[part]) * 6 / 10 + (snd ? snd->trim : SNS_KIT_TRIM[s->snd[3] % SN_KIT_COUNT]) +
              (part == SN_PADS ? -4 : 0);
    t->p[P_LEVEL] = sns_clamp(s->lvl[part] ? lvl : 0, 1, 127);
    /* the place: the pads a little left and spread, the keys a little right, the bass and drums centred */
    int w = s->width;
    t->p[P_PAN] = (int16_t)(part == SN_PADS ? -w * 18 / 100 : part == SN_KEYS ? w * 18 / 100 : 0);
    t->p[P_SPRD] = sns_clamp(part == SN_PADS || (part == SN_KEYS && snd->voice == SNS_UNISON) ? w * 90 / 100 : 0, 0, 127);
    /* the sends: SPACE's REVERB and DELAY; the drums' room is ROOM */
    static const uint8_t REV[SN_PARTS] = {100, 25, 90, 0}, DLY[SN_PARTS] = {50, 0, 100, 0};
    t->p[P_REV] = part == SN_DRUMS ? sns_clamp(s->room * 127 / 100 * 55 / 100, 0, 127)
                                   : sns_clamp(s->reverb * 127 / 100 * REV[part] / 100, 0, 127);
    t->p[P_DLY] = sns_clamp(s->delay * 127 / 100 * DLY[part] / 100, 0, 127);
    /* the matrix: the lift */
    t->p[P_M1SRC] = MS_MODW;
    t->p[P_M1DST] = (int16_t)(snd && snd->bright >= 0 ? MD_E1 + snd->bright : MD_OFF);
    t->p[P_M1AMT] = -32;
    t->p[P_M2SRC] = MS_OFF;
    t->p[P_M2DST] = MD_OFF;
    if (part == SN_DRUMS) {
        t->p[P_VOICE] = V_POLY;
        t->p[P_ITYPE] = 0;
        if (s->snd[3] == SN_KIT_BIG) {            /* the samples: their velocity */
            t->p[P_M4SRC] = MS_VEL;
            t->p[P_M4DST] = MD_AMP;
            t->p[P_M4AMT] = 64;
        } else {
            t->p[P_M4SRC] = MS_OFF;
            sns_kit_params(s, &t->p[P_E0]);
        }
        t->p[P_DIST] = 0;
        return;
    }
    if (snd->engine == SNS_SAMPLE) {             /* the piano: its velocity (the SAMPLE engine has none of its own), */
        t->p[P_M4SRC] = MS_VEL;                  /* a low-pass at about 5 kHz to take its edge off, and a room of its */
        t->p[P_M4DST] = MD_AMP;                  /* own over SPACE's REVERB */
        t->p[P_M4AMT] = 64;
        t->p[P_E4] = SNS_PIANO_CUT;
        t->p[P_REV] = sns_clamp(t->p[P_REV] + SNS_PIANO_ROOM, 0, 127);
    } else {
        t->p[P_M4SRC] = MS_OFF;
    }
    if (snd->glide >= 0)
        t->p[P_GLIDE] = snd->glide;
    if (snd->voice >= 0)
        t->p[P_VOICE] = (int16_t)(snd->voice == SNS_UNISON ? V_UNISON : snd->voice == SNS_LEGATO ? V_LEGATO
                                  : snd->voice == SNS_MONO ? V_MONO : V_POLY);
    if (snd->engine == SNS_VOICE && snd->own) {  /* a choir: AH to OH (eng_formant.c: A 0, O 95), at the tempo */
        t->p[P_E0] = 0;
        t->p[P_E1] = 95;
        t->p[P_E2] = sns_talk(s);
        t->p[P_E3] = 0;
    }
    /* ENSEMBLE: the chorus on the pads and the keys, over what the sound has of its own */
    if (part != SN_BASS)
        t->p[P_CHOR] = sns_clamp(s->ensemble * 127 / 100, 0, 127);
    /* TONE on the lifted parts: RESO, DRIVE, CRUSH. A lifted ANALOG or TRIO sound starts bright (CUT at least 100):
     * the lift brings the darkness, a dark preset under it would go silent */
    if (lifted) {
        if ((snd->engine == SNS_ANALOG || snd->engine == SNS_TRIO) && snd->bright >= 0 && t->p[P_E0 + snd->bright] < 100)
            t->p[P_E0 + snd->bright] = 100;
        if (snd->engine == SNS_ANALOG)
            t->p[P_E5] = sns_clamp(s->reso * 120 / 100, 0, 127);
        else if (snd->engine == SNS_TRIO)
            t->p[P_E6] = sns_clamp(s->reso * 120 / 100, 0, 127);
        t->p[P_DIST] = sns_clamp((snd->dist >= 0 ? snd->dist : 0) + s->drive * 50 / 100, 0, 127);   /* (DRIVE 100: 50) */
        if (s->crush) {
            t->p[P_ITYPE] = 5;                    /* IT_CRUSH: the bits fall as CRUSH rises, the rate with them */
            t->p[P_IA] = sns_clamp(127 - s->crush * 100 / 100, 16, 127);
            t->p[P_IB] = sns_clamp(127 - s->crush * 70 / 100, 0, 127);
            t->p[P_IC] = 110;
            t->p[P_IMIX] = 127;
        } else {
            t->p[P_ITYPE] = 0;
        }
    } else {
        if (snd->dist >= 0)
            t->p[P_DIST] = snd->dist;
        t->p[P_ITYPE] = 0;
    }
}

/* the song's own: the tempo for the delay and the LFOs, the delay, the reverb's room */
static inline void sns_song(const sn_settings *s)
{
    song.g[G_BPM] = s->bpm;
    song.g[G_DTIME] = SNS_DTIME[s->dtime % SN_DTIME_COUNT];
    song.g[G_DFDBK] = 45;
    song.g[G_DMIX] = 100;
    song.g[G_RSIZE] = (int16_t)(60 + (s->reverb > s->room ? s->reverb : s->room) * 30 / 100);
    song.g[G_RDAMP] = 50;
    song.g[G_RTYPE] = 0;                          /* (Felucca's ROOM model) */
}

/* the block's controls into the tracks (the audio ISR, before the mix): the lift */
static inline void sns_block(const sn_groove *g)
{
    for (int p = 0; p < SN_PARTS; ++p) {
        trk[p].mw = (uint8_t)(127 - g->cut[p]);
        trk[p].ex_off = 0;
    }
}

#endif
