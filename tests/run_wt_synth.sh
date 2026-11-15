#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/.."
python3 tools/gen_wavetables.py --check
python3 tools/gen_wt_dsp.py --check
mkdir -p build/host
cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic tests/flash_test.c -o build/host/flash_test
build/host/flash_test
cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic tests/wt_synth_host.c -o build/host/wt_synth_host
cc -std=c11 -O2 -Wall -Werror -Wno-unused-function -Ifirmware/src -Ifirmware/gen \
    tests/wt_app_host.c -o build/host/wt_app_host
cc -std=c11 -O2 -Wall -Werror -Wno-unused-function -DDINGHY_HOST -Ifirmware/src -Ifirmware/gen \
    firmware/src/wtlab.c -o build/host/wtlab
build/host/wt_synth_host build/host/wt-voices.wav
build/host/wt_app_host
build/host/wtlab build/host/wt_instrument tests/wt_instrument.txt > build/host/wt_instrument.log
cat build/host/wt_instrument.log
build/host/wtlab build/host/wt_ui tests/wt_ui.txt > build/host/wt_ui.log
cat build/host/wt_ui.log
rm -f build/host/wt_workflow.flash.bin
build/host/wtlab build/host/wt_workflow tests/wt_workflow.txt 0 build/host/wt_workflow.flash.bin > build/host/wt_workflow.log
cat build/host/wt_workflow.log
build/host/wtlab build/host/wt_reload tests/wt_reload.txt 0 build/host/wt_workflow.flash.bin > build/host/wt_reload.log
cat build/host/wt_reload.log
build/host/wtlab build/host/wt_stress tests/wt_stress.txt > build/host/wt_stress.log
cat build/host/wt_stress.log
python3 tests/wt_synthcheck.py build/host/wt-voices.wav build/host/wt_instrument build/host/wt_ui build/host/wt_workflow build/host/wt_reload build/host/wt_stress
