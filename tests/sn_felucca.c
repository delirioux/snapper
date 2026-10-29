/* SPDX-License-Identifier: GPL-3.0-only */
/* SNAPPER through Felucca's engines on the host: the groove's notes on the real DSP (the same sources as the
 * firmware, through hostsim.c), the lift on the tracks as the firmware sets it, to a WAV. What the
 * FM-1 will play, short of its hardware; and the voice budget, the peaks and the CPU against Felucca's reference.
 *
 *   tests/sn_felucca.sh OUTDIR [SECONDS]
 *   env STYLE=0..9, SONG, FORM=0/1, BUILD=0..3, BREAKIN=bar (REC tapped at that bar), DRAMA=bar (SEQ at that bar),
 *       KEYS=bar (a key held over that bar, KEYS live), KIT=0..1, LIFT, ENERGY, ROOM, NAME (the file:
 *       OUTDIR/snapper_NAME.wav); with STEMS=1 each part to its own WAV as well (the others' levels at 0) */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "sn_groove.h"
#include "sn_sounds.h"

static int env_int(const char *n, int def) { const char *v = getenv(n); return v ? atoi(v) : def; }

static sn_groove G;
static long n_on, n_off;
static uint8_t sig_loaded = 0xFF;
static int only_part = -1;

static void sound(void *ctx, const sn_event *e)
{
    (void)ctx;
    track_t *t = &trk[e->part % SN_PARTS];
    int n = e->part == SN_DRUMS ? sns_drum_note(&G.s, e->note) : e->note;
    if (n < 0)
        return;
    if (e->on) {
        trk_note_on(t, (uint32_t)n, e->vel);
        n_on++;
    }
    else { trk_note_off(t, (uint32_t)n); n_off++; }
}

/* a sound into a part's track, as sn_fw.c loads it */
static void host_own(track_t *t, const preset_t *pr)
{
    static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
    for (int i = 0; i < 8; i++) t->p[P_E0 + i] = pr->e[i];
    t->p[P_ATK] = pr->env[0];
    t->p[P_DEC] = pr->env[1];
    t->p[P_SUS] = pr->env[2];
    t->p[P_REL] = pr->env[3];
    t->p[P_ED_FLT] = pr->fenv;
    t->p[P_VOICE] = pr->mono ? V_LEGATO : V_POLY;
    for (int i = 0; i < 4; i++) t->p[P_DIST + i] = (int16_t)(pr->fx[i] ? pr->fx[i] - 1 : FX_DEF[i]);
}
static void host_fm6(track_t *t, const uint8_t *packed)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK;
    uint8_t v[FP_SIZE + 1u];
    fm6_unpack(packed, v);
    fm6_set_patch(tr, v);
    t->p[P_E7] = FM6_OWN;
    fm6_slot[tr] = FM6_OWN;
}
static void load_parts(void)
{
    for (int p = 0; p < 3; ++p) {
        const sns_sound *snd = sns_of(&G, p);
        host_preset(&trk[p], snd->engine, snd->preset);
        if (snd->own) host_own(&trk[p], snd->own);
        if (snd->fm6) host_fm6(&trk[p], snd->fm6);
    }
    host_preset(&trk[3], sns_kit_engine(G.s.snd[3]), 0);
    for (int p = 0; p < SN_PARTS; ++p) {
        sns_track(&trk[p], &G, p);
        if (only_part >= 0 && p != only_part) trk[p].p[P_LEVEL] = 0;
    }
    sns_song(&G.s);
    sig_loaded = G.sound_sig;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    int seconds = argc > 2 ? atoi(argv[2]) : 0;
    char path[512];
    host_tracks_init();
    sn_init(&G, (float)FS);
    if (getenv("STYLE")) sn_style_apply(&G.s, env_int("STYLE", 0));
    G.s.song = (uint16_t)env_int("SONG", 0);
    sn_song_sounds(&G.s);                         /* (the song's sounds from its style's pool) */
    G.s.form = (uint8_t)env_int("FORM", 1);
    if (getenv("KIT")) G.s.snd[3] = (uint8_t)env_int("KIT", 0);
    if (getenv("LIFT")) G.s.lift = (uint8_t)env_int("LIFT", 85);
    if (getenv("BUILD")) G.s.build = (uint8_t)env_int("BUILD", 0);
    if (getenv("ROOM")) G.s.room = (uint8_t)env_int("ROOM", 85);
    if (getenv("ENERGY")) G.s.energy = (uint8_t)env_int("ENERGY", 75);
    only_part = env_int("PART", -1);
    sn_settings_fix(&G.s);
    sn_rebuild(&G);
    sn_set_sound(&G, sound, 0);
    const char *name = getenv("NAME") ? getenv("NAME") : "groove";
    snprintf(path, sizeof path, "%s/snapper_%s.wav", dir, name);
    load_parts();
    FILE *w = fopen(path, "wb");
    if (!w) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    wav_hdr(w, 0);
    sn_play(&G, true);
    load_parts();
    int brk_at = env_int("BREAKIN", -1), drama_at = env_int("DRAMA", -1), keys_at = env_int("KEYS", -1);
    long f = 0, limit = (seconds > 0 ? (long)seconds : 40L) * FS, tail = -1;
    uint32_t busy_max = 0, clips = 0, kills0 = voice_kills;
    int32_t peak = 0;
    uint64_t mix_ns = 0;
    int last_bar = -1;
    while (1) {
        int32_t o[2 * CTL];
        sn_process(&G, CTL);
        if (G.sound_sig != sig_loaded) load_parts();
        sns_block(&G);
        if (G.bar != last_bar && G.step == 0) {
            last_bar = G.bar;
            if (G.bar == brk_at) sn_breakin(&G, true);
            if (G.bar == brk_at) sn_breakin(&G, false);
            if (G.bar == drama_at) sn_drama(&G);
            if (G.bar == keys_at) sn_key(&G, 13, 66, true);
            if (G.bar == keys_at + 1) sn_key(&G, 13, 66, false);
            if (getenv("TRACE"))
                fprintf(stderr, "bar %3d  %-6s  chord %-7s cut %3d  muted %d%d%d%d\n", G.bar, sn_sec_name(G.sec),
                        G.loop[sn_chord_now(&G)].name, G.cut[0], G.muted[0], G.muted[1], G.muted[2], G.muted[3]);
        }
        uint64_t t0 = now_ns();
        mix_block(o, CTL);
        mix_ns += now_ns() - t0;
        uint32_t b = busy_now();
        if (b > busy_max) busy_max = b;
        for (int i = 0; i < 2 * CTL; ++i) {
            int32_t v = o[i] < 0 ? -o[i] : o[i];
            if (v > peak) peak = v;
            if (v >= 32700) clips++;
        }
        for (int i = 0; i < CTL; ++i) wav_put(w, o[2 * i], o[2 * i + 1]);
        f += CTL;
        if (tail < 0 && f >= limit) { sn_play(&G, false); tail = 3L * FS; }
        if (tail >= 0 && (tail -= CTL) <= 0) break;
    }
    fseek(w, 0, SEEK_SET);
    wav_hdr(w, (uint32_t)f);
    fclose(w);
    printf("%s: %.1f s, %s, %ld on, %ld off, voices at most %u, %u stolen, peak %d (%.1f dBFS), %u clipped, mix %.0f ns/sample\n",
           path, (double)f / FS, sn_style_name(G.s.style), n_on, n_off, busy_max, voice_kills - kills0, peak,
           20.0 * log10((double)peak / 32768.0), clips, (double)mix_ns / (double)f);
    return 0;
}
