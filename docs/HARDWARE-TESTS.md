# WT Lab hardware validation guide

## 1.0.0 publication status

The user confirmed correct playing and satisfactory new tables on installed `0.5.0-waves`
and approved publishing its unchanged firmware source as 1.0.0 on 2026-11-15.
The installed prototype passed independent restart and USB audio/MIDI checks.
The published 1.0.0 is a version-labelled rebuild and was not re-flashed during publication.
Exhaustive sustained-load, worst-case recovery and electrical qualification remain incomplete.
The development baseline/installation entries below are historical, not current release status.

## Historical development measurements

**Last known-working baseline: `0.3.2-test` / `FM-1_922`, designated by the user on 2026-11-14.**
Retain `build/wtlab-0.3.2-test.fwsc` and its matching `.build.json` / `.device.json` records as the
WT rollback reference; do not overwrite these versioned artifacts with later builds.
Package SHA-256: `bba35044fd66c6e5b2205040584fe1adac1cb1ddd7a5efc5433a62e6750a3944`.
The user reported "This looks good, thanks!" after installation. Package, source/test archive,
restart and independent USB audio/MIDI checks pass. This designation is not full hardware
acceptance or public-release approval; outstanding checks below remain outstanding.
The matching target/test sources are archived in `build/wtlab-0.3.2-test.source.zip`;
their hashes were verified against the build/validation records.
The former `0.2.7-test` and `0.1.8-test` baseline packages, reports and source archives remain intact.
The 0.1.8 F7 peak 68%, F5 peak 82%, both zero LATE are historical results, not CPU measurements for 0.3.2.

Software/target builds do not approve hardware timing, USB reliability, listening quality or recovery.

**Current test installation: `0.3.2-test` / `FM-1_922`, installed 2026-11-14.**
The user requested the swapped CC navigation controls/lowered bars and freshly confirmed the connected WT unit,
muted/low MASTER and stable USB/power. Package/app/loader CRCs, 62 target source hashes and matching
source ZIP were verified; installation completed without force. An independent identity query reports
`FM-1_922 [running]`, and USB MEDIA/MidiEndpoint/AudioEndpoint nodes have OK status.
Versioned `.build.json`, `.check.json`, `.device.json`, validation reports and `.source.zip` are retained.
The identity is shared with older WT builds and does not itself prove the displayed version;
on-screen BUILD, MIDI/controller listening and CPU/load checks remain pending.
`0.3.2-test` is now designated last known-working; the `0.2.7-test` rollback remains intact.
WTLABDEV check-only binaries remain non-installable; do not feed them to an installer.

## UI alignment installed: 2026-11-11

`build/wtlab-0.1.9-test.check.json` records the successful real-target **check-only** build of the
FM Lab visual alignment pass. Eleven corresponding actual-emulator views were inspected before/after;
comparison PNGs and source/validation provenance are in `build/ui-comparison`.

The user subsequently requested build/flash and confirmed muted/low MASTER with stable power.
The `FM-1_922` package's app/loader exactly match the tested check build; current source hashes and
all loader/package CRCs were verified. The updater completed successfully on `Felucca 0`, and an
independent query confirmed running `FM-1_922` with healthy USB audio/MIDI endpoints.
The user confirmed BUILD `0.1.9-test` and normal startup with the revised UI visible.
Versioned package/build/device records are retained in `build`.
Package SHA-256: `3475d8b6a68913fbcdaff3b3456d17fc29370ce78dc18c11a1e26ff97a4b4172`.

MSVC/GCC/ASan/UBSan pass, including focused layout/transition tests. All 44 new screens agree
across toolchains and all 15 original audio/flash artifacts remain baseline-identical.
Only `wt_ui.c` differs among target build inputs. Image 354,052 bytes, RAM 76,472, pool 288,760,
RAM-text 6,716; the RAM kernels remain call-free.

These are not new device CPU results. Before accepting this UI candidate, check
screen legibility, corner battery/charging indication, page/SAVE transitions and knob feedback,
then repeat the F7/F5 eight-note CPU/peak/LATE tests while navigating and editing.
The former `0.1.8-test` baseline is retained as an older rollback; the current user-designated
last known-working version is `0.2.7-test`.

### UI behavior follow-up: 0.2.0-test installed

