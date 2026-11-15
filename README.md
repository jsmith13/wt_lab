# WT Lab

**Wavetable firmware for the M-VAVE FM-1**, with the familiar controls and workflow of
[FM Lab](https://github.com/jsmith13/fm_lab).

[Download WT Lab 1.0.0](https://github.com/jsmith13/wt_lab/releases/tag/v1.0.0).
Device identity: **FM-1_922**. WT Lab is independent of M-VAVE and Yamaha.

## Instrument

- Eight voices, two wavetable oscillators per voice, interpolated position and table crossfades.
- Fourteen tables, eight morphing frames each, with seven pitch-dependent harmonic limits.
- Amp and motion/filter envelopes, two LFOs, per-voice filters and live pitch/position modulation.
- Chorus, phaser, delay and reverb; arpeggiator and phrase recording/playback.
- Eight factory presets and 256 user slots, A/B comparison, mute/solo and native saving.
- USB audio/MIDI, sustain, expression, pitch bend, channel pressure and sixteen MIDI assignments with Learn.
- Familiar four-knob editors, spectrum/output views, and four normal GLOBAL pages.
- Guarded CPU1 rendering, settled-control caching and aligned voice copies; no voltage or clock changes.

See the [quick reference](docs/QUICK-REFERENCE.md) and [MIDI controls](docs/MIDI.md).

## Tables

The original eight tables retain their exact sample data and numbering:
SINE-SAW, ODD, PULSE, FORMANT, ORGAN, GLASS, HOLLOW and AKWF VOX.

| Addition | Character | CC0 source family |
| --- | --- | --- |
| STRINGS | Rounded-to-raspy bowed tone | AKWF cello |
| REED | Mellow-to-woody reed tone | AKWF clarinett |
| E-PIANO | Rounded-to-bright electric-key tone | AKWF epiano |
| PLUCK | Warm-to-wiry plucked-string tone | AKWF aguitar |
| CHIP | Simple-to-complex chip-oscillator tone | AKWF oscchip |
| GRIT | Rounded-to-abrasive distorted tone | AKWF distorted |

These are oscillator timbres, not realistic sampled instruments. Imported cycles are phase-aligned,
DC-removed, Fourier-resampled and brightness-ordered. Shared peak scaling preserves gain between
harmonic levels. The 402,976 sample bytes reside in flash, not the audio-buffer RAM pool.
Pinned unmodified sources, licenses, hashes and selection metrics are retained in
`assets/wavetables/akwf`. See [LICENSING.md](LICENSING.md).

## Install

Download `wtlab-1.0.0.fwsc` and the matching source ZIP from the release.
Mute the output, use stable power/USB, and install only on the intended WT Lab unit.
The Python installer requires `mido` and `python-rtmidi`:

```powershell
python -m pip install mido python-rtmidi
python tools\fm1_install.py --info --port "Felucca 0"
python tools\fm1_install.py wtlab-1.0.0.fwsc --port "Felucca 0"
```

Confirm the running identity is **FM-1_922** before updating. The installer asks for confirmation;
do not use `--force` to bypass an identity mismatch. After restart, check GLOBAL's BUILD field.
Product identity alone does not establish the displayed release version.

**Saved-data precautions:** cross-flashing FM Lab, WT Lab or stock firmware may overwrite native
sounds/settings. Firmware rollback does not restore those data. Older WT builds cannot read table
IDs 8..13 and may reject their entire 32-patch bank. Preserve needed data before rollback.
Absolute MIDI table-selection values now span fourteen tables; old automation may select different IDs.

## Hardware status

The user installed `0.5.0-waves`, verified restart/USB audio/MIDI, and confirmed correct playing and
satisfactory new tables before approving publication as 1.0.0. Firmware code and tables are unchanged;
1.0.0 is a version-labelled rebuild, not the exact package installed during that acceptance.
Repository text is normalized to LF for portable builds and consistent source fingerprints.
**The published package was not re-flashed during publication.**

Earlier matched-workload measurements on the same optimized engine reported PEAK 60%, or 57%
after resetting while holding notes, with LATE 0. Those are workload-specific 0.4.8 observations,
not new 1.0.0 measurements or a universal performance guarantee.

The worker uses private staged state, startup probes, full-budget admission, bounded waits and
flash/reset interlocks. Rejected jobs are discarded and CPU0 rerenders their original voices.
Failures still notify `CPU1 FAILED / CPU0 FALLBACK`. The 600 us request limit and 1250 us recovery
reserve remain provisional; retirement/recovery cannot guarantee glitch-free audio.
Long-term, worst-case and exhaustive hardware qualification remain incomplete.
The [hardware guide](docs/HARDWARE-TESTS.md) retains historical measurements and remaining checks.

## Build

Use Linux x86-64 or WSL, Python 3 with Pillow, the JieLi toolchain and the AC79 SDK.
`build.sh` defaults to `~/.jieli/toolchain` and `~/fw-AC79_AIoT_SDK`; override with
`JIELI_TOOLCHAIN` and `AC79_SDK`. Toolchain setup is in `tools/get_toolchain.sh`.

```sh
sh build.sh --cpu1 --cpu1-job-samples 64 --product FM-1_922 --release 1.0.0
```

Add `--check-only` to validate without creating an installable package.
CPU1 remains opt-in for source builds; the published package uses the explicit flags above.
Omitting `--cpu1` builds the serial variant. Do not change voltage/PLL settings.

The 64-sample path batches only settled manual voices. Controller ramps/publications and
arp/sequence retain 32-sample jobs; envelope, LFO, controls, effects and performance boundaries
remain at 32 samples. The cache reuses only settled audio-owned control state. Guarded recovery
and the generic memory-copy implementation remain unchanged.

## Tests and source

```sh
sh tests/run_wt.sh
sh tests/run_wt_sanitize.sh
```

Windows oscillator checks are available through `tests\run_wt.ps1`.
Generators reproduce offline with `python tools/gen_wavetables.py --check` and
`python tools/gen_wt_dsp.py --check`.

The independent CPU1 oracle is self-contained: `tests/reference` retains the original serial
algorithms and their license headers, sharing the expanded table data/types, not the worker.
Compile `tests/cpu1_host.c` and `tests/cpu1_reference.c` with C11, `-Ifirmware/src -pthread`,
ASan/UBSan or TSan, and `-DWT_CPU1_JOB_SAMPLES=32` or `64`; do not define `WT_CPU1` for this oracle.
Application tests may use `WT_PROFILE_TEST`, `WT_CPU1=1` and the selected job size.
Host timing is explicitly NOT DEVICE and does not measure hardware performance.

`tests/wt_new_tables.txt` exercises all six additions through the real app's table/position controls.
The oscillator tests generate a two-second scan of each table in `build/host/wavetable-demo.wav`.

The release source archive includes the complete firmware, generator inputs, tests and licenses.
WT Lab is GPL-3.0-only with retained compatible MIT, MPL-2.0, font and CC0 obligations.
The archived FM baseline remains for development reference and is not the active instrument.
