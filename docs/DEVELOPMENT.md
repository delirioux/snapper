# Developing Snapper

The working notes behind Snapper: the agreements, how the work flows, why the code is shaped as it is, where it
stands and what comes next. What Snapper is and how to play it is in the [README](../README.md) and the
[manual](MANUAL.md).

Status: 29 October 2026. Version 0.1 on `dev`: the groove engine, the sounds, the gated kit and the screens, passing
the host tests; not yet run on an FM-1.

## Agreements

| What | Agreed |
| --- | --- |
| Home | GitHub, `github.com/delirioux/snapper`, branch `main`; unfinished work on `dev` until it has run on the FM-1. |
| Upstream | A fork of [Sill](https://github.com/delirioux/sill), Daurade, Flyfish and [Tuna](https://github.com/delirioux/tuna) (themselves forks of [Felucca](https://github.com/hugelton/Felucca)). Never push to them or to Felucca. Felucca's five engines are a submodule, `firmware/engines`. |
| Name | Ocean animals, as Tuna, Flyfish, Makohai, Daurade and Sill: the snapper. The logo: a red snapper with a spiny dorsal and sharp teeth on a ring of audio bars that ring out and stop dead, echo rings cut off by a straight line, a box with two 3D knobs, an 80s wordmark sliced by gate lines, snapper red and steel blue. |
| What it is | Generative 80s drum-heavy pop built on tension: a drone and a drum machine that build, the big gated drums breaking in on the downbeat, a drama and a key change. Its sounds and styles are its own; the repository names no artist, record or film as a model, in code, docs or history. |
| Description | "Snappy generative 80s drum-heavy pop for the FM-1, caught in 1985." |
| Language | English throughout: code, comments, commits, docs. |
| Licence | GPL-3.0-only. The drum one-shots are synthesized by `tools/sn_drums.py` and dedicated to the public domain (CC0). |
| Commits | Author `delirioux <340923392+delirioux@users.noreply.github.com>`. Work done with Claude ends with the `Co-Authored-By` and `Claude-Session` lines. |
| Repository contents | The firmware, its tests and tools, its docs and its plan (this file). Chat discussions stay out. Docs and screenshots change with the code. |
| Controls | The keys only ever play music; settings, pages and sets are on knobs and buttons. |
| Changes | A new style or song waits for the next bar line; so do a drama and the way back in. The break-in lands on a downbeat. The knobs act at once. |
| Colours | GATE is the default palette; Felucca's palettes stay on MENU. |

## How changes reach GitHub

Claude pushes to the repository directly (the Claude GitHub App). Work goes to `dev`; a push there builds a test
package (Actions > the run > Artifacts > `snapper-firmware`) and the test site under `dev/` (with `dev/try/`, the
browser build), installed with the installer's "Install a test build". It goes to `main` once it has run on the FM-1.

## Where Snapper lives

Snapper's files are `firmware/src/sn_*`, `tests/sn_*`, `tools/sn_*`, `web/sn_*` and `web/emu/sn_*`. From Tuna:
`tuna_libm.c` and `tuna_sys.h`. Everything else is Felucca's, touched only where Snapper hooks in (`felucca.c`,
`main.c`, `audio.c`, `panel.c`, `gfx.c`, `eng_sample.c`: `FELUCCA_SNAPPER`; `tools/build.py`: `SN_MODULES`;
`tools/gen_samples.py`: the drum set at 44.1 kHz, kept whole; `tools/gen_ui_palettes.py`: GATE).

