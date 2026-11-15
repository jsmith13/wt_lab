# WT Lab implementation plan

This is the historical implementation plan; WT Lab 1.0.0 now publishes the user-accepted
fourteen-table instrument. Remaining exhaustive hardware qualification is documented in the hardware guide.

Create WT Lab as a standalone sibling of FM Lab. Preserve FM Lab's interaction
conventions, replace its FM-specific engine and editor, and validate sound quality on the computer
before testing performance on the second FM-1.

## Scope and baseline

Steps 1 through 4 are implemented and host/target-build verified. The active `wtlab.c` plays the
native instrument; `wt_ui.c` implements its familiar editor. `fm_baseline.c` preserves the old FM
development baseline for the remaining workflow integration. Hardware performance is not yet verified.

The source snapshot includes FM Lab's current uncommitted improvements. It now has 256 user slots
and sample-rate controls, rather than the 128 slots described during the initial research.
Preserve these in the baseline. The final WT patch capacity remains subject to its storage layout;
do not reduce the slot count merely to match the older description.

Engine direction: adapt the relevant MIT-licensed fixed-point wavetable playback code from
[Mutable Instruments Braids](https://github.com/pichenettes/eurorack/tree/master/braids), rather than
importing the entire synthesizer. Factory tables combine original generated material and a
curated selection from [AKWF](https://github.com/KristofferKarlAxelEkstrand/AKWF-FREE), whose repository
declares CC0. Neither dependency was included in step 1; step 2 adds the small attributed adaptation,
original generated tables, and pinned CC0 source material without importing either whole project.

## 1. Establish an independent baseline

Copy the current FM Lab source, including its current improvements, but exclude Git history,
generated builds, and temporary files. Leave the original completely untouched.

Rename the application, build artifacts, and documentation to WT Lab. Preserve upstream licenses
and attribution. Introduce distinct, versioned settings and patch identifiers so WT Lab cannot
interpret FM Lab's stored patches as its own. Confirm an available firmware package identity before
producing an installable release.

Checkpoint: the copied instrument builds and passes representative existing host tests before
synthesis changes begin.

Status: complete, verified on 2026-11-09. The unmodified copy passed the panel checks before renaming.
After renaming, the panel, tap/hold focus, bank/SysEx round-trip, MIDI, and sample-rate checks pass.
The panel's 16-bit and 24-bit WAV files are byte-identical before and after the baseline changes.
Both Windows/MSVC and Linux/WSL host runners were exercised. Four build-policy unit tests pass.

The real JieLi check-only build passes image/RAM, RAM-text, hardware-access, and ROM-reference checks:
application 142,512 / 581,564 bytes; RAM 67,460 / 98,304 bytes; pool 281,192 / 344,064 bytes.
No installable package was generated, and no device was flashed or measured. Synthesis remains FM.

Baseline identity policy: use the non-installable `WTLABDEV` identity for check-only firmware builds.
An installable package requires an explicitly supplied, confirmed available `FM-1_9NN` identity;
never reuse FM Lab's `FM-1_97N` family or infer an allocation from the release version.

Baseline persistence policy: `WTS1` settings and `WTB1` banks identify version 1 of the inherited
FM layouts. Change the version when introducing incompatible native WT data. Flash addresses are
still shared with FM Lab, so separate identifiers prevent misinterpretation, not overwriting.
Back up device data before switching firmware.

## 2. Build the wavetable oscillator and data pipeline

Extract and adapt Braids' relevant fixed-point playback/interpolation code into a small C oscillator
module, retaining attribution. Avoid importing its unrelated synthesis models or hardware layer.

Implement 44.1 kHz playback with a wrapping phase accumulator, interpolation between samples and
frames, pitch-dependent band-limited levels, smooth level transitions, and smoothing of live parameter
changes. Editing table position must not restart the note.

Create an offline table generator that produces flash-resident sample arrays and metadata. Start
with eight tables of eight frames, expanding only after listening and measuring. Include original
harmonic teaching tables and a curated selection of CC0 AKWF material; retain source files,
provenance, licenses, and reproducible conversion settings.

Source preparation includes DC removal, phase alignment where appropriate, level management, and
musically coherent frame ordering. Interpolation alone is not anti-aliasing. Braids' original
96 kHz pitch tables and simple oversampling are not suitable unchanged for this design.

Checkpoint: rendered audio verifies tuning, frame endpoints, smooth scanning, bounded output,
and alias suppression across low and high notes.

Status: complete, verified on Windows/MSVC, Linux/GCC, and the real JieLi compiler. At this checkpoint
the active `wtlab.c` instrument was unchanged. No package was produced and no hardware was flashed.

### Implemented design

`firmware/src/wt_osc.h` and `wt_osc.c` provide an allocation-free, fixed-point oscillator with a wrapping
32-bit phase accumulator, sample/frame interpolation, and seven smoothly blended harmonic levels.
The 44.1 kHz pitch lookup covers MIDI notes 0..134, including Q8 fractional semitones. Output is signed
16-bit mono for subsequent voice mixing. There is no runtime floating point or 64-bit division.

Live position, increment, and gain edits use exact 128-sample ramps (2.90 ms). Repeated publication of
an unchanged target does not restart the ramp. Table edits use a 128-sample crossfade; retargeting an
active fade completes that fade before moving to the latest selection, rather than jumping or mixing
all eight tables. This can take up to about 5.80 ms after the final selection. Only starting a new
oscillator resets phase. Invalid controls return an error without changing the existing note.
Integration must serialize oscillator mutation and rendering on the audio owner.

`tools/gen_wavetables.py` reproducibly creates `firmware/src/wt_data.h` from seven original mathematical
tables and eight retained AKWF vocal WAVs. It checks source/license hashes, removes DC, phase-aligns
the vocal fundamentals, orders those frames by spectral brightness, and RMS-matches full-band frames.
One peak scale per table applies to all frames and harmonic levels, preserving fundamental gain across
levels. Source revisions, conversion details, and licenses are recorded in `LICENSING.md` and the
AKWF manifest.

The eight tables are SINE-SAW, ODD, PULSE, FORMANT, ORGAN, GLASS, HOLLOW, and AKWF VOX. Each has eight
frames at harmonic limits 127, 63, 31, 15, 7, 3, and 1. Every level retains 256 samples plus a wrap guard:
`8 * 7 * 8 * 257 * 2 = 230,272` flash bytes. This exceeds the early rough 64 KiB estimate deliberately:
retaining full-length cycles at the sparse high-note levels avoids the interpolation images caused by
aggressively shortening those cycles. The complete bank fits the target image budget.

### Verification record

`tests/run_wt.ps1` and `tests/run_wt.sh` compile with warnings treated as errors, exercise oscillator
behavior, render audio, and run independent FFT/tuning checks. Coverage includes sample and frame
interpolation, endpoints and phase wrap, all tables at low/high pitch and frame edges, every level
boundary, invalid-control atomicity, finite ramps, rapid table retargeting, and render block-size
independence. Linux address/undefined-behavior sanitizers also pass.

All three generated WAVs are byte-identical between Windows and Linux. Tuning checks from MIDI 24
through 127, including a fractional semitone, remain within one cent. The saw comparison measures
folded-partial rejection of 55.7 dB at 37.7 Hz, 64.5 dB at 737.5 Hz, 82.7 dB at 3170.8 Hz, and 88.2 dB
at 7951.1 Hz. At the lowest frequency both renderers already use the full-band level; the higher three
cases improve on naive full-band playback by more than 35 dB. Fundamental changes between levels stay
below 0.1 dB in the rendered comparisons. All generated frames satisfy DC, harmonic-limit, wrap-guard,
and headroom checks, and vocal frames share fundamental phase.

The real target link probe (`python3 tests/wt_targetcheck.py` in Linux/WSL) passes image/RAM, RAM-text,
hardware-access, and ROM-reference checks:

| Minimal WT probe | Used | Budget |
| --- | ---: | ---: |
| Application image | 260,768 bytes | 581,564 bytes |
| RAM | 15,376 bytes | 98,304 bytes |
| Pool | 65,280 bytes | 344,064 bytes |

These are platform-plus-oscillator figures, not the finished UI/polyphonic instrument or a hardware CPU
measurement. The default FM application also rebuilds successfully and is byte-identical to the
step 1 binary; the four existing packaging-policy tests pass.

Audition `build/host/wavetable-demo.wav`: two seconds scanning each table, in the order listed above.
`wavetable-tuning.wav` contains six tuning checks; `wavetable-alias.wav` alternates band-limited and
naive saw playback at the four comparison frequencies. These are engineering previews, not presets.
Hardware listening/performance checks and protection against aliasing from arbitrary audio-rate
modulation or later nonlinear processing are not claimed.

## 3. Implement the polyphonic instrument

Target eight voices, each containing two wavetable oscillators.

| Component | Initial controls |
| --- | --- |
| Each oscillator | Table, position, level, semitone tuning, fine detune |
| Amplifier | Attack, decay, sustain, release; velocity response |
| Per-voice filter | Cutoff, resonance, type, keyboard tracking |
| Filter envelope | ADSR and bipolar depth |
| Two LFOs | Rate, shape, depth, destination, key sync |

Keep modulation routing small and explicit rather than introducing a full matrix. Initial
destinations should cover oscillator positions, pitch, and filter cutoff. Envelope-driven position
scanning should also be available.

Reuse released-first voice stealing, mono last-note priority, panic handling, and live patch updates
where applicable. Preserve oscillator phase and envelope progress during ordinary edits.
Independent filter envelopes require per-voice filter state; the inherited shared filter alone
cannot supply that behavior.

Checkpoint: chords, overlapping releases, mono legato, voice stealing, and modulation behave correctly
without stuck notes or discontinuities.

Status: complete, verified on Windows/MSVC, Linux/GCC, and the actual JieLi toolchain.
The one-oscillator/amp/keys/MIDI/HOME milestone was exercised first; the complete engine now supports
eight voices with two oscillators each. The sibling `fmlab` folder remains untouched.

### Engine and ownership

`wt_synth.h` / `wt_synth.c` define a typed, validated native in-memory patch and an allocation-free
Q23 voice mixer. This is not yet the persistent patch format. Each oscillator has table, normalized
position, level, +/-24 semitones, +/-1 semitone Q8 fine detune, and bipolar motion-envelope depth.
Amp and filter/motion ADSRs run independently per note. The second envelope can move both positions
and cutoff with independent positive/negative depths.

The per-voice TPT state-variable filter follows FM Lab's topology but keeps separate integrators,
coefficients, and type-crossfade weights for every note. Cutoff spans 20 Hz..16 kHz, resonance spans
Q = sqrt(0.5)..8, and tracking goes from none to approximately one semitone per key relative to C4.
`tools/gen_wt_dsp.py` reproducibly generates Q30 filter coefficients and the sine LFO table. A
129-cutoff/16-resonance grid, bilinear lookup, and coefficient ramps avoid runtime floating point and
64-bit division. Filter type edits crossfade rather than abruptly switching outputs.

Both LFOs have 0..20 Hz rate, five shapes (sine, triangle, saw, square, random hold), depth, key sync,
and one explicit destination: position 1, position 2, both positions, pitch, or cutoff. Free-running
LFOs use the instrument clock, including while silent; key-synced LFOs restart only on a fresh note.
Control targets update every 32 samples and reuse the oscillator's 128-sample ramps. Pitch modulation
at full depth spans an octave either way; position/cutoff depth can span the full normalized range.
Positions/cutoff clamp at their endpoints rather than wrap. Oscillator pitches retain step 2's limits.

ADSR times span 0..5000 ms, with a 128-sample fast/declick minimum. Envelope arithmetic uses Q24 and
exact finite ramps; sustain edits slew, and time edits preserve current level and fractional stage
progress rather than retrigger. Ordinary patch changes and mono legato preserve oscillator phases,
filter state, and envelope progress. New notes/retriggers reset their phases and envelopes; an old
nonzero output is faded out over 128 samples during stealing/retriggering. Velocity response is
adjustable from fixed level to linear MIDI-velocity gain.

Allocation prefers a repeated held pitch, then a free voice, then the oldest releasing voice, then
the oldest held voice. Age comparisons survive counter wrap. Mono has unique last-note priority,
fallback with remembered velocity, and room for all 128 MIDI keys. A stale stolen note-off cannot
release a different replacement pitch. Panic/mode changes clear held-note ownership with a short
declick, and edits during panic cannot prolong its release. Mixing reserves eight-voice headroom;
a Q23 output limiter and bounded resonant states protect extreme patches.

The app's main loop owns `edit`, UI, and both publication buffers. After startup, only audio owns
`wt_synth_t`. Published patches and a 64-event note/mode ring are consumed by the ISR. Overflow is
reported visibly and clears/panics the pending event stream rather than silently losing note-offs.
The key ledger remembers both pitch and transmit channel, so octave/channel/page changes cannot
strand an outgoing note. The updater remains available through the unchanged platform.

### Playable development app

The old FM app is retained as `firmware/src/fm_baseline.c`; `wtlab.c` is now the WT app. Its basic
HOME/EDIT/ENV/LFO/GLO pages reuse the fonts, palette, four-row controls, SELECT banks, SEL fine
adjustment, and keyboard ownership conventions. HOME shows a full-band table preview at the most
recent voice's smoothed position. A simple sine with oscillator 2, filter, and modulation depth off
is the initial patch. The README lists all temporary controls.

These pages establish access to the engine, not completion of step 4: tap/hold focus, the complete
waveform/neighbor-frame view, mute/solo, and A/B integration remain. SAVE/PRESETS/FX/ARP/SEQ/PLAY/REC
report their pending native workflow explicitly. Settings/banks are neither loaded nor written;
this avoids misinterpreting the FM layouts while native persistence is still pending step 5.

### Verification record

`tests/run_wt_synth.ps1` and `tests/run_wt_synth.sh` verify both generated headers, compile/run the
voice and application-ownership harnesses, then drive the actual app with `wt_instrument.txt`.
Core tests use strict warnings as errors. They exercise eight voices/two oscillators, released-first
and held-note stealing, age wrap, duplicate notes, stale note-offs, mono fallback/all-128-key ownership,
live edits, sustain/time edits, panic during edits, retrigger declicking, independent per-note filters,
key tracking, both LFOs/destinations/sync modes, bipolar motion, and extreme resonant/modulated patches.
Whole/irregular renders remain byte-identical across control boundaries and overlapping releases.
Application tests cover ring counter wrap, overflow-to-panic, invalid publication, and latest-complete
patch consumption.

Audio checks verify each oscillator's tuning, semitone/fine offsets, velocity gain, and low-/high-pass
responses. Measured attenuation is -13.15 dB for the LP fixture above cutoff and -23.34 dB for the HP
fixture below cutoff. The scripted panel test checks C4 tuning, chords, dual oscillators, audible and
visible position edits, releases across page/octave changes, velocity-zero note-off, MIDI channel
filtering, original outgoing-channel ownership, and CC panic. Output reaches exact silence after
release/panic. Voice fixtures and 16-/24-bit instrument WAVs are byte-identical between MSVC and GCC.
Address/undefined-behavior sanitizers pass for the voice engine, app event/ownership harness, and
the full keyboard/MIDI/editor scenario.

The normal JieLi check-only build passes all image/RAM, RAM-text, hardware-access, and ROM-reference
checks with no compiler warnings:

| Step-3 playable WT app | Used | Budget |
| --- | ---: | ---: |
| Application image | 296,588 bytes | 581,564 bytes |
| RAM | 21,360 bytes | 98,304 bytes |
| Pool | 65,280 bytes | 344,064 bytes |

The four packaging-policy tests still pass. No package was produced and no device was flashed.
These sizes exclude the later effects/bank integration. Eight-voice hardware CPU load, worst-case
audio deadlines, and hardware listening remain step-6 work, not claims inferred from host timing.
Preview `build/host/wt_instrument.wav`; inspect `wt-voices.wav` for tuning/velocity/filter fixtures.

## 4. Adapt the familiar UI

Keep fonts, spacing, colors, four-knob legends, tap/hold behavior, SELECT bank navigation,
and SEL fine adjustment.

| Control | WT Lab behavior |
| --- | --- |
| HOME | Wavetable/frame view, spectrum, output waveform |
| EDIT | Selected oscillator; tap exposes its primary controls |
| ENV | Amp/filter envelopes and position-envelope routing |
| LFO | Selected LFO and its routing |
| GLO | Instrument-wide tuning, MIDI, mono/poly, output settings |
| FX | Familiar tap/hold entry; native effect DSP/workflow integration in step 5 |
| ALGORITHM encoder | Table selection for the selected oscillator |
| SEL | Fine adjustment; oscillator mute/solo in relevant editing contexts |

Use the first two operator-labeled black keys for oscillator selection; do not invent six
oscillator-like objects merely to fill the panel. Preserve existing A/B and mono/poly gestures
where they remain appropriate.

Replace the algorithm diagram with a view that shows neighboring frames, the interpolated waveform,
and the actual modulated position.

Checkpoint: shortcut and full-editor controls change the same parameters; changing pages while
holding notes never strands them.

Status: complete, host and target build verified. Step 4 adds:

- Shared field mappings for shortcut knobs, full-editor controls, value text, and four-knob legends.
  EDIT/ENV/LFO tap exposes the selected object's primary controls; hold 400 ms opens its full editor.
  Focused SELECT steps real objects; full-page SELECT or another page tap steps parameter banks.
- Two-oscillator selection, release-only SEL mute/solo, fine-gesture consumption, mono/poly gestures,
  and RAM-only A/B tap-swap/hold-copy. Muting changes only the published audio patch, never stored
  oscillator levels. A/B excludes mute masks and instrument preferences. Existing key ledgers retain
  original note/channel ownership even when HOME notes become shortcut keys on another page.
- Neighboring-frame curves, live interpolation, frame/mix labels, base/live position markers,
  amp/filter envelope plots and live markers, and LFO shape/phase/routing displays. Oscillator
  previews share the actual mip/frame/table blending; LFO plots share the engine's shape helper.
- HOME spectrum and triggered output waveform, reusing the existing integer analyser with a native
  sine source, not Dexed. Audio alone owns the rolling output capture and newest-voice state;
  a request/ready mailbox holds immutable metadata/samples while main draws or runs the FFT.
  This brings the analyser/scope portion of step 5 forward without effects or persistence.
- GLO instrument transpose/fine tuning, MIDI and mono/poly, filter/velocity controls and real
  24/16/12/8-bit output processing. Device preferences remain outside patch A/B. Dry stereo/mono
  outputs are currently identical; effects in step 5 will make stereo routing meaningful.
- Expanded application checks for exact tap/hold boundaries, common controls, explicit routes,
  mute/fine/solo isolation, A/B ownership, held shortcut-key note-offs, immutable captures, analyser
  bins and output quantization. `tests/wt_ui.txt` exercises the real main-loop gestures and views,
  including audible mute/unmute, A/B, live octave retuning and 8-bit output.

Windows/MSVC and Linux/GCC pass oscillator, voice, application, audio and panel checks. The five
voice/instrument/UI WAVs and 23 scenario screenshots match byte-for-byte across compilers.
Address/undefined-behavior sanitizers pass the engine, application harness and both complete scenarios.
The real JieLi check-only build passes RAM-text, image/RAM/pool, hardware-access and ROM checks:
image **303,448 / 581,564 B**; RAM **27,984 / 98,304 B**; pool **75,520 / 344,064 B**.
Four packaging-policy tests pass. No installable package or device flash; hardware timing/listening
remain step 6. Native SAVE/PRESETS/FX/ARP/SEQ/PLAY/REC explicitly remain pending step 5.

### Visual alignment follow-up

After the `0.1.8-test` CPU milestone, the actual current FM Lab and WT Lab emulators were run with
the same eleven-view `tests/wt_layout.txt` scenario. Side-by-side inspection exposed differences
that reusing the fonts alone had not resolved. The revised WT UI matches FM Lab's HOME title
at y18, editor header at y24, HOME plot at y64..200, full parameter rows at y56/88/120/152,
ENV/LFO graph at y44..148 and cells at y152, patch line at y44/188, legend at y208 and status at y224.
It also adopts the reference's lilac accent, white values, gray labels, amount bars, battery/view
pips, step-cell geometry and inline SAVE cursor. HOME knob feedback is transient below the legend;
GLO's detailed diagnostics intentionally retain a compact layout rather than dropping timing fields.
All oscillator/envelope/LFO/filter controls and gesture mappings are preserved.

Focused tests check real rendered layout, amount bars, stale-pixel clearing, unused cells, knob
feedback expiry, inline saving and safe long-version placement. Existing visual checks now inspect
the new plot regions/colors. MSVC/GCC/ASan/UBSan pass with 59 matching artifacts; all 44 screens
intentionally differ from the old UI, while all 15 original audio/flash artifacts remain identical.
The shared comparison scenario's 16-/24-bit audio is also unchanged.
`build/ui-comparison` contains three FM/before/after PNG sheets and a provenance JSON.

The `0.1.9-test` real-target check-only build passes at 354,052 image bytes, 76,472 RAM,
288,760 pool and 6,716 RAM-text, with call-free audio kernels. Only `wt_ui.c` changed among
target build inputs. No installable package or device flash; physical UI/load testing is pending.
The unchanged `0.1.8-test` package/build/device records and a verified source ZIP are retained
as the user's last known-working rollback.

The user subsequently requested packaging/installation and confirmed muted outputs with stable power.
The `FM-1_922` package's app/loader match the tested check build exactly; source/package integrity
checks passed. Installation and an independent running-identity/USB query succeeded. The user
confirmed BUILD `0.1.9-test` and normal revised UI startup. Versioned package/build/device records
are retained; further hardware UI/load/listening checks remain pending, and `0.1.8-test` remains
the designated known-working rollback.

### UI behavior parity and dual-oscillator HOME

User observations on `0.1.9-test` drove a follow-up: gray cumulative charging bars, FM battery
geometry/thresholds, 1.5-second knob feedback in the patch line, and no title repaint below y43.
The previous y18..50 title canvas overlapped the patch line by six pixels, explaining its partial
flicker; a transfer-level test catches the overlap even when completed screenshots look correct.
Default HOME now shows OSC 1/OSC 2 in independent 120-pixel panels, including table/neighbor/live
waveforms, blend/position markers and silent/muted indication. EDIT's single-oscillator preview remains.
Short FX is the four fixed wet mixes; hold FX enters the original detailed banks. Displayed amounts
and chorus width use unitless 0..100, with physical units retained for rates/times; labels match
DEPTH, FEEDBK and PRE-DELAY.

Focused charging, transfer, feedback, panel isolation and FX regressions pass with MSVC/GCC and
ASan/UBSan. All 59 artifacts match across builds. The workflow fixture now enters detailed FX
with a hold; its two old WAV hashes consequently change. Replaying the same updated inputs against
the verified `0.1.9-test` source archive produces identical audio/flash; all other original audio
is unchanged. `tests/wt_ui_parity.txt` and `build/ui-parity` retain twelve actual-emulator comparisons.
The `0.2.0-test` real-target check-only build passes at 354,576 image bytes, 76,472 RAM,
288,760 pool and 6,716 call-free RAM-text. The matching package was subsequently installed with
explicit safety approval and independent identity/USB checks. The user reports normal operation,
but did not separately report the exact BUILD string. Package/build/device records and a verified
source ZIP are retained; `0.1.8-test` remains the designated known-working rollback.

### HOME filter/scope parity and compatible cutoff extension

`0.2.1-test` preserves the underlying HOME plot/title under the four FX wet-mix shortcuts; changing
that focus no longer clears/restarts the plot. Detailed FX unit suffixes are joined to their values.
Filter HOME follows FM's warped-TPT response visualization, using generated WT log-tangent data and
the same interpolated resonance damping as WT DSP. The base-setting curve is not a representation
of a polyphonic sum or individually modulated voice cutoffs. The note/harmonic ruler and scope
note/window labels use the captured newest note and global tuning, not live audio-owned state or
the selected oscillator's detuning. Scope fill is the same RGB(52,62,120) as FM; actual output
pixels match. WT-specific ENV/LFO drawing remains unchanged. Fresh span is 5 kHz; saved spans
remain authoritative.

The user chose preserving old cutoff frequencies over globally remapping the dial to 20 kHz.
The original 129 knots and all 6,192 Q30 coefficients remain bit-identical. Five extra knots at
positions 66048..68096 extend the original logarithmic grid, with the last knot capped at 20 kHz.
`WT_CUTOFF_MAX` is independent of oscillator `WT_POSITION_MAX`; coefficient interpolation,
validation, display and control clamping accept the extension. Envelope/key-tracking scales are
unchanged; modulation now saturates at the higher ceiling. The 172-byte patch format is unchanged,
but an extended base cutoff is invalid to older WT readers. Their all-or-nothing bank validation
rejects the entire affected 32-slot bank; back up before rollback and never overwrite it there.

Focused UI/boundary/resonance/transition/storage tests and MSVC/GCC/ASan/UBSan pass. All 59
regression artifacts match; all 15 pre-existing audio/flash artifacts match 0.2.0 and 13 screens
intentionally change. `tests/wt_home_parity.txt` and `build/ui-home-parity` retain thirteen paired
actual-emulator views. Target check-only passes at 357,456 image bytes, 76,472 RAM, 288,760 pool
and 6,716 call-free RAM-text (2,247 instructions). No new installable package or flash;
hardware UI/load/listening acceptance remains pending.

### Paired HOME controls and non-disruptive EDIT state

`0.2.2-test` promotes dual HOME's thin position rail into an eight-frame bar with independent
base/live markers and adds a configured-gain bar. Level bars are not VU meters, require no new
audio processing/capture metadata, and dim rather than forget the gain when muted. Permanent
frame/blend numbers move to a 16-pixel strip at y40 in full EDIT; HOME retains transient knob values.

HOME fields encode their oscillator target, so knob routing/value feedback no longer depends on
whichever oscillator was previously selected. The primary bank is LVL1/POS1/LVL2/POS2; SELECT
switches to DET1/PENV1/DET2/PENV2. DETUNE adjusts the combined Q8-semitone offset, using 256-unit
normal steps and one-unit SEL-held steps. It writes equivalent bounded legacy semitone/fine fields,
preserving their total +/-25-semitone capability and existing preset offsets. Three-decimal semitone
feedback resolves every fine step. PENV reuses the native bipolar position-envelope depth.
Paired edits select/highlight their target oscillator, and table selection wraps with `ui_wrap`.

Short EDIT keeps the current HOME plot and knob bank; it enables the existing OP1/OP2,
MONO/POLY and high-F-sharp A/B key contexts. The header marks EDIT state/selected oscillator.
HOME exits without changing the view. HOME control-bank/focus-only transitions no longer clear
plots or reset spectrum peaks. Full EDIT retains its parameter banks and exact frame information.
ENV/LFO drawing, filter/FX control maps, DSP and storage formats remain unchanged.

Focused bar/endpoints/mute, field routing, detune precision/bounds, cyclic-table and all-view
edit-state tests pass. MSVC/GCC/ASan/UBSan produce 59 matching existing artifacts plus 17 matching
new scene artifacts; all 15 existing audio/flash artifacts remain identical. One stress-fixture
knob assignment changes to preserve its original oscillator-2 position edit under the new map.
`tests/wt_home_controls.txt` and `build/ui-home-controls` retain fifteen inspected frames.
Only `wt_ui.c` changes among target inputs. Check-only passes at 358,024 image bytes, 76,472 RAM,
288,760 pool and 6,716 call-free RAM-text (2,247 instructions). The matching package was subsequently
installed with explicit safety approval. Source/app/loader parity, independent package CRCs,
updater restart, running identity and USB enumeration pass. The user confirmed on-screen BUILD
`0.2.2-test` and normal HOME with the new bars. Further physical UI/load checks remain pending;
no hardware performance claim. Versioned build/device records and a verified
source ZIP are retained; prior 0.2.0 and designated known-working 0.1.8 packages remain intact.

### Compact table headings and control-only ENV shortcut

`0.2.3-test` replaces HOME's OSC headings with the respective table names. Short ENV now keeps
the current HOME plot/peaks and maps the knobs to AMP ADSR on entry; SELECT/repeated ENV switches
AMP/motion ADSR. The full envelope graph/cells/routing editor remains unchanged.
Focused framebuffer/application checks, the MSVC instrument regression suite and GCC ASan/UBSan
application checks pass. Twelve regenerated audio/flash artifacts match 0.2.2-test byte-for-byte.
Only `wt_ui.c` changes among target inputs; target checks pass at 357,988 image bytes, 76,472 RAM,
288,760 pool and 6,716 RAM-text with 2,247 call-free instructions.

The matching package was installed with fresh safety approval. App/loader/source parity,
independent package CRCs, updater restart, identity and USB audio/MIDI enumeration pass.
The user confirmed BUILD `0.2.3-test`, normal HOME and the revised ENV shortcut.
Further physical UI/listening/CPU/load checks remain pending; no new hardware load claim.
Versioned build/validation/device records and verified source ZIP are retained.
Prior 0.2.2 and designated known-working 0.1.8 packages remain intact.

### Output quality controls and composed menu rows

`0.2.4-test` uses FX MIX and EDIT WT 1/2 titles, with selected operator/mode LEDs driven through
the existing physical-key mapping. Label/value/bar rows are composed before one LCD transfer;
the previous second value-box transfer briefly exposed a cleared right-hand area.
GLO page 3 is velocity/sample-rate/bit-depth/output, keeping cutoff-envelope routing in ENV.

Existing descending bit-depth encodings remain unchanged; knob direction and bar fill are inverted.
An audio-owned fractional sample-and-hold clock processes voice output before bit quantization/FX
at 8/11.025/16/22.05/32 kHz. Full-rate 44.1K bypasses it; changing rate primes a fresh first sample.
Synthesis, envelopes, per-voice filtering, modulation and effects remain at their native clock.
Preferences are published with the existing mailbox and remain outside A/B sound comparisons.

The settings codec reads schema 1 at native rate and schema 2 with its rate index in the formerly
reserved final word. Native-rate writes keep the original bytes/schema; reduced-rate saves use
schema 2. All preferences and 64 phrase notes round-trip without changing sound-bank format.
Before older-firmware rollback, restore native rate and await quiet autosave or retain a settings
backup. Older firmware cannot read reduced-rate preferences/phrase.

MSVC regressions and GCC ASan/UBSan checks pass; 18 new scene artifacts match exactly.
Twelve regenerated native-rate audio/flash fixtures remain unchanged. Actual emulator frames were
inspected. Target: 358,652 image bytes, 76,488 RAM, 288,760 pool, 6,716 RAM-text and 2,247 call-free
instructions. DSP kernels/table/filter data/platform inputs are unchanged. Subsequently packaged
and installed with fresh safety approval; source/app/loader parity and independent package CRC,
running identity and USB audio/MIDI checks pass. Build/device records and matching source ZIP
are retained. User confirmed BUILD 0.2.4-test, normal HOME and reported that it looks good.
Exhaustive physical UI/quality, listening and CPU/load checks remain pending; previous 0.2.3 and
designated known-working 0.1.8 packages remain preserved.

### Control-only HOME LFO shortcut

`0.2.5-test` includes LFO focus in the existing control-only HOME signature and rendering path.
Entering, switching LFOs or leaving the shortcut no longer clears the plot/spectrum peaks or
substitutes the LFO waveform. The original wavetable/filter/scope visualization continues using
the same live audio snapshot. RATE/SHAPE/DEPTH/DEST, remembered LFO selection, SELECT/repeated-tap
cycling and fine rate/depth gestures keep their existing behavior. Long LFO still draws the
waveform above the primary/key-sync parameter cells.

Focused tests verify all HOME views, both LFO targets, exact patch edits, title changes,
fine gestures, plot/peak preservation and full-editor transitions. MSVC regressions and strict
GCC ASan/UBSan application/scene checks pass; all 18 scene artifacts match between toolchains.
Twelve regenerated native-rate audio/flash artifacts remain unchanged; twelve actual frames were
inspected. Target: 358,540 image bytes, 76,488 RAM, 288,760 pool, 6,716 RAM-text and 2,247 call-free
instructions. Only wt_ui.c changed among target inputs. No package or flash; installed 0.2.4 and
designated known-working 0.1.8 remain preserved.

### FM-style ARP/SEQ HOME shortcuts

`0.2.6-test` makes all HOME rendering independent of shortcut focus/bank: ARP and SEQ now
retain the same wavetable/filter/scope path used by the sound shortcuts. The step grid is
reserved for PAGE_STEP. The FM reference was read without modifying fmlab.
ARP uses ARP 1/2 or ARP 2/2 and FM's exact knob legends; its selected bank is remembered
separately from the shared editor bank, following the existing LFO/effect selection pattern.
Another ARP press still toggles it, with latched-on feedback when applicable.
SEQ uses a fixed SEQ title and tempo/bars/div/click controls, with its context LED active
in the shortcut or step editor. Native performance parameters, clocks and transport behavior
are unchanged; PLAY/REC still work without leaving the shortcut.

Focused tests cover every HOME plot/peak, FM titles/legends, exact performance edits, ARP
bank memory/toggles, fixed SEQ controls, 399/400 ms holds and running playback/overdub/stop.
MSVC regressions and strict GCC ASan/UBSan application/scene checks pass; all 18 scene
artifacts match between toolchains. Twelve historical audio/flash artifacts remain unchanged
and twelve actual emulator frames were inspected. Target: 358,556 image bytes, 76,488 RAM,
288,760 pool, 6,716 RAM-text and 2,247 call-free instructions. Only wt_ui.c changed among
target inputs since 0.2.5-test. Subsequently packaged and installed with fresh safety approval,
without force. Source/app/loader parity and independent package CRC, running identity and
USB audio/MIDI checks pass. Versioned build/validation/device records and matching source
ZIP are retained. The user reported that it looks good; exact BUILD and exhaustive
shortcut/listening/CPU confirmation remain unconfirmed.
Previous 0.2.4 and designated working 0.1.8 rollback packages remain unchanged.

### Octave and PLAY button lighting

`0.2.7-test` copies FM's main-loop LED behavior using the existing hardware glow layer.
OCT-/OCT+ are steady at +/-1 and pulse 600 ms bright / 300 ms dim at +/-2; returning
to zero clears both layers. PLAY's bright layer follows the published sample clock's
first sixteenth of each quarter-note beat, including count-in. The glow layer indicates
an available phrase even when stopped. REC lighting is unchanged.

The host adapter now records glow states as well as bright states. Focused tests cover
both octave directions, phase boundaries, clamping/cross-zero/navigation and PLAY's
empty/available phrase, clock acknowledgement, 120/200 BPM pulses, stop/clear and count-in.
MSVC application regressions and strict GCC ASan/UBSan application checks pass.
Audio/screenshot fixtures were not regenerated for this LED-only change.
Target: 359,048 image bytes, 76,504 RAM, 288,760 pool, 6,716 RAM-text and 2,247 call-free
instructions. Only wt_ui.c changed among target inputs; DSP/platform/HAL inputs are unchanged.
Subsequently packaged and installed with fresh safety approval, without force.
Source/app/loader parity and independent package CRC, running identity and USB audio/MIDI
checks pass. Versioned build/validation/device records and verified matching source ZIP
are retained. The user reported "Everything looks good!" and designated 0.2.7-test as last
known working on 2026-11-13. Broader hardware/CPU acceptance remains pending; previous 0.2.6
and former working baseline 0.1.8 rollback artifacts remain unchanged.

## 5. Restore the complete instrument workflow

Bring across the effects, arpeggiator, looper, and preset naming/storage. Spectrum/scope and RAM-only
A/B are already available from step 4; integrate them with the complete native sound/workflow.

Define a native WT patch format that saves oscillators, envelopes, filter, modulation, and effects
together. Keep MIDI channel, output mode, and other device preferences separate. Preserve the user-bank
workflow and target the baseline's 256 slots if the finalized patch size fits the existing allocation.
Any required capacity change must be deliberate and documented.

Remove DX7-specific import/export while preserving updater SysEx handling.
Custom wavetable import stays outside version one.

Checkpoint: saving and reloading reproduces the complete sound; malformed or incompatible data
is rejected explicitly.

Status: complete, verified on Windows/MSVC, Linux/GCC and the actual JieLi compiler.
Earlier sections retain their historical checkpoint sizes and then-pending limitations; this section
describes the current instrument. The sibling `fmlab` source remains untouched.

### Native sound and storage

`wt_storage.c` encodes every field explicitly in little endian, not as a compiler structure dump.
The 172-byte patch saves both oscillators, both ADSRs, filter/velocity/tracking, both LFOs, all
16 effect parameters and an 11-character name plus terminator. Eight native factory sounds provide
simple starting points through pads, bass, keys and leads. The initial sound remains a plain sine.

Each user bank holds a 16-byte schema header and 32 patches: **5,520 bytes**, fitting the existing
two-sector allocation with its outer record header. All eight banks and **256 user slots** remain.
`storage_layout.h` retains the physical allocations; `storage.h` shares the actual CRC/header/programming
implementation between hardware and the fake-flash host. `WTB2` and `WTS2` distinguish the incompatible
native records from the earlier FM/baseline layouts. Validation checks version, length, CRC, reserved
fields, all parameter ranges and printable/terminated names. A malformed bank is rejected atomically;
none of its voices is partially accepted. Empty/rejected banks initialize to INIT WAVE, with explicit
rejection status for corrupt/incompatible data.

The separate 240-byte settings wire format saves device preferences, selected preset/save slot,
performance controls and up to 64 phrase notes. It does not embed an edited sound or mute mask.
Boot restores these and the selected saved sound but leaves recording, arp latch and transport stopped.

`wt_workflow.c` implements PRESETS/bank jumps, incoming/outgoing MIDI bank/program selection,
name/slot entry and immutable queued-save snapshots. Incoming programs are not echoed. Factory bank 0
uses programs 0..7; user banks 1..8 use programs 0..31. GLO's fourth bank controls program transmission.
HOME cancels a queued save; a completed snapshot never replaces later edits or falsely marks them saved.

Flash erases still stall interrupts. Main requests a quiet pause; audio acknowledges only after voices,
queued input and audible tails have settled and performance is stopped. Main revokes acknowledgement
for newly held input or running/latched performance before writing. Bank writes compare the intended
CRC against readback before replacing the RAM bank; settings compare their complete readback bytes.
Failures report explicitly and do not claim a saved sound. Settings autosave after two unchanged seconds
and a quiet opportunity; persistent failure does not retry-spin.

**Compatibility/power-loss limits:** new magic does not create new flash space. WT writes overwrite the
corresponding FM regions; back up device data before switching. Each bank is erased in place, with its
header written last. There is no redundant copy/rollback: interrupted saves can invalidate all 32 slots
in that bank, while other banks remain unchanged. A cut settings save can lose device preferences and
the phrase. The host verifies rejection, not preservation of the old bank under every interruption.

### Effects and performance

`wt_fx.c` ports the GPL FM Lab integer effect kernels/coefficient tables, without linking the FM engine:
chorus, six-stage phaser, interpolated/filtered-feedback delay and stereo feedback-network reverb.
Main computes coefficients into complete patch publications; audio alone owns the active copy and all
effect histories. Bounded validity counters replace asynchronous pool clears, so disabled/re-enabled
effects never replay stale data and main never races an active delay/reverb buffer.

FX tap/hold uses the familiar shortcut/full-editor controls. The audio path is native voices/filters,
bit-depth processing, effects, click, stereo/mono routing and the real post-effects scope/spectrum.
Mono duplicates the stereo mean, not just the left channel.

`wt_play.c` adapts FM Lab's clock/arp/phrase kernels to native voices and the 32-sample control block.
Clock telemetry uses a seqlock. Arp provides five patterns, one to four octaves, latch, six note rates,
gate, swing and velocity at 40..238 BPM. The phrase has 64 notes across one/two/four bars, one-bar count-in,
quantized starts/releases, one-pass recording and overdub. SEQ hold opens cursor/key step entry,
duration edits and short cursor auditions; REC hold clears/stops. Both panel and MIDI feed performance.

Separate source/generated-note ownership prevents overlapping panel, MIDI, arp and phrase releases
from cutting one another. Panic also clears stale generator ownership. Page/octave/channel changes
retain outgoing key note-off/channel pairing. DX7 data imports/exports are absent; updater SysEx handling
remains platform-owned and unchanged. Custom table import remains outside version one.

### Verification record

The native host runners now cover voice, UI, storage and performance checks plus four actual-app
scenarios: `wt_instrument.txt`, `wt_ui.txt`, `wt_workflow.txt`, and `wt_reload.txt`. The last is a distinct
process opening the raw flash image saved by the workflow scenario. Windows/MSVC and Linux/GCC produce
**nine matching WAVs, 39 matching panel screenshots and an identical persisted flash image**.

Focused checks cover every user slot and bank boundary, full native patch/settings round trips,
bit-identical freshly rendered saved/reloaded sound including all four effects, bad versions/ranges/
names/CRCs/reserved fields, incompatible old magic, I/O and erase/payload/header/readback failures.
They verify no RAM commit on failure, unaffected sibling banks, quiet deferral under held/muted keys,
latched arp, silent loop portions and reverb tails, revoked acknowledgements, cancellation and save
snapshots during later edits. They also cover per-source releases, real mono averaging, each effect's
impulse/stereo response, stale-history prevention, cold minimum-delay starts with poisoned pool memory,
maximum effect settings, step entry/playback,
record count-in/quantization/automatic end, overdub, latch replacement and panic.

Scripted audio checks measure C4/E4/G4/C5 from the step grid, the actual count-in click, post-release
effect tails, reloaded stereo effects, note-off silence, no incoming MIDI echo, and visible control
legends. Address/undefined-behavior sanitizers pass the engine, expanded application harness and all
four complete scenarios. Four packaging-policy tests pass.

The real JieLi check-only build passes RAM-text, image/RAM/pool, hardware-access and ROM checks,
without compiler warnings:

| Complete step-5 WT app | Used | Budget |
| --- | ---: | ---: |
| Application image | 322,416 bytes | 581,564 bytes |
| RAM | 76,168 bytes | 98,304 bytes |
| Pool | 288,760 bytes | 344,064 bytes |

No installable package was produced and no device was flashed. These are build/host results, not
hardware CPU, listening, physical flash-endurance or power-loss measurements; those remain step 6.

## 6. Validate and package

Extend the existing host harness with focused audio and UI tests. Run the actual JieLi build and
its image-size, RAM, hardware-access, and ROM-reference checks.

On the second FM-1, measure worst-case load with eight voices, both oscillators, moving positions,
resonant filters, modulation, all effects, and active screen drawing. Aim below half the audio budget;
do not claim eight-voice readiness from host performance. Verify MIDI, USB audio, saving, power cycling,
and rescue/update behavior.

Deliver a standalone source tree, reproducible table generator, factory presets, quick reference,
host-test outputs, and a clearly marked device-test firmware with a confirmed separate package identity.

### Step 6 progress: pre-hardware work complete

The software/build portion was verified before connecting a device. At this milestone no device was
contacted/flashed, no installable package was produced and no package identity was allocated.

- Added GLO bank 4 diagnostics: read-only app CPU, peak and build, active voices and deadline-miss
  count. SEL requests an audio-owned peak/miss reset. Host explicitly shows `NOT DEVICE`; app timing
  excludes platform USB/mastering and cannot establish whole-ISR headroom. Long release-version
  labels truncate visibly rather than overflowing the display buffer; the report retains the full version.
- Added an actual-UI workload combining eight dual-oscillator voices, moving positions, two LFOs,
  resonant filtering, all effects and analyser drawing, plus ninth-note stealing and panic recovery.
  Focused tests cover read-only/reset semantics and retained-RAM boot-guard/reset-button policy.
- Adapted the optional MIDI load helper to WT: validated ranges, explicit ambiguous-port selection,
  channel-correct note-off/panic cleanup and a backend-free dry run. Only dry-run/mocked tests were
  executed; real MIDI use is part of the pending bench work.
- Enforced normal RAM, pool and RAM-text bounds/missing-symbol failures. Successful builds retain
  deterministic artifact hashes, memory/check results and firmware/build-source hashes in
  `build/wtlab-build.json`; failed builds invalidate stale reports. This is not a hermetic
  toolchain/SDK archive or hardware approval.
- Added the [native quick reference](docs/QUICK-REFERENCE.md) and
  [hardware validation guide](docs/HARDWARE-TESTS.md), with setup, exact stress recipe, measurement
  sheet, acceptance criteria, storage risks and recovery prerequisites. The old FM map is retained
  separately as [FM-QUICK-REFERENCE.md](docs/FM-QUICK-REFERENCE.md).

Final pre-hardware verification:

- Windows/MSVC and Linux/GCC oscillator and native suites pass. All **59 artifacts** match
  byte-for-byte: three standalone oscillator WAVs, plus eleven native WAVs, 44 screenshots and one
  persisted flash image.
- `sh tests/run_wt_sanitize.sh` reproduces address/undefined-behavior checks of the oscillator,
  eight-voice engine, storage/UI/performance/recovery-policy harness and all five actual-app
  scenarios. All 59 resulting artifacts match the normal Windows/Linux outputs.
- Six build-policy/resource/provenance tests and four no-device load-helper tests pass on Windows
  and Linux. The minimal real JieLi oscillator probe also still passes.
- Repeated full real JieLi check-only builds produce identical application, loader and JSON report;
  reported binary hashes match the files. No installable package was produced.

| Region | Step 6 used | Available |
| --- | ---: | ---: |
| Application image | 323,004 bytes | 581,564 bytes |
| RAM | 76,184 bytes | 98,304 bytes |
| Pool | 288,760 bytes | 344,064 bytes |
| RAM-text | 2,924 bytes | 24,576 bytes |

Still required: a confirmed separate identity and clearly marked device-test package, then physical
CPU/headroom/listening, MIDI/USB/audio, saving/cold-power-cycle, updater/rescue and soak tests on the
second FM-1. Do not substitute host timing, simulated flash failures or boot-policy tests for these.
The hardware guide deliberately leaves the results unmeasured.

### First local hardware smoke test: 2026-11-10

The user subsequently approved the connected FM-1, expendable/backed-up existing data and
`FM-1_922` for a local `0.1.0-test` package, not a public reservation. Preflight found the copied
platform accepted only the older flash chip; FM Lab's shared Puya/UCUN allowlist and RAM-only
stuck-flash reset policy were brought into WT app/rescue/loader without modifying `fmlab`.
Normal/slow/boundary/wrapped waits, rejection of unknown chips and stuck-operation reset pass
MSVC/GCC and sanitizer checks; all 59 original host artifacts remain byte-identical.

The real package passes SDK/CRC/source/artifact checks and target validation: 323,436 image bytes,
76,184 RAM, 288,760 pool and 3,308 RAM-text. Installation completed through the normal USB-MIDI
updater; an independent query confirmed `FM-1_922` and normal audio/MIDI enumeration. The user
confirmed the normal WT Lab screen, clean local-key sine audio and normal release.

See [the hardware record](docs/HARDWARE-TESTS.md) for hashes, versioned build/install records and
the validated FM Lab rollback package. CPU, comprehensive listening, USB capture/MIDI response,
persistence/cold-power-cycle, soak and physical recovery testing are still pending. This is a
successful initial smoke test, not hardware acceptance or public release approval.

### Hardware CPU failure and optimization iteration

Subsequent testing failed the audio deadline: original INIT WAVE app CPU was approximately
28/52/77/105% for 1/2/3/4 held notes, with distortion at four; other presets reached 264% peak.
The first smoke test therefore does not approve playable polyphony.

Target assembly exposed repeated lookup preparation, nested per-sample helper calls and full
rendering of a silent oscillator. `0.1.1-test` caches settled oscillator lookup geometry, skips
silent sample reads while advancing all phase/control state, inlines hot helpers and skips settled
filter-bypass ramp work. No voice limit, sound parameter, interpolation or storage-format change.
New tests compare complete oscillator state and every sample with the scalar renderer. All strict
MSVC/GCC and sanitizer checks pass; all 59 baseline artifacts remain byte-identical.

Real target code contains no oscillator-renderer calls and only block-level synth calls.
The image is 325,948 bytes; RAM/pool/RAM-text remain 76,184 / 288,760 / 3,308 bytes. Source/artifact
hashes, package CRCs and target resource checks pass. The user approved installation on the same
muted unit; updater completion and an independent `FM-1_922` query are verified. Versioned original
and optimized artifacts are retained. User-confirmed `0.1.1-test` improved INIT WAVE peak to 49%,
but controlled F3 HOLLOW PAD still reached 74% / zero LATE with four notes and 132% / 410 LATE
with eight. Performance acceptance failed; listening remains unconfirmed.

The second iteration, `0.1.2-test`, caches frame pointers even while position ramps, refreshing
them only at frame boundaries. It simplifies signed interpolation, skips unused LFO waveform
evaluation without skipping phase, hoists settled filter/gain checks and advances the control
envelope exactly in blocks. Additional scalar sample/state tests cover frame boundaries, silence,
envelope edits and stage transitions. MSVC/GCC/sanitizers pass and all 59 original artifacts remain
byte-identical. Target image is 326,476 bytes; other memory usage is unchanged. The user approved
installation with muted output/stable power; updater and independent running-identity query pass.
Controlled F3 measurements and listening remain pending. Desktop equivalence and target code
generation do not establish hardware headroom.

### Selected-output filter milestone: 0.1.8-test

Further measured iterations are retained in `docs/HARDWARE-TESTS.md`. On `0.1.6-test`, F7 peaked
at 96% and F5 at 90%; the user requested F7 below 85%. The voice kernel now prepares fixed-type
filter processing once per block, skips unused output mixing, caches settled coefficients and
preserves per-sample coefficient ramps during cutoff/resonance changes. Type transitions retain
the general crossfade algorithm. Shared state-update arithmetic preserves both filter histories.

New scalar-reference tests compare samples and full voice state across all filter types, extremes
and ramp retargets. Strict MSVC/GCC, sanitizers and all 59 baseline artifacts pass unchanged.
The first iteration improved F7 but regressed F5, so it was superseded rather than accepted.
The final user-approved/installed `0.1.8-test` measures F7 56% CPU / 68% peak / zero LATE and
F5 52% / 82% / zero LATE. The requested F7 milestone is met without changing polyphony or presets.
Image/RAM/pool/RAM-text are 327,356 / 76,216 / 288,760 / 6,716 bytes; target resource/no-call,
package CRC and source/artifact hash checks pass. Final listening and comprehensive bench
acceptance remain pending; the original below-50% worst-case goal is not established.

## First playable wavetable milestone

One oscillator, one original morphing table, amp envelope, MIDI/keys, and HOME visualization.
Establish that end-to-end before adding the second oscillator, modulation, and the full preset system.

Completed during step 3 and verified before the full voice/modulation checks. The native initial
patch still exposes this simple one-oscillator starting point; oscillator 2 is brought in by its level.
