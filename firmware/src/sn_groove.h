/* SPDX-License-Identifier: GPL-3.0-only */
/* SNAPPER groove engine: ten styles of generative 80s drum-heavy pop on four parts (PADS, BASS, KEYS, DRUMS), built
 * on tension: a song starts with a drone and a bare drum machine, builds, and then the big gated drums break in on the
 * downbeat. It runs in time, sixteenth by sixteenth, and hands each note-on and note-off to the sound (Felucca's
 * tracks on the FM-1, a renderer on the host). Besides the notes it computes, every block, the LIFT: how far the
 * filter of the lifted parts is open (0..127 per part), closed in the hush, opening through the rise, open in the
 * full band.
 *
 * A song is a style plus a number (the SONG, turned by ALGORITHM): the number picks the chord loop, the parts'
 * patterns and the riff, so the same style and number always play the same song. A new style or song comes in on
 * the next bar line; the knobs act at once.
 *
 * The song's form (FORM on): HUSH (the drone and the drum machine), RISE (the bass in, the filter opening, the
 * machine busier), IN (the break-in: a tom fill over the bar's second half), FULL (the gated kit and the band, a
 * crash on the downbeat), DRAMA (the band gone to one held chord and the drone, a tom roll swelling in), IN again
 * (a whole bar of toms), FULL again (KEY UP higher), up to DRAMAS times, then END (down to the drone) and round again
 * with a new riff. BUILD sets how long HUSH and RISE last (8, 16, 32 bars, or HOLD: until REC). DRAMA AUTO puts the
 * dramas in the form; OFF leaves them to SEQ. FORM off: the full band on and on.
 *
 * Performance: REC is BREAK-IN (a tap in the hush or the rise: the fill from the bar's half, or the next bar's when
 * the half has passed, the full band on the downbeat after; held there: the tension holds until let go; in the full
 * band a tom fill into a crash; in a drama, the way back in now), SEQ is DRAMA (from the full band, on the next bar),
 * the keys (KEYS live, snapped to the scale).
 *
 * No libm, no doubles: floats on the FPU (single precision), as Sill, Daurade, Flyfish and Tuna. */
#ifndef SN_GROOVE_H
#define SN_GROOVE_H

#include <stdbool.h>
#include <stdint.h>

enum { SN_PADS, SN_BASS, SN_KEYS, SN_DRUMS, SN_PARTS };
enum { SN_SIDE_LOUD, SN_SIDE_NIGHT };
#define SN_STYLES 10
enum { SN_ST_SLOWBURN, SN_ST_BIGROOM, SN_ST_BRASSHIT, SN_ST_STADIUM, SN_ST_MACHINE,
       SN_ST_NIGHTDRIVE, SN_ST_LATENIGHT, SN_ST_UNDERTOW, SN_ST_TOMTOM, SN_ST_RHYTHMBOX };
enum { SN_SC_MINOR, SN_SC_MAJOR, SN_SC_DORIAN, SN_SC_MIXO, SN_SCALES };
enum { SN_HATS_8THS, SN_HATS_16THS, SN_HATS_OPEN, SN_HATS_COUNT };
enum { SN_FILLS_OFF, SN_FILLS_8, SN_FILLS_4, SN_FILLS_COUNT };
enum { SN_BL_PULSE, SN_BL_LONG, SN_BL_SYNCO, SN_BL_OCTAVE, SN_BL_SEQ, SN_BL_SLIDE, SN_BL_COUNT };
enum { SN_KIT_BIG, SN_KIT_BOX, SN_KIT_COUNT };
enum { SN_KICK_PUNCH, SN_KICK_ROUND, SN_KICK_COUNT };
enum { SN_CLAP_SNARE, SN_CLAP_CLAP, SN_CLAP_BOTH, SN_CLAP_COUNT };
enum { SN_CRASH_OFF, SN_CRASH_SECTION, SN_CRASH_8, SN_CRASH_COUNT };
enum { SN_DTIME_16, SN_DTIME_8, SN_DTIME_3_16, SN_DTIME_4, SN_DTIME_COUNT };
enum { SN_SPEED_SLOW, SN_SPEED_MID, SN_SPEED_FAST, SN_SPEED_COUNT };
enum { SN_BUILD_8, SN_BUILD_16, SN_BUILD_32, SN_BUILD_HOLD, SN_BUILD_COUNT };
enum { SN_DRAMA_OFF, SN_DRAMA_AUTO, SN_DRAMA_COUNT };
/* the drums as the groove names them; the glue maps them to the kit (sn_sounds.h). The big kit's gated snare, clap
 * and toms; the drum machine's own small kick, rim and hat for the hush and the rise */