| File | Does |
| --- | --- |
| `sn_groove.h/.c` | the engine: ten styles, twelve chord loops, the rhythm families of each part (the pads, the bass lines, the full kit's grooves, the drum machine's patterns), the riff, the form of tension, the tom fills, the lift, BREAK-IN, DRAMA, the keys, swing and human timing. Plain C, no Felucca: it runs on the host as it does on the FM-1 |
| `sn_sounds.h` | the forty-eight sounds on Felucca's engines and their trims, the two kits and their notes, and how a part's cards set its track (level, pan, sends and ROOM, the matrix's lift slot, TONE) |
| `sn_fm6.inc` | the FM6 patches (DRONE, GLASS, PUNCH and ORCH HIT, Snapper's and Sill's; SLAP, FRETLESS (SLIDE), Tuna's), made by `tools/sn_fm6.py` |
| `tools/sn_drums.py` | the thirteen drum one-shots, synthesized, seeded, the gated room in the snare, the clap and the toms, into `assets/samples-cc0/DRUMS/` |
| `tools/sn_piano.py` | the PIANO: six notes of VCSL's Kawai grand (CC0), into `assets/samples-cc0/PIANO/` |
| `sn_ui.h/.c` | the twelve pages, the cards, the tension meter, the form strip, the styles list, the sets, on Felucca's `gfx.c` |
| `sn_fw.c` | the glue: the groove on the tracks and MIDI out, loading a part's sound when it changes, the panel, the LEDs, the settings and the sets in flash, the splash |
| `sn_logo.inc` | the splash logo, made by `tools/sn_logo.py` from the same drawing as `assets/logo.svg` (`tools/draw_logo.py`) |
| `tests/sn_groove_test.c` | the engine: every style plays and ends its notes, in key; the keys wait for the full band, the big kit for the break-in, the machine makes way; crashes on downbeats; the form's order and lengths and its key up; DRAMA OFF; BUILD HOLD; REC tapped early and late, held, in the full band and in a drama; SEQ; FORM off; a song replays exactly; a style on the bar line; the keys; a bad record |
| `tests/sn_ui_test.c` | the screen: the controls, every page in every palette, the layout lint, PNGs and a GIF (`docs/screens/`) |
| `tests/sn_felucca.c` | a song through Felucca's real engines to a WAV: voices, steals, peaks, the cost |
| `tests/sn_audition.c` | every sound's loudness, for the trims in `sn_sounds.h` |
| `web/emu/sn_web.c` | the browser build, on Felucca's emulator |

## Decisions and why

- **Sill's machine, a new engine.** The frame (the four tracks, the matrix slot, the cards, the sets, the flash,
  the browser build) is Sill's, proven on the FM-1. The groove engine, the styles, the sounds, the pages and the
  drums are Snapper's. Sill's choir, its VOX page and its pump are gone.
- **Tension is the song.** A song starts small and dark and the payoff is the moment the big drums hit. HUSH (the
  drone, the machine) and RISE (the bass, the filter opening) last BUILD bars between them; the break-in is one bar,
  the machine to its half and then the toms; the full band starts its chord loop on that downbeat with a crash. The
  break-in and the drama hold the loop's last chord, so the full band's first chord resolves it.
- **The break-in lands on beat one.** REC never starts a fill mid-bar: before the bar's half it starts there, after
  it the next bar's half. Held in the hush or the rise, it freezes the form in the rise's last bar (the tension
  meter creeps but never arrives) until it is let go. BUILD HOLD leaves the rise open until REC.
- **The drama is a second break-in.** Four bars of one held chord, the drums out and then a low tom rolling in, a
  whole bar of toms, and the full band KEY UP higher (each drama adds KEY UP: +2 by default, the classic lift).
  DRAMA AUTO puts DRAMAS of them in the form, OFF leaves them to SEQ. After the last, END comes down to the drone and
  the song goes round again from the hush, home, with a new riff.
- **The gated room is in the sample.** A gated reverb is a dense room held flat and cut dead; running one on the
  FM-1 would cost a reverb's buffers and cycles on every hit. The snare, the clap and the toms carry theirs: a
  synthetic room (a burst of filtered noise decaying slowly, convolved with the dry hit), mixed in under the hit,
  held for 0.30 s (the snare), 0.24 s (the clap) or 0.36 s (the toms) and shut in 12 ms, squashed as a desk would.
  The price: the gate's length is fixed. ROOM is the drums' send into Felucca's own room on top.
- **The drum machine is on the same kit.** Eight voices and four tracks leave no track for a second drum engine, so
  the box's small kick, rim and hat are three more one-shots on the BIG kit (keys 63 to 65). The hush, the rise, the
  break-in's first half and the end play them; the full band plays the big ones (RHYTHMBOX keeps the box under its
  big snare). The BOX kit plays all of it on Felucca's DRUM engine, its 80 kit, General MIDI notes.
- **FM where FM is good.** The electric piano, the brass section, the wood bars and the bell are FM6's own patches;
  the DRONE (three detuned pairs swelling in over 1.5 s or so, a sub an octave down, a slow drift), the GLASS pad
  and the PUNCH bass are Snapper's. Sixteen sounds a part, four a style, one a song, as Sill.
- **The pads' rhythm by style.** HELD (the triad held as long as the chord lasts), PULSE (eighths, the triad's top
  two), STAB (a horn section's syncopated hits), COMP (a ballad's piano hand, the full voicing). In the hush, the
  rise, the drama and the end the pads always hold: that is the drone.
- **The riff waits.** KEYS play only in the full band (and live, always). SPARSE styles sing only the phrase's answers;
  NIGHTDRIVE arpeggiates the chord.
- **The lift follows the tension** (in the matrix, as Sill: MODW to the lifted parts' brightness, -32, every 32
  frames): about a quarter open in the hush, opening through the rise with the tension, open in the full band,
  half open and rising in the drama, closing in the end.
- **Eight voices for everything.** The held chords in the full band play the triad, the pulse two notes; every drum
  has a ring length. Measured over 60 s through Felucca's engines (BUILD 8): STADIUM steals 20, TOMTOM 71,
  NIGHTDRIVE 160, BRASSHIT 262 (the brass stabs' and the riff's tails), out of 360 to 1,200 notes.
- **No libm, no doubles**: floats on the FPU (`-mcpu=r3 -mfprev1`), `sinf` and friends from `tuna_libm.c`.
- **Settings save themselves while stopped**, in Felucca's first project object ("SNP1"); the eight sets in the
  second ("SET1"). Other firmwares' records there are not read. A bad record is fixed field by field.
- **GATE**: a dark red room, steel-blue values, snapper red for the one active thing; its own colour tag (1280), so
  another firmware's choice does not carry over.

## Measured on the host

60 s of each style (BUILD 8) through Felucca's engines (`tests/sn_felucca.sh`): peaks between -4.8 and -3.8 dBFS,
nothing clipped, the eight voices full at the busiest moments. The mix costs 100 to 180 ns a sample on this host;
the real number comes from the FM-1.

## Plan

1. **On the FM-1.** It installs, boots and plays: the splash, the hush, the break-in on the downbeat, a drama, the
   key up, the end and round again. Then the settings and sets surviving a power cycle, MIDI out, the CPU in the
   busiest styles over a long session.
2. **The sound by ear.** The gated snare and toms (the gate's length, the room's level under the hit), the drum
   machine against the drone, the DRONE, GLASS and PUNCH patches, the trims of the new sounds
   (`tests/sn_audition.sh`), the drama's held chord.
3. **The feel.** BUILD's lengths, how fast the tension meter climbs, the fills, the voice steals in BRASSHIT.
4. **Release 1.0** on `main`, with the installer, once it has run on the FM-1.