The user reported the charging color/animation mismatch, partial patch-line flicker, knob feedback
below the hints, and FX shortcut/formatting differences in `0.1.9-test`, and requested dual-oscillator
HOME. The candidate corrects these without changing DSP or storage formats. The flicker was traced
to the title's 32-pixel canvas ending at y50 while the patch line begins at y44; the title is now
clipped above that line. A transfer-level regression verifies steady redraws paint the patch line once.

Gray charging bars fill cumulatively every 600 ms with FM Lab's battery geometry and thresholds.
HOME knob feedback occupies the patch line for 1.5 seconds. Both oscillator previews are visible
on default HOME; short FX maps the four mixes, while held FX retains detailed effect banks and
the requested labels/units. `build/ui-parity` contains twelve FM/installed/revised emulator views.

MSVC/GCC/ASan/UBSan pass with 59 matching artifacts. Retimed detailed-FX workflow inputs produce
byte-identical audio/flash with the archived `0.1.9-test` source; other original audio is unchanged.
`build/wtlab-0.2.0-test.check.json`: image 354,576 bytes, RAM 76,472, pool 288,760,
RAM-text 6,716; audio kernels remain call-free. The matching package was subsequently installed
with explicit muted/low-MASTER/stable-power approval; package/source/loader integrity, updater
completion, independent `FM-1_922` identity and USB audio/MIDI enumeration all passed.
The user reports "Things are as expected"; exact on-screen BUILD was not separately reported.
The versioned device record and verified source ZIP are retained alongside the older rollback files.

After an approved later flash, physically recheck charging/discharging, sustained patch-line
legibility, knob feedback/expiry, both oscillator panels, FX tap/hold/mix mappings, and F7/F5
load/LATE while navigating. These hardware checks are not established by emulator tests.

### HOME/filter/scope follow-up: 0.2.1-test not installed

Short FX retains each HOME plot/title, and detailed FX values have no space before their units.
Filter HOME uses the FM-style response curve/note/harmonic overlays, WT-native cutoff/Q data,
wrapping TYPE, compact Hz/kHz and `RESO` feedback. Scope uses played-note pitch for its trigger/window
and shows note/duration; pixel comparisons confirm its fill already matches FM Lab.
WT ENV/LFO visuals are unchanged. The fresh span remains 0-5K; explicitly saved span values survive
updates, so a device currently showing 0-20K must be set to 5K once using SPAN.

User-selected compatibility policy: retain old cutoff frequencies and extend the upper range.
All 129 old frequency knots, 6,192 coefficient values and 16 resonance values remain exact.
Five appended knots reach 20 kHz, with unchanged modulation scale and a higher modulation ceiling.
**An extended base cutoff saved in a bank causes older WT firmware to reject the whole 32-slot
bank**, not just that patch. Back up before rollback and do not save over the rejected bank.
This does not change the separate FM/WT data-overlap risk.

MSVC/GCC/ASan/UBSan pass with 59 matching artifacts and all 15 existing audio/flash artifacts
unchanged from 0.2.0. Focused tests cover FX plot retention, unit formatting, wraparound, response
shape/Q, note/ruler overlays, actual scope duration, extended cutoff bounds, high-resonance
transitions and storage round-trip. Thirteen paired frames are in `build/ui-home-parity`.
`build/wtlab-0.2.1-test.check.json`: image 357,456 bytes, RAM 76,472, pool 288,760, RAM-text 6,716;
all 2,247 RAM-text instructions remain call-free. No installable package or device flash.
After a separately approved flash, recheck all three HOME views with FX, filter display and
20 kHz/high-resonance operation, saved-span behavior, and F7/F5 app CPU/peak/LATE during navigation.

### Paired HOME controls: 0.2.2-test installed

The user approved replacing permanent frame/blend text with position bars and configured-level bars,
without permanent numeric levels. Each position bar has eight frame ticks, a dim base marker and a
bright live marker; muted level bars retain their length but dim. Exact frame/blend values are shown
above full EDIT's rows. Plain wavetable/scope HOME has LVL1/POS1/LVL2/POS2, with DET1/PENV1/DET2/PENV2
on SELECT. SEL-held DETUNE uses the existing 1/256-semitone precision; its combined range remains
+/-25 semitones using the unchanged semitone/fine patch fields. PENV is position-envelope depth.

