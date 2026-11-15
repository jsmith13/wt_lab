# Licensing

The experimental CPU1 worker, entry/stop and flash gate are adapted from
[felucca-bsp](https://github.com/hugelton/felucca-bsp), revision
`18eadadb8c364abd026accd2b1ea1c14ca347f06`, under MPL-2.0.
Their copyright/license headers are retained; `LICENSE-MPL-2.0` carries the license text.
Exception-frame restoration also follows the MPL-2.0 Melodee/BSP implementation.
These files retain their file-level MPL obligations within the GPL-3.0-only firmware.

WT Lab is derived from FM Lab and remains `GPL-3.0-only`. This tree retains the upstream code,
factory voices, copyright notices, and licenses listed below. The standalone wavetable oscillator
adapts MIT-licensed Mutable Instruments interpolation; its factory data combines original generated
tables and 56 CC0 AKWF single-cycle source WAVs, as detailed below.

The copied FM Lab logo and screenshots are inherited reference assets, not WT Lab branding or
screenshots of a completed wavetable synth. The logo is not used as WT Lab's README logo; distinct
branding will be created before release. The upstream licensing and attribution text follows.

FM Lab is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full text in
`LICENSE`). You may use, study, change and share it. If you give someone a firmware built from it, give them
its complete source under the same licence too.

One file carries a second licence as well: `firmware/src/dx7_core.c` is
`SPDX-License-Identifier: Apache-2.0 AND GPL-3.0-only`. Apache-2.0 is compatible with GPL-3.0, so the firmware
as a whole is GPL-3.0-only.

## Who made what