enum { SN_DR_KICK, SN_DR_SNARE, SN_DR_CLAP, SN_DR_HAT, SN_DR_OPEN, SN_DR_CRASH, SN_DR_TOMH, SN_DR_TOMM, SN_DR_TOML,
       SN_DR_MKICK, SN_DR_MRIM, SN_DR_MHAT, SN_DR_COUNT };
/* the form's sections, in the order they play (FORM on); after the last FULL, END, then round again from HUSH */
enum { SN_SEC_HUSH, SN_SEC_RISE, SN_SEC_IN, SN_SEC_FULL, SN_SEC_DRAMA, SN_SEC_IN2, SN_SEC_FULL2, SN_SEC_END, SN_SECS };
#define SN_SETS 8
#define SN_SOUNDS 16                               /* each part's SOUND list (sn_sounds.h) */
#define SN_LOOPS 12                                /* the chord loops (sn_groove.c SN_LOOP) */
#define SN_TR_MAX 4                                /* the key up, all dramas together: up to +4 */
#define SN_DRAMA_BARS 4
#define SN_END_BARS 4

/* what the player set: the cards (and the knobs above them). Saved as it is (sn_fw.c), so append only */
typedef struct {
    uint8_t style;
    uint8_t build, drama, energy, room;            /* PLAY: SN_BUILD_*, SN_DRAMA_*, ENERGY 0..100, ROOM 0..100 */
    uint8_t snd[4];                                /* SOUND: PADS BASS KEYS (indices into each part's list), DRUMS = kit */
    uint8_t reso, drive, ensemble, crush;          /* TONE 0..100 (CRUSH 0: off) */
    uint8_t key, scale, chords, bars;              /* KEY: 0..11 (C..B), SN_SC_*, the loop (0..SN_LOOPS-1), 1 2 4 */
    uint8_t swing, human, hats, fills;             /* GROOVE: swing 0..60 %, human 0..100 (no card), SN_HATS_*, SN_FILLS_* */
    uint8_t bassline;                              /* GROOVE: SN_BL_* */
    uint8_t kick, clap, crash;                     /* DRUMS: SN_KICK_*, SN_CLAP_*, SN_CRASH_* */
    uint8_t lvl[4];                                /* MIX 0..100 */
    uint8_t reverb, delay, dtime, width;           /* SPACE */
    uint8_t form, full, dramas, keyup;             /* FORM: on, full bars 8 16, dramas a song 1 2, semitones up 0..2 */
    uint8_t speed, range, gate, repeat;            /* KEYS: SN_SPEED_*, octaves 1 2, gate 10..100 %, bars 1 2 4 */
    uint8_t bpm;                                   /* SELECT: 60..200 */
    uint8_t palette, look;                         /* MENU: COLOR, LOOK (FLAT, LINE) */
    uint8_t lift;                                  /* the lift's ceiling (the style's, no card) */
    uint16_t song;                                 /* ALGORITHM: the song number in the style, 0..999 */
} sn_settings;

typedef struct {
    uint8_t on, part, note, vel;                   /* DRUMS: note is SN_DR_*; vel 1..127 (on) */
} sn_event;