Short EDIT retains the selected HOME plot and current knob bank while enabling OP1/OP2,
MONO/POLY and high-F-sharp A/B. The title marks EDIT state and selected oscillator. HOME exits the
state without switching views. Algorithm/table controls wrap cyclically, including SINE-SAW/AKWF VOX.
ENV/LFO and filter/FX controls remain unchanged; only `wt_ui.c` differs among target build inputs.

MSVC/GCC/ASan/UBSan pass with 59 existing and 17 new scene artifacts matching exactly. All 15
existing audio/flash artifacts remain unchanged; the stress fixture uses POS2 for its original
oscillator-2 position edit. Fifteen inspected emulator frames are in `build/ui-home-controls`.
Target check-only passes: image 358,024 bytes, RAM 76,472, pool 288,760, RAM-text 6,716 with
2,247 call-free instructions. The user subsequently requested flash and confirmed muted/low MASTER
with stable USB/power. The packaged app/loader and every source hash match the tested check build;
loader marker and header/file-list/all-file CRCs pass independent checks.
Installed package SHA256: `fb4e5bfeade8af2661f01296560bf42df985577a1c65a8ad5542a5faeab6e185`.
The updater completed its write/restart; an independent query reports running `FM-1_922`.
Windows Felucca MEDIA, MidiEndpoint and AudioEndpoint enumerate with OK status.
Versioned build/device records and a verified source ZIP are retained. The user confirmed GLO BUILD
`0.2.2-test` and normal HOME with the oscillator position/level bars.
Further hardware UI/load checks remain pending; no new hardware CPU/listening claim. Prior 0.2.0 and designated
known-working 0.1.8 packages remain unchanged.

Physically verify bar readability, base/live separation, muted-level visibility,
both knob banks, fine DETUNE without an accidental SEL mute, table wrap, and edit-state key
functions on all three HOME views. Recheck F7/F5 app CPU/peak/LATE during navigation.

### Compact headings and control-only ENV: 0.2.3-test installed

Wavetable names replace OSC 1/OSC 2 headings, retaining selected-side accent and OFF/SILENT
indications. Short ENV keeps the current wavetable/filter/scope HOME plot and spectrum peaks;
its four knobs start at AMP ADSR, with SELECT/repeated ENV switching to motion/filter ADSR.
Long ENV retains its graph, parameter cells and routing bank.

The MSVC instrument regression suite and GCC ASan/UBSan application checks pass, including
all-table heading/status checks and all-view ENV routing, preservation and full-editor transitions.
Twelve regenerated audio/flash artifacts remain byte-identical to 0.2.2-test; only `wt_ui.c`
changed among target inputs. Emulator comparisons are in `build/ui-env-home`.
Target: image 357,988 bytes, RAM 76,472, pool 288,760, RAM-text 6,716; 2,247 call-free instructions.

The user requested flash and freshly confirmed muted/low MASTER with stable USB/power.
The packaged app/loader/source match the tested check build and all independent package CRCs pass.
Installed package SHA256: `52e2183e5b91d5a8ee083e444f55868764fdca085ed7090f580fd772447bb912`.
The updater completed write/restart; independent identity reports running `FM-1_922`.
Felucca USB audio/MIDI endpoints enumerate with OK status. Versioned build/validation/device
records and a verified matching source ZIP are retained. The user confirmed GLO BUILD `0.2.3-test`,
normal HOME with table-name headings and short ENV preserving HOME while SELECT switches AMP/motion
ADSR. Further UI/listening/CPU/load checks are not established by this flash.
Prior 0.2.2 and designated known-working 0.1.8 packages remain unchanged.

### Output quality and FM parity: 0.2.4-test installed

Short FX is titled FX MIX. HOME EDIT is EDIT WT 1/2; selected OP1/OP2 and MONO/POLY keys light.
Long-menu rows compose labels, values and bars before one LCD transfer instead of briefly clearing
the right-hand values. GLO page 3 is VELOCITY / SAMPLE RATE / BIT DEPTH / OUTPUT.
Clockwise turns increase quality; 24-bit and 44.1K bars are full, 8-bit and 8K empty.
Lower rates use sample-and-hold before FX without changing native synth/filter/envelope/effects
timing. Rates are 8/11.025/16/22.05/32/44.1 kHz; no 88.2K option.

