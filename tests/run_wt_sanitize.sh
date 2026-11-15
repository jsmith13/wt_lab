#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/.."
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:quarantine_size_mb=16}"
python3 tools/gen_wavetables.py --check
python3 tools/gen_wt_dsp.py --check
mkdir -p build/sanitize
flags="-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
cc -std=c99 $flags -Wall -Wextra -Werror -pedantic tests/flash_test.c -o build/sanitize/flash_test
build/sanitize/flash_test
cc -std=c99 $flags -Wall -Wextra -Werror -pedantic tests/wt_host.c -o build/sanitize/wt_host
cc -std=c99 $flags -Wall -Wextra -Werror -pedantic tests/wt_synth_host.c -o build/sanitize/wt_synth_host
cc -std=c11 $flags -Wall -Werror -Wno-unused-function -Ifirmware/src -Ifirmware/gen \
    tests/wt_app_host.c -o build/sanitize/wt_app_host
cc -std=c11 $flags -Wall -Werror -Wno-unused-function -DDINGHY_HOST -Ifirmware/src -Ifirmware/gen \
    firmware/src/wtlab.c -o build/sanitize/wtlab
build/sanitize/wt_host build/sanitize/wavetable
python3 tests/wtcheck.py build/sanitize/wavetable
build/sanitize/wt_synth_host build/sanitize/wt-voices.wav
build/sanitize/wt_app_host
for scenario in wt_instrument wt_ui wt_stress; do
    build/sanitize/wtlab "build/sanitize/$scenario" "tests/$scenario.txt" > "build/sanitize/$scenario.log"
done
rm -f build/sanitize/wt_workflow.flash.bin
build/sanitize/wtlab build/sanitize/wt_workflow tests/wt_workflow.txt 0 build/sanitize/wt_workflow.flash.bin > build/sanitize/wt_workflow.log
build/sanitize/wtlab build/sanitize/wt_reload tests/wt_reload.txt 0 build/sanitize/wt_workflow.flash.bin > build/sanitize/wt_reload.log
python3 tests/wt_synthcheck.py build/sanitize/wt-voices.wav build/sanitize/wt_instrument build/sanitize/wt_ui \
    build/sanitize/wt_workflow build/sanitize/wt_reload build/sanitize/wt_stress