/* what the screen hears: each note-on */
typedef struct { uint8_t part, note, vel, pad; } sn_seen;

/* a chord as played: its root (pitch class), the notes (MIDI) it voices, its name (with the key up in it) */
typedef struct {
    uint8_t root, n;
    uint8_t note[5];                               /* the pads' voicing, low to high */
    uint8_t tone[4];                               /* chord tones as pitch classes: root, third, fifth, seventh */
    char name[8];
} sn_chord;

#define SN_HELD 24
#define SN_FEED 32
#define SN_LOOP_MAX 8
#define SN_HOOK_LEN 64                             /* the riff: up to four bars of sixteenths (REPEAT) */

typedef struct {
    sn_settings s;
    float sr;
    bool playing;
    uint32_t clock;                                /* frames since PLAY */
    float wait;                                    /* frames until the next sixteenth */
    int step;                                      /* the sixteenth sounding in its bar (0..15; -1 before the first) */
    int bar;                                       /* bars since PLAY (0: the first) */
    /* the song: built from the style, the song number, the key and the loop (sn_rebuild) */
    sn_chord loop[SN_LOOP_MAX];
    int nloop;
    uint8_t pad_pat, bass_pat, drum_var, hook_var;
    int8_t hook[SN_HOOK_LEN];                      /* scale steps from the key's tonic (-128: rest); length in hook_len */
    uint8_t hook_len[SN_HOOK_LEN];
    uint32_t rnd;                                  /* the per-note randomness (human, ghosts): seeded per bar */
    /* what waits for the next bar line */
    int next_style;                                /* -1 none */
    int next_song;                                 /* -1 none */
    /* the key up: the song's notes this many semitones up (each drama's KEY UP, added) */
    int tr;
    /* the form */
    int sec, sec_bar;                              /* the section (SN_SEC_*), bars into it */
    int round;
    int dramas_done;                               /* this round's dramas so far */
    int fill_from;                                 /* IN: the sixteenth the fill starts on (8: the bar's half, 0: all) */
    /* performance */
    bool rec_held;                                 /* REC held in the hush or the rise: the tension holds */
    bool in_next;                                  /* the break-in on the next bar */
    bool drama_next;                               /* SEQ: a drama on the next bar */
    bool out_next;                                 /* REC in a drama: the way back in on the next bar */
    bool fill_bar;                                 /* REC in the full band: a tom fill over this bar's last beats */
    int fill_bar_at;                               /* the bar it plays in */
    bool crash_next;                               /* a crash on the next downbeat */
    uint32_t keys;                                 /* the FM-1's keys held (bit note - 53) */
    int key_note[27];                              /* the note each key plays (snapped, the key up in it), -1 none */
    /* the block's controls for the glue */
    uint8_t cut[SN_PARTS];                         /* 0..127: how far each part's filter is open */
    float lift_now;                                /* 0..1: the filter for the screen */
    float tension;                                 /* 0..1: how near the break-in (the screen's meter) */
    bool muted[SN_PARTS];                          /* out (the form, a level of 0) */
    /* what each part played, a bit per sixteenth: the bar playing and the bar before (the screen) */
    uint16_t hits[SN_PARTS], hits_last[SN_PARTS];
    uint16_t drum_hits[SN_DR_COUNT], drum_last[SN_DR_COUNT];
    uint32_t hook_on_clock;                        /* the riff's latest note-on */
    /* the notes sounding: their note-off at clock off */
    struct { uint8_t note, part; uint32_t off; } held[SN_HELD];
    int nheld;
    sn_seen feed[SN_FEED];
    uint8_t feed_w, feed_r;
    char msg[24];                                  /* a word for the screen ("NEXT STADIUM", "DRAMA"); the UI clears it */
    uint8_t msg_new;
    uint8_t sound_sig;                             /* bumped when every part's sound must load again (a style, a set) */
    void (*sound)(void *ctx, const sn_event *e);
    void *sound_ctx;
} sn_groove;