MSVC instrument regressions and GCC ASan/UBSan application/scene checks pass.
All 18 new scene artifacts match between toolchains; twelve regenerated native-rate audio/flash
artifacts remain byte-identical. Tests cover actual LED drives, title/row transfers, rate/bit
direction and bounds, fractional clocks, voice-output holding and settings/64-note phrase restore.
Inspected frames are in `build/ui-quality-parity`. Target: image 358,652 bytes, RAM 76,488,
pool 288,760, RAM-text 6,716; 2,247 call-free instructions.
Packaged and installed with fresh muted-output/stable-power approval, without force.
Packaged source/app/loader match the tested check build; independent package CRC, running identity
and Windows USB audio/MIDI enumeration pass. Versioned build/device records and a verified source
ZIP are retained. The user confirmed BUILD `0.2.4-test`, normal HOME and reported that it looks good.
Exhaustive physical UI/quality, listening and CPU/load checks remain pending.
The previous 0.2.3 package and designated known-working 0.1.8 rollback are unchanged.

Reduced-rate settings require schema 2. Before older-firmware rollback, restore 44.1K and wait
for quiet autosave (CFG* clears), or retain a backup of preferences/phrase. Native-rate saves use
the original schema 1; sound banks and existing bit-depth encodings are unchanged.
For detailed acceptance, check actual OP/mode LEDs, steady right-hand values across
long editors, FX/edit titles on all HOME views, all quality steps and lower-rate aliasing/tails.
Recheck F7/F5 app CPU/peak/LATE at native and reduced rates during navigation; software checks
do not establish hardware headroom.

### Control-only HOME LFO: 0.2.5-test not installed

Short LFO keeps the current wavetable/filter/scope plot and selects RATE / SHAPE / DEPTH / DEST
for the last selected LFO. SELECT or another short tap switches LFO 1/2; the title identifies it.
HOME leaves the shortcut without cycling the view. Long LFO retains its waveform/key-sync editor.

MSVC instrument regressions and strict GCC ASan/UBSan application/scene checks pass.
Focused tests cover every HOME view, both targets, exact routing, fine edits and full-editor
transitions. All 18 new scene artifacts match between toolchains; twelve regenerated native-rate
audio/flash artifacts remain unchanged. Inspected frames are in `build/ui-lfo-home/lfo-home.png`.
Target: image 358,540 bytes, RAM 76,488, pool 288,760, RAM-text 6,716; 2,247 call-free instructions.
Only wt_ui.c changed among target inputs. No package/flash; 0.2.4 remains installed.
After a separately approved flash, check all three HOME views during LFO target/knob changes,
audible modulation, HOME exit and the long-press graph/key-sync editor. Physical CPU/load
acceptance remains separate from these software checks.

### FM-style ARP/SEQ HOME: 0.2.6-test installed

ARP and SEQ borrow the knobs without replacing any HOME plot. ARP titles/legends match FM:
ARP 1/2 (TEMPO/PATTERN/OCTAVE/LATCH), ARP 2/2 (GATE/RATE/SWING/VELO).
SELECT changes the independently remembered bank; another ARP press toggles it.
SEQ uses the fixed SEQ title and TEMPO/BARS/DIV/CLICK controls, including during PLAY/REC;
its context light is on in the shortcut or full step editor. Long SEQ retains step editing.
This version also includes the LFO HOME change.

MSVC instrument regressions and strict GCC ASan/UBSan application/scene checks pass.
Tests cover all HOME plots/peaks, FM labels/titles, exact control edits, ARP memory/toggles,
fixed SEQ controls/LED, hold boundaries and playback/overdub/stop preservation.
All 18 scene artifacts match between toolchains; twelve historical native-rate audio/flash
artifacts are unchanged. Inspected frames: `build/ui-performance-home/performance-home.png`.
Target: image 358,556 bytes, RAM 76,488, pool 288,760, RAM-text 6,716; 2,247 call-free instructions.
Only wt_ui.c changed among target inputs since 0.2.5-test.
Packaged and installed with fresh muted-output/stable-power approval, without force.
Source/app/loader match the tested check build; independent package CRC, running identity
and Windows USB audio/MIDI enumeration pass. Versioned build/device records and verified
matching source ZIP are retained. The user reported that it looks good; exact BUILD and
exhaustive shortcut/listening/CPU confirmation remain unconfirmed.
Previous 0.2.4 and designated working 0.1.8 rollback packages are unchanged.
Confirm GLO BUILD `0.2.6-test`, normal HOME and LFO/ARP/SEQ on all HOME views, both ARP banks/toggle states,
SEQ light, PLAY/REC navigation and long SEQ step editing. CPU/load acceptance remains separate.

