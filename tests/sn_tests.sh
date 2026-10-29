#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Snapper's host tests: the generated files up to date (the splash, the FM6 patches), the groove engine strict about
# warnings and single precision and its own tests (tests/sn_groove_test.c), the screen and its layout lint
# (tests/sn_ui_test.sh).
#   sh tests/sn_tests.sh        (exit 0: all passed)
set -eu
mkdir -p build/host
python3 tools/sn_logo.py --check
python3 tools/sn_fm6.py --check
cc -std=c11 -O2 -Wall -Wextra -Wshadow -Wdouble-promotion -Werror -Ifirmware/src -c firmware/src/sn_groove.c \
   -o build/host/sn_groove.o
cc -std=c11 -O2 -Wall -Ifirmware/src -o build/host/sn_groove_test tests/sn_groove_test.c firmware/src/sn_groove.c -lm
build/host/sn_groove_test
sh tests/sn_ui_test.sh build/sn_ui
echo "sn_tests: all passed"