typedef struct {
    const char *name;
    uint8_t side, bpm, key, scale;
    uint8_t build, drama, energy, room;
    uint8_t snd[4];                                /* PADS BASS KEYS sound, the kit */
    uint8_t loops[4];                              /* the chord loops a song picks from (SN_LOOP indices) */
    uint8_t bars;                                  /* bars a chord */
    uint8_t swing, hats, fills, bassline;
    uint8_t kick, clap, crash;
    uint8_t pad, hook, drums, machine;             /* the rhythm families (sn_groove.c) */
    uint8_t speed, range, gate, repeat;            /* the KEYS page */
    uint8_t lift, lift_parts;                      /* the lift's ceiling; bit per part: what the LIFT filters */
    uint8_t reverb, delay, dtime;
    uint8_t reso, drive, ensemble;
    uint8_t alt[3][3];                             /* PADS BASS KEYS: three more sounds each a song can pick (sn_song_sounds) */
} sn_style;

extern const sn_style SN_STYLE[SN_STYLES];

void sn_defaults(sn_settings *s);                  /* SLOWBURN, as it starts */
void sn_style_apply(sn_settings *s, int style);    /* a style's own settings into s (PLAY SOUND TONE KEY GROOVE DRUMS SPACE
                                                    * KEYS and the tempo); MIX FORM MENU stay */
void sn_settings_fix(sn_settings *s);              /* every field into its range (a loaded record) */
void sn_song_sounds(sn_settings *s);               /* the song's sounds: song 1 (0) the style's own, the others one of four
                                                    * per part, seeded by the song (the kit stays the style's) */
void sn_init(sn_groove *g, float sample_rate);
void sn_set_sound(sn_groove *g, void (*fn)(void *ctx, const sn_event *e), void *ctx);

void sn_play(sn_groove *g, bool on);
void sn_rebuild(sn_groove *g);                     /* the song again from the settings (KEY, the loop, the riff) */
void sn_queue_style(sn_groove *g, int style);      /* playing: on the next bar line; stopped: at once */
void sn_queue_song(sn_groove *g, int song);
void sn_breakin(sn_groove *g, bool down);          /* REC: down and up both count (see above) */
void sn_drama(sn_groove *g);                       /* SEQ: a drama from the next bar */
void sn_key(sn_groove *g, int key, int note, bool down);   /* the FM-1's key (0..26) playing note: KEYS, snapped */
void sn_process(sn_groove *g, int frames);         /* the audio: notes in time, the lift */
void sn_all_off(sn_groove *g);

bool sn_feed_pop(sn_groove *g, sn_seen *n);
int sn_chord_now(const sn_groove *g);              /* the loop's chord sounding (0..nloop-1) */
int sn_bars_per_chord(const sn_settings *s);
int sn_sec_len(const sn_groove *g, int sec);       /* a section's bars (0: open, BUILD HOLD's rise) */
int sn_build_bars(const sn_settings *s);           /* HUSH and RISE together (HOLD: 8, the rise then open) */
int sn_repeat_bars(const sn_settings *s);          /* the riff's phrase: 1 2 4 bars */
bool sn_in_full(const sn_groove *g);               /* the full band playing (the form's, or FORM off) */
bool sn_tense(const sn_groove *g);                 /* the hush or the rise: REC breaks in */
const char *sn_style_name(int i);
const char *sn_scale_name(int i);
const char *sn_note_name(int pc);
const char *sn_sec_name(int sec);                  /* "HUSH" .. */
const char *sn_sec_short(int sec);                 /* "HUSH" "RISE" "IN" .. */
void sn_loop_name(const sn_settings *s, int loop, char *out, int max);   /* "LOOP 4" */
int sn_snap(const sn_settings *s, int note);       /* a note onto the scale (the nearest step, ties down) */

#endif