### Octave and PLAY button lights: 0.2.7-test installed

OCT-/OCT+ light steadily at one octave and pulse 600 ms bright / 300 ms dim at two;
zero clears both layers. PLAY glows for an available phrase and flashes on each
sample-clocked beat during playback/count-in. Stopping leaves the phrase glow on.
REC lighting and native octave/note/transport behavior are unchanged.

MSVC application regressions and strict GCC ASan/UBSan application checks pass,
including both octave directions/phases/clamping/navigation and PLAY availability,
clock acknowledgement, 120/200 BPM pulses, stop/clear and count-in.
Target: image 359,048 bytes, RAM 76,504, pool 288,760, RAM-text 6,716;
2,247 call-free instructions. Only wt_ui.c changed among target inputs.
Packaged and installed with fresh muted-output/stable-power approval, without force.
Source/app/loader match the tested check build; independent package CRC, running identity
and Windows USB audio/MIDI enumeration pass. Versioned build/validation/device reports and
verified matching source ZIP are retained. The user reported "Everything looks good!" and
designated this version as last known working on 2026-11-13. Previous 0.2.6 and former
working baseline 0.1.8 artifacts remain unchanged.
For detailed acceptance, check GLO BUILD `0.2.7-test`, normal HOME, both octave limits and bright/dim cadence,
PLAY phrase glow while stopped, beat flashes at different tempos and count-in.
Physical LED appearance and CPU/load acceptance remain pending.

## First local device test: 2026-11-10

The user approved the connected FM-1, existing-data loss risk, low MASTER/muted outputs and
`FM-1_922` as a **local test identity**, not a public reservation. The Firmware Hub catalogue
at <https://thefirmwarehub.com> did not list that exact product identity when checked.

Installed `build/wtlab-0.1.0-test.fwsc` over USB-MIDI from the running `FM-1_970`. The updater
completed its write/reboot and an independent identity query confirmed `FM-1_922`. Windows
enumerated normal Felucca audio/MIDI endpoints. The user confirmed the normal WT LAB wavetable
screen and clean plain-sine audio from a local key, with normal release.

The preflight also carried over FM Lab's supported Puya/UCUN flash-chip policy and RAM-only
timeout reset; app, rescue and update loader use the same allowlist. MSVC/GCC policy checks,
address/undefined-behavior checks and target RAM-text validation passed. All 59 original host
artifacts remained byte-identical.

The package uses 323,436 image bytes, 76,184 RAM, 288,760 pool and 3,308 RAM-text. Its SHA-256 is
`9e7267eed6cabc83cbb8f0d85da4af1965c4cdb77cd0906b6a294ff8a5f977e3`.
Versioned build/install records are retained beside the package. A CRC/loader-validated FM Lab
rollback package is `build/rollback-fmlab-970.fwsc`; it is not a backup of presets/settings.

These observations do **not** establish CPU headroom, USB audio capture/MIDI musical behavior,
all-preset quality, persistence, cold-power-cycle behavior, soak reliability or physical rescue.
The results matrix below remains unmeasured.

## CPU overload and optimized device iteration

The original `0.1.0-test` firmware is **not performance-approved**. The user reported digital
distortion after a few notes and a peak of 264% across presets. On F1 INIT WAVE, with oscillator 2
silent and filter/effects off, measured app CPU was approximately:

| Held voices | Original app CPU |
| --- | ---: |
| 1 | 28% |
| 2 | 52% |
| 3 | 77% |
| 4 | 105%, audible distortion |

Target disassembly showed nested per-sample calls, repeated oscillator lookup preparation and
full lookup work even for the silent second oscillator. `0.1.1-test` caches settled lookup geometry,
skips silent lookups while preserving phase/control state, inlines hot helpers and avoids settled
filter-bypass ramp work. Polyphony, tables, interpolation, smoothing and storage formats are unchanged.
Scalar-reference tests compare complete oscillator state and samples. MSVC/GCC/ASan/UBSan checks
pass; all 59 original audio/screen/flash artifacts remain byte-identical.

The real target oscillator renderer contains no calls; synth rendering calls control/oscillator
routines at block granularity, not per sample. This establishes the intended code generation,
**not a measured hardware speedup**. The new image uses 325,948 bytes; RAM/pool/RAM-text remain
76,184 / 288,760 / 3,308 bytes. Package SHA-256:
`535be15c3e219d1303184b4a185e2d1926e550903e909e9c4b25863ade0a9c3c`.

