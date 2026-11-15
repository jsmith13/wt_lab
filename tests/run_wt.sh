#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/.."
python3 tools/gen_wavetables.py --check
mkdir -p build/host
cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic tests/wt_host.c -o build/host/wt_host
build/host/wt_host build/host/wavetable
python3 tests/wtcheck.py build/host/wavetable
