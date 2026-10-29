# Contributing to Snapper

Snapper is a small fork. Bugs and ideas: [Issues](https://github.com/delirioux/snapper/issues) on this repository,
please; not Felucca's, Tuna's or Daurade's trackers: Snapper is maintained here.

With a bug, say the Snapper version (the splash), the style and song, the steps to reproduce it, what you expected
and what happened. A change to the songs goes in `firmware/src/sn_groove.c`, to the sounds in `sn_sounds.h` (run
`sh tests/sn_audition.sh` and keep the trims even). The keys only ever play music. `sh tests/sn_tests.sh` must pass,
and the screenshots in `docs/screens/` change with the screen.