The user approved installing this version on the same muted, stably powered unit. Installation
completed and an independent query confirmed running `FM-1_922`. The versioned package/build report
and original `0.1.0-test` evidence are retained. The user confirmed BUILD `0.1.1-test`: INIT WAVE
peaked at 49% with 12-18 held notes (still eight synthesis voices, using stealing); MORPH SAW
peaked at 78%. Controlled HOLLOW PAD measurements were 74% peak / zero LATE with four steady notes
and 132% peak / 410 LATE with eight. **This iteration still fails performance acceptance.**

`0.1.2-test` extends frame-pointer caching to moving positions, rebuilding pointers only at frame
boundaries. It also removes unnecessary unsigned-wide interpolation correction, skips inaudible LFO
waveform evaluation (not phase advancement), hoists settled filter/gain checks and advances the
control envelope exactly in blocks. Added scalar-state tests cover frame crossings, silence,
envelope edits and stage boundaries. Strict MSVC/GCC/sanitizer checks pass and all 59 original
artifacts remain identical. Target image: 326,476 bytes; other memory usage is unchanged.
Package SHA-256: `ff152b833230dc114c8794e37cc4286a760258f16773afda567b698d5e8d7fdf`.
The user approved this second update with muted output/stable power; installation and an independent
running `FM-1_922` query succeeded. Its screen/build and controlled F3 measurements remain pending.

### Subsequent measured iterations

The user confirmed each displayed build below. Measurements are user-reported, eight steady
F3 HOLLOW PAD notes unless specified; percentages describe app time, not the whole audio ISR.

| Build | CPU / peak / LATE | Peak SYN / FX | Outcome |
| --- | --- | --- | --- |
| 0.1.2-test | 70 / 85 / 0 | not instrumented | Four notes: 42 / 52 / 0; effects-off test also 70 / 85 / 0 |
| 0.1.3-profile | 73 / 85 / 0 | 70% / 13% | INIT WAVE: 27 / 41 / 0 |
| 0.1.4-test | 71 / 83 / 0 | 66% / 13% | Call-free oscillator/voice loops moved to fast RAM; small improvement |
| 0.1.5-test | 76 / 90 / 0 | 72% / 13% | Shared waveform cache worsened timing; removed in the next build |
| 0.1.6-test | 66 / 80 / 0 | 63% / 13% | O / V / C: 37% / 21% / 2%; displayed build confirmed |

These runs avoided app deadline misses but did not meet the below-50% peak headroom goal.
They do not establish worst-case/all-effects readiness or listening quality.

After the computer reboot, the `0.1.6-test` source/binary hashes, package CRCs and all 59 saved
host/sanitizer artifacts were reverified. The user reconnected the same unit and explicitly approved
installation with muted outputs/stable power. The updater completed and an independent query
confirmed running `FM-1_922`. The user confirmed BUILD `0.1.6-test` and the measurements above:
controlled F3 deadlines pass, but the peak headroom goal remains unmet.

This iteration removes the ineffective waveform cache, uses bit-exact 32-bit sample interpolation
verified against wide arithmetic, and adds peak-block O/V/C timing for oscillator, voice/filter/
envelope and control work. GLO bank 4 retains SYN/FX, voices and LATE; SEL resets the measurements.
Substage timings include instrumentation overhead and may not sum exactly to total app time.
Target usage: 326,796 image bytes, 76,216 RAM, 288,760 pool and 6,156 RAM-text; all limits and the
RAM-text no-call check pass. Package SHA-256:
`9872de4acaf9438c383b314634beeb7ab219c7e7a341b2139351f51ec84b690f`.
Versioned package/build/device records are retained; performance acceptance is still pending.

### Selected-output filter optimization: 0.1.7-test / 0.1.8-test

On `0.1.6-test`, the user reported peaks F7 96%, F3 80%, F8 59%, F5 90% and F4 84%.
The requested next milestone was F7 below 85%, without cutting voices or changing the presets.

The voice kernel now recognizes a settled, single selected filter output once per block. It
avoids unused output arithmetic and filter-type crossfade checks; settled coefficients are loaded
once per block. Moving coefficients still advance exactly every sample. Actual filter-type
transitions retain the original general path, and both internal filter histories always advance.
Tests compare exact samples and complete voice state to the original scalar algorithm across all
types, cutoff/resonance extremes, short blocks, type changes and coefficient-ramp retargets.
MSVC/GCC/sanitizers pass; all 59 original audio/screen/flash artifacts remain byte-identical.