| | |
| --- | --- |
| The platform: `firmware/hal/`, `firmware/loader/`, `crt0.S`, `app.ld`, `firmware/src/` (`usb.c`, `midi_uart.c`, `ota.c`, `recovery.c`, `lcd.c`, `gfx.c`, `libc.c`, `bootguard.h`, `uac_fir.h`), `tools/` | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca)), with the changes of [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) |
| `firmware/src/platform.c`, the host mode `tests/host.c`, `docs/` | zvenson ([Dinghy](https://github.com/zvenson/dinghy)) |
| The FM engine `firmware/src/dx7_core.c` and its tables `dx7_tables.h` | see below; carried here from [dxsloop](https://github.com/zvenson/dxsloop) (zvenson) |
| The 20 factory voices, `tools/gen_voices.py` and the `firmware/src/dx7_voices.h` it generates | Copyright (C) 2026 Sven Trogus, designed for sloopDX ([dxsloop](https://github.com/zvenson/dxsloop) `tools/gen_dx7_bank.py`); FM Lab keeps the voices as they are and drops the drum kit |
| The archived instrument `firmware/src/fm_baseline.c`, `dx7_voice.h`, `tools/load_test.py`, `tools/docshots.py`, `assets/logo.svg`, the inherited test scripts and checks in `tests/`, inherited documentation | FM Lab |
| The native WT app `firmware/src/wtlab.c`, `wt_synth.c` / `.h`, voice DSP generation/data, and WT-specific tests/documentation | WT Lab, GPL-3.0-only; FM Lab's allocation/mono policies, UI conventions, and filter topology are acknowledged in the source |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| DX7 core: the msfa engine of Dexed, Copyright 2012 Google Inc., 2016-2025 Pascal Gauthier, ported to integer C by Sven Trogus (the port is GPL-3.0-only, Copyright (C) 2026 Sven Trogus); the tables are computed on the host from msfa's expressions | Apache-2.0 (msfa) AND GPL-3.0-only (the port) | `firmware/src/dx7_core.c`, `firmware/src/dx7_tables.h`; <https://github.com/asb2m10/dexed> |
| Terminus font 8x16 (ter-u16n), rendered into `firmware/gen/felucca_font.h` | SIL OFL 1.1 | `assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |
| Wavetable interpolation, Copyright 2012 Emilie Gillet, adapted from stmlib and Braids; WT Lab's extensions are GPL-3.0-only | MIT AND GPL-3.0-only | `firmware/src/wt_osc.c`, with the complete upstream MIT notice |
| 56 Adventure Kid waveforms by Kristoffer Karl Axel Ekstrand | CC0-1.0 | `assets/wavetables/akwf/`, with the original license and source/hash manifests |

## Wavetable engine and data

The C interpolation helpers in `wt_osc.c` adapt `Interpolate824` and `Crossfade` from
[stmlib/utils/dsp.h](https://github.com/pichenettes/stmlib/blob/d18def816c51d1da0c108236928b2bbd25c17481/utils/dsp.h),
revision `d18def816c51d1da0c108236928b2bbd25c17481`. Neighboring-frame playback follows
Braids' `DigitalOscillator::RenderWavetables` in
[braids/digital_oscillator.cc](https://github.com/pichenettes/eurorack/blob/f4cfa7697095a28d292bff758beb51deb49b5a86/braids/digital_oscillator.cc),
revision `f4cfa7697095a28d292bff758beb51deb49b5a86`.
No Braids factory wave data or hardware code is included.

The active WT firmware no longer includes the Dexed/DX7 core or FM factory patches. They remain in
the tree for the archived FM baseline. `wt_synth.c` uses the small attributed oscillator adaptation,
not Braids' full instrument architecture. `gen_wt_dsp.py` generates original mathematical filter/LFO
lookup data; no additional third-party engine or data collection is introduced by the voice layer.

WT Lab uses 16-bit source samples, wider intermediate products to avoid overflow, 44.1 kHz pitch
tables, seven harmonic-limited levels, finite control ramps, and queued table crossfades instead of
Braids' original renderer and its simple oversampling. These additions, the API, the generator, and
the tests are GPL-3.0-only. The upstream MIT permission notice remains in the adapted source.

`tools/gen_wavetables.py` is the reproducible source for `firmware/src/wt_data.h`. Seven tables are
original mathematical designs under GPL-3.0-only. `AKWF VOX` is derived from `AKWF_hvoice_0001.wav`
through `AKWF_hvoice_0008.wav` from
[AKWF-FREE](https://github.com/KristofferKarlAxelEkstrand/AKWF-FREE/tree/8de90bf94376670947369e69de0af6b9fbd19286/AKWF/AKWF_hvoice),
revision `8de90bf94376670947369e69de0af6b9fbd19286`, under that collection's CC0-1.0 dedication.
The unmodified WAVs, original license, revision, and SHA-256 checksums are retained in
`assets/wavetables/akwf/`. Generation verifies them without accessing the network.

The six appended tables each use eight cycles from the same pinned revision and CC0 dedication:
`STRINGS` from `AKWF_cello`, `REED` from `AKWF_clarinett`, `E-PIANO` from `AKWF_epiano`,
`PLUCK` from `AKWF_aguitar`, `CHIP` from `AKWF_oscchip`, and `GRIT` from `AKWF_distorted`.
Their unmodified sources and per-family manifests live in the `strings`, `reed`, `epiano`,
`pluck`, `chip`, and `grit` subdirectories. Each manifest records source hashes, revision,
source-family directory, selected frame order and curation metrics. The collection's pinned
`LICENSE.md` is byte-identical to the retained `LICENSE.txt`; all source families share it.

AKWF preparation removes DC, Fourier-resamples each cycle, aligns the fundamental's phase, and orders
the eight frames by spectral brightness. Full-band frames are RMS-matched, then one shared peak scale
per table is applied to all frames and harmonic levels. Lower levels are never independently
normalized. The result is a derived morphing table, not a byte-for-byte reproduction of the raw WAVs.

## Independent test reference

`tests/reference/wt_synth.c` and `wt_osc.c` retain the original 0.4.1 serial algorithms,
copied without code changes and verified against that retained build's SHA-256 fingerprints.
Text line endings are normalized to the repository's LF policy for publication.
They are not linked into firmware. Their original GPL-3.0-only and MIT/GPL notices remain.
Small header adapters share the current public types and expanded generated data so independent
CPU1 output/state/recovery tests run without an unpublished sibling directory.

## Voice data

**No Yamaha and no M-VAVE voice data is in this tree, and none should be added.** The factory voices are
Sven Trogus's, designed from scratch for sloopDX and written out in `tools/gen_voices.py`, which is the readable
source for `firmware/src/dx7_voices.h`; both are GPL-3.0-only like the rest of the code. `.syx` banks you import
stay on your device and are yours, not part of this repository.

This rule is inherited from dxsloop and is not a formality: a DX7 voice is 156 bytes of someone's authorship,
and the factory banks are Yamaha's.

## The logo

`assets/logo.svg` / `logo.png` are FM Lab's own: a modulator feeding a carrier, over a plot of
`sin(4t + 1.2 sin t)`. They are GPL-3.0-only like the rest of the tree.

Dinghy's logo carries the designburg signet, which is *not* under the GPL and marks Dinghy itself. It is not
in this repository, and must not be reintroduced. If you fork FM Lab, the same courtesy applies in reverse:
make your own.

## Trademarks

"DX7" is a trademark of Yamaha Corporation. FM Lab is an independent implementation of the DX7's synthesis (by
way of Dexed) and contains no Yamaha voice data, firmware or ROM content. It is not affiliated with, endorsed
by or supported by Yamaha.

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments; "M-VAVE" and "FM-1" belong to their
owners. FM Lab is not made or endorsed by any of them.