The first iteration, `0.1.7-test`, optimized settled coefficients only: F7 improved to 55% CPU /
71% peak / zero LATE, and F4 peak to 60%. The user heard no F7 playback/filter-transition problem.
However, F5 hit 100% peak with reported aliasing: **this iteration was not accepted**.
`0.1.8-test` extends selected-output processing to moving coefficients to address that regression.
The user explicitly approved both installations with muted output/stable power; updater and
independent running `FM-1_922` queries passed, and the user confirmed each displayed build.

| 0.1.8-test workload | App CPU | App peak | LATE | Peak SYN / FX | Peak O / V / C |
| --- | ---: | ---: | ---: | --- | --- |
| F5 ODD BASS | 52% | 82% | 0 | 78% / 0% | 18% / 55% / 3% |
| F7 PULSE LEAD | 56% | 68% | 0 | not reported | not reported |

The requested F7 target is met and F5's timing regression is corrected in these tests.
Final-build low-volume listening, updated other-preset measurements and comprehensive hardware
acceptance remain pending; this does not establish the original below-50% worst-case headroom goal.
Target image/RAM/pool/RAM-text: 327,356 / 76,216 / 288,760 / 6,716 bytes, within all limits;
RAM-text remains call-free. Package/source hashes and CRCs pass. Package SHA-256:
`891dd2bbb9b81b8837b6d375275a67b364266e467caef7368e32e45c51b929e2`.
Versioned build/device records retain both the unsuccessful and final iterations.

## Before connecting a test device

Use the second FM-1, not the known-good FM Lab unit. Back up its existing data with that firmware's
supported method first. WT has no native bulk backup/export; do not assume DX7 tools can back it up.
Settings/bank addresses overlap FM; writes overwrite those regions. Have stable power and a known
recovery route. Start MASTER low and avoid headphones until levels are established.

Installable device-test packaging requires an **explicit separate FM-1_9NN identity**.
FM Lab's FM-1_97N family is reserved. The example identities in automated tests are synthetic, not
allocations. The approved local test uses FM-1_922; coordinate a public allocation before release.
Do not select one speculatively. Once the identity is approved, build a version marked
`-test`, retain its complete source/licenses and `build/wtlab-build.json`, and verify artifact hashes
before installation. Packaging alone is not release approval.

Application USB rescue requires intact app startup, flash driver and USB code. With a suitable
package/recovery setup established, OCT- alone at power-on should show `WT LAB USB RESCUE`, audio off.
Both octave buttons are not this shortcut. Retained-RAM boot-guard tests cover reset policy only;
they do not prove physical recovery or rescue an image that cannot reach startup. Do not begin with
deliberate power cuts or interrupted firmware updates.

## Establish basic operation

Verify cold boot and the on-screen BUILD against the recorded artifact. Start on F1 INIT WAVE:
local C4 should sound as a plain sine; oscillator 2 is silent. Check keys/octaves, both oscillator
levels, factory presets, mute/solo, A/B and every tap/hold editor. Confirm held notes release after
page, octave and MIDI-channel changes. Exercise mono fallback and ninth-note stealing.

Check USB MIDI and the jack independently, including receive-channel filtering, velocity-zero
release, CC 120/123 panic, bank/program boundaries and outgoing local-note pairing. Use the correct
TRS MIDI adapter type for the device. Confirm incoming programs are not echoed.

Record USB audio at 44.1 kHz while monitoring the analog output. Check stereo chorus/reverb,
mono summed onto both channels, MASTER operation and 24/16/12/8-bit modes. Compare against host
fixtures, but do not require analog or dithered USB captures to be byte-identical.

## Prepare a repeatable CPU workload

Use a spare/empty user slot only if saving this setup. The exact host gesture recipe is
`tests/wt_stress.txt`; it is a host script, not a file the MIDI helper uploads.

| Section | Setting |
| --- | --- |
| Oscillator 1 | FORMANT, position about 50%, level 100%, semitone 0 |
| Oscillator 2 | HOLLOW, position about 50%, level 100%, semitone 0; also test small fine detune |
| Amp | Short attack, full sustain; leave notes held |
| Filter | Low pass, cutoff about 1.1 kHz, resonance about 80%; also sweep cutoff/resonance |
| LFO 1 | Triangle, 2 Hz, depth about 75%, destination BOTH positions |
| LFO 2 | Sine, 1 Hz, depth about 25%, destination FILTER |
| Effects | All four enabled, each mix about 30%; start with their default remaining parameters |

Repeat with maximum feedback/decay, fast modulation, envelope movement and every table, particularly
dual AKWF VOX. Test high and low notes, position/table changes during a chord, and repeated
stealing/retriggering. Watch output levels; saturation safety is not a listening-quality guarantee.

The optional MIDI helper requires `mido` and `python-rtmidi` only for real MIDI. Preview is safe
without them and never enumerates/opens ports:

```powershell
python .\tools\load_test.py 8 20 --dry-run
```

Only when ready to test the chosen device:

```powershell
python .\tools\load_test.py 8 20 --port "exact FM-1 MIDI output name" --channel 1
```

Without a note count it stages 1, 2, 4, 6 and 8 notes. Choose the receive channel correctly; no
program/patch setup is sent. Ambiguous devices require an explicit port. Normal/interrupted runs
attempt note-off and CC 123 cleanup on that channel. If the link drops, use the device's panic/mode
controls or reconnect/send panic; software cannot release notes across a disconnected link.

## Measure, do not infer

GLO bank 4: APP CPU is the latest block, APP PEAK the maximum since reset, LATE the number of app
callbacks exceeding the ~5.80 ms budget. SEL resets peak/LATE at the next audio block. Host says
`NOT DEVICE`. The meter includes native DSP/publication/capture work, but excludes platform USB,
resampling/mastering and interrupt entry/exit. It is not a whole-ISR profiler.

For each note count, settle the sound, reset peak/LATE, hold at least 20 seconds, and record results.
Then repeat while continuously editing and drawing waveform, spectrum, envelopes/LFO and scope.
Diagnostics are not visible simultaneously with every page: reset, exercise one page, then return
to read peak/LATE. Test USB recording and heavy MIDI traffic concurrently, and soak the hardest
case for at least 10 minutes.

| Workload | App current | App peak | LATE after reset | Glitches/USB/notes |
| --- | --- | --- | --- | --- |
| Idle / INIT | not measured | not measured | not measured | not tested |
| 1 / 2 / 4 / 6 voices, stress setup | not measured | not measured | not measured | not tested |
| 8 voices, all effects | not measured | not measured | not measured | not tested |
| 8 voices + editing/spectrum/USB | not measured | not measured | not measured | not tested |
| Worst-case soak / rapid stealing | not measured | not measured | not measured | not tested |

Target **peak app load below 50%**, zero LATE, no glitches, no stuck notes and reliable USB/MIDI.
This leaves headroom but does not prove a sub-50% whole ISR: account for platform work and inspect
recorded audio. Any deadline miss blocks approval. If the target is missed, profile/optimize or
deliberately revise supported polyphony before release; do not infer readiness from desktop speed.

## Persistence, performance and recovery

Save a named two-oscillator sound with both LFOs/filter/envelopes and all effects. Verify UI fields
and sound after cold power cycling. Exercise U1-32/U2-01 and U8-32 using expendable slots; ensure
neighbor slots/banks survive normal saves. Device preferences and phrase must restore independently.
Boot must not resume PLAY, REC or a latched chord.

Check queued save under held/muted notes, latched arp, running loops and long tails; it must wait.
Release/stop and wait for `PATCH SAVED` / `CFG*` clearing. Check cancel and edits after confirmation.
Physical erase timing/write failures/endurance remain hardware questions. Any later destructive
power-loss testing requires expendable data, backups and already-tested recovery; a cut save may
lose the entire 32-slot bank or settings/phrase, with no rollback.

Exercise all arp patterns/rates/gate/swing/latch and one/two/four-bar phrases: count-in, recording,
overdub, quantization, automatic stop of recording, step audition, PLAY stop and REC hold-clear.
Use local and MIDI notes together, including same-pitch overlap and channel changes.

Finally verify a normal updater round trip and manual rescue with the confirmed identity and a
known-good package. Record device/model, package/source hashes, host OS/audio/MIDI setup, power
source, tests performed and failures. Hardware acceptance, identity allocation and release approval
must remain explicit; none is supplied by the build manifest.
