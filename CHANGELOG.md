# Changelog

## 0.5.0-waves - complementary factory tables, 2026-11-15

- Append STRINGS, REED, E-PIANO, PLUCK, CHIP and GRIT: six eight-frame CC0 AKWF morphing tables.
- Preserve all original eight table IDs and exact generated sample bytes, factory voices,
  native storage layout, four GLOBAL pages and guarded CPU1 rendering.
- Retain pinned unmodified source WAVs, SHA-256 manifests and brightness/shape selection metrics.
- Expand navigation/codec/DSP/state/recovery and phase/alias/headroom checks to the new bank.
- Sample data totals 402,976 bytes. Existing CPU1/cache/copy/timing algorithms are unchanged.
- Absolute MIDI table selection now spans fourteen tables. Rollback firmware cannot read new
  table IDs and may reject a bank containing them; preserve needed data before rollback.
- This candidate is unflashed and does not inherit 0.4.8's known-working designation.

## 0.4.9-cpu1 - diagnostic cleanup, 2026-11-15

- Restore the normal four GLOBAL pages by removing the experimental CPU1, OTHER/cache
  and JOB profiling screens and their UI-only fields/worker toggle.
- Retain normal system diagnostics, peak/LATE reset and guarded CPU0 fallback notification.
- Leave the accepted 0.4.8 audio engine, cache, word copies, batching and internal timing unchanged.
- The exact tested 0.4.8 package is now user-designated known-working; this cleaned successor
  requires separate installation/acceptance and does not inherit exhaustive hardware qualification.

## 0.4.8-copy - experimental, 2026-11-15

- Start from the exact installed 0.4.7 staging profiler; preserve all profiling for comparison.
- Replace only CPU0 staging and accepted-commit voice copies with aligned 32-bit representation copies.
- Assert voice alignment/whole-word size and use GNU may_alias semantics; other compilers retain
  the original struct assignment. Leave generic memcpy, configuration copies and DSP untouched.
- Preserve private staging, completion/retirement guards and original-state recovery.
- Target 0.4.7 disassembly confirms both 568-byte voice transfers call the byte-wise memcpy loop.
- No hardware gain claimed until measured; installed 0.4.7 and known-working 0.4.3 remain preserved.

## 0.4.7-stage - experimental, 2026-11-15

- Start from the exact installed 0.4.6 cache source, retaining its validated package and 0.4.3 rollback.
- Add CPU0-only JOB subcomponent timing: setup, voice staging, dispatch, acceptance/commit,
  accepted mix merging, global state/clamp, and residual bookkeeping.
- Add read-only GLOBAL 7/7; all measurements share the existing immutable originating-peak capture.
- Keep rendering/recovery and pure wait outside JOB; rejected worker output is never merged.
- Preserve cache semantics, voice/DSP behavior, batching, admission limits, recovery and voltage/clock.
- Additional profiling perturbs execution; this is instrumentation, not an optimization.

## 0.4.6-cache - experimental, 2026-11-15

- Reuse prepared live patch/effects/vibrato across unchanged base, MIDI and mute inputs only
  while route-value/mix and bend ramps are settled.
- Recompute all moving-control boundaries, including the final ramp endpoint; preserve pitch,
  gain/pan interpolation, performance updates, patch validation/application and DSP timing.
- Keep prepared state audio-owned, invalidate on initialization, and expose per-peak cache hit/miss counts.
- Retain the finer profiler and all CPU1 batching/admission/recovery/power policies.
- Separate from installed 0.4.5 and known-working 0.4.3; no hardware speedup yet established.

## 0.4.5-profile - experimental, 2026-11-15

- Start from the exact 0.4.4 diagnostic source, preserving known-working 0.4.3 and installed 0.4.4.
- Split OTHER into publication/events, live control/patch preparation, job staging/commit,
  output, view/quiet capture and remaining measured work.
- Time effects configuration as a nested PREP component; retain originating PEAK in the snapshot.
- Add read-only GLOBAL 6/6; original system/worker diagnostics remain at 4/6 and 5/6.
- Preserve DSP, batching, MIDI timing, voltages/clock, admission and recovery policy.
- This is instrumentation, not an optimization or a hardware cost attribution.

## 0.4.4-profile - experimental, 2026-11-15

- Start from user-designated known-working 0.4.3, retaining its exact source/package separately.
- Measure per-callback CPU0 rendering (including retired-worker recovery) and pure worker-join wait.
- Capture CPU0, wait, effects, other work and CPU0 oscillator/voice/control percentages through the
  existing immutable display mailbox, associated with the callback that raised PEAK.
- Keep synthesis, 32/64 batching rules, voltages/clock and all admission/recovery limits unchanged.
- Additional timer reads perturb timings slightly; this is a diagnostic build, not a claimed speedup.

## 0.4.3-cpu1 - experimental, 2026-11-15

- Add 64-sample worker batches with exact 32-sample control/envelope/LFO substeps.
- Batch settled manual voices; retain 32-sample jobs during controller smoothing/publications,
  arpeggiator or sequence playback. Effects, stereo and performance clocks still process every 32 samples.
- Include job size in startup fingerprint and build reports. Keep the 600 us cap, recovery reserve,
  dispatch admission, unchanged voltage/clock and staged rollback.
- Separate comparison package from 0.4.2; host/target checks alone do not establish hardware acceptance.
- User designated known working on 2026-11-15 after reporting peak 76%, versus 89% on both
  0.4.1 and the control-only 0.4.2. Retain both prior rollback packages; exhaustive qualification pending.

## 0.4.2-cpu1 - experimental, 2026-11-15

- Move per-voice oscillator/filter control preparation into staged voice rendering, with immutable
  patch, tuning, controller and global-LFO inputs. CPU0 retains allocation, publication and global clocks.
- Preserve partial-control-block timing, global/key-synced LFO/random wrap, control-error propagation
  and original-state re-render after worker failure.
- Keep 32-sample jobs as the isolated control-offload comparison; preserve all guard/power limits.
- Independent untouched 0.4.1 serial renderer is the output/full-state oracle.

## 0.4.1-cpu1 - experimental

- Keep a voice job on CPU0 when a full 600 us worker budget no longer fits before the callback cutoff.
  Avoid dispatching nearly at the cutoff and then retiring an otherwise healthy worker.
- Separate request-age timeout (WHY 2) from callback worker cutoff (WHY 7).
  Preserve dispatch age and signed time left at detection on GLOBAL 5/5; show serial SKIP count.
- Keep all timeout, recovery-reserve, voltage and clock limits unchanged.
- Follow-up to repeatable 0.4.0-cpu1 retirement during playing; hardware cause and speedup remain unproven.
- Installed for testing on 2026-11-14 with fresh safe-power/output approval, without force.
  Restart, independent running identity and USB audio/MIDI checks passed; worker/load acceptance pending.
- User designated last known working on 2026-11-15 after reporting improved stability and peak 89%
  versus 96% on the former baseline. Full hardware qualification remains outstanding; `0.3.2-test` retained.

## 0.4.0-cpu1 - experimental

- Isolated opt-in CPU1 voice rendering with balanced active-voice staging and single-core recovery.
- Startup memory self-test, per-core fault handling, RAM-idle flash gate and safe reset/update halt.
- Unchanged voltages and clock; 600 us request-time timeout and callback deadline with provisional recovery reserve.
- GLOBAL 5/5 worker diagnostics and session-only dispatch switch. Existing settings/patch formats unchanged.
- Not a release or last known-working designation; no hardware speedup/reliability qualification yet.
- Installed for guarded testing on 2026-11-14 without force after fresh safe-power/output approval.
  Updater restart, independent identity and USB audio/MIDI enumeration passed.
  CPU1 startup, exact screen BUILD, listening/load and stability acceptance remain pending.

## WT Lab - Unreleased

- **Last known-working baseline:** the user approved `0.3.2-test` on 2026-11-14.
  Its verified package/source archive and matching build/check/device records are retained;
  older `0.2.7-test` and `0.1.8-test` rollbacks remain intact. Full hardware acceptance remains pending.

- **CC assignment follow-up:** PRESETS selects destination groups,
  ALGORITHM selects targets; removed the navigation hint and lowered values/bars toward their labels.
  Installed as `0.3.2-test` on 2026-11-14 after fresh safe-output/power confirmation, without force.
  Package matches the tested check-only build; CRC/source/archive checks, restart and independent
  USB audio/MIDI checks pass. Physical UI/listening and CPU acceptance remain pending;
  `0.2.7-test` remains last known working.

- **CC assignment layout:** envelope-style grouped destination selector,
  ALGORITHM for group and PRESETS for target, and SOURCE / DEPTH / POLAR / BEND knob labels.
  GLO advances assignment slots; SELECT/HOME retain their usual navigation without redundant hints.
  Existing global mappings and stored destination IDs are unchanged.
  Installed as `0.3.1-test` on 2026-11-14 with fresh safe-output/power confirmation, without force.
  Package matches the tested check-only build; CRC/source/archive checks, updater restart and
  independent USB audio/MIDI checks pass. Physical UI/listening and CPU acceptance remain pending;
  `0.2.7-test` remains last known working.

- **MIDI controllers and Learn:** sustain, smooth wheel/expression, adjustable pitch bend
  (default +/-2 semitones), channel pressure and standard volume/pan/filter/effects CCs.
  Sixteen globally stored routes expose depth/polarity and Learn via held GLO, matching FM Lab.
  Physical parameter edits regain control; the next MIDI movement reactivates the assignment.
  Live values do not alter native patches. Sustain-aware voice stealing/repeated notes/mono fallback,
  separate All Sound Off / Reset All Controllers / All Notes Off, and legacy settings migration.
  Packaged as `0.3.0-test` and installed on 2026-11-14 with fresh muted-output/stable-power
  approval, without force. Package/app/loader CRCs, 62 source hashes and matching source ZIP
  were verified before installation; updater restart and independent USB audio/MIDI checks pass.
  Hardware timing/listening acceptance remains pending; this does not supersede the
  user-designated `0.2.7-test` baseline.

- **Octave and PLAY light parity, 0.2.7-test check-only:** OCT-/OCT+ light steadily at
  one octave and pulse 600 ms bright / 300 ms dim at two, using FM's existing glow layer.
  Zero clears both lights. PLAY glows when a phrase is available and flashes on each
  sample-clocked beat during playback/count-in; stopping retains the phrase glow.
  REC lighting, octave range, note ownership, storage and transport behavior are unchanged.
  MSVC application regressions and strict GCC ASan/UBSan application checks pass.
  Focused tests cover both directions, phase boundaries, clamping/navigation, phrase
  availability, clock acknowledgement, beat flashes at 120/200 BPM, stop/clear and count-in.
  Target: image 359,048 bytes, RAM 76,504, pool 288,760, RAM-text 6,716;
  2,247 call-free instructions. Only wt_ui.c changed among target inputs.
  Subsequently packaged and installed with fresh muted-output/stable-power approval,
  without force. Source/app/loader match the tested check build; independent package CRC,
  running identity and USB audio/MIDI checks pass. Versioned build/validation/device
  reports and a verified matching source ZIP are retained. The user reported "Everything looks good!"
  and designated 0.2.7-test as last known working on 2026-11-13. Broader hardware/CPU acceptance
  remains pending; previous 0.2.6 and former working baseline 0.1.8 artifacts remain unchanged.
- **FM-style ARP/SEQ HOME shortcuts, 0.2.6-test check-only:** both borrow the knobs without
  replacing the current wavetable/filter/scope plot or clearing spectrum peaks.
  ARP titles are ARP 1/2 and ARP 2/2, with TEMPO/PATTERN/OCTAVE/LATCH and
  GATE/RATE/SWING/VELO legends matching the read-only FM reference. SELECT cycles the
  independently remembered ARP bank; another ARP press still toggles it, including latch feedback.
  SEQ uses the fixed TEMPO/BARS/DIV/CLICK bank and SEQ title, including during PLAY/REC.
  The SEQ context light matches FM Lab; long SEQ retains the real step editor.
  Focused all-view tests cover plot/peak preservation, exact knob routing, bank memory,
  toggle/hold boundaries and uninterrupted playback/overdub/stop behavior.
  MSVC regressions and strict GCC ASan/UBSan application/scene checks pass; all 18 scene
  artifacts match between toolchains. Twelve historical native-rate audio/flash artifacts
  are unchanged; twelve actual emulator frames were inspected.
  Target: image 358,556 bytes, RAM 76,488, pool 288,760, RAM-text 6,716; 2,247 call-free
  instructions. Only wt_ui.c changed among target inputs since 0.2.5-test.
  Includes the LFO shortcut change. Subsequently packaged and installed with fresh
  muted-output/stable-power approval, without force. Source/app/loader match the tested
  check build; independent package CRC, running identity and USB audio/MIDI checks pass.
  Versioned build/device records and verified matching source ZIP are retained.
  The user reported that it looks good; exact on-screen BUILD and exhaustive shortcut/listening/CPU checks remain unconfirmed;
  0.2.4 and designated working rollback 0.1.8 packages are unchanged.
- **Control-only LFO shortcut, 0.2.5-test check-only:** short LFO retains the current
  wavetable/filter/scope HOME plot and spectrum peaks instead of replacing it with the LFO graph.
  RATE / SHAPE / DEPTH / DEST edit the last selected LFO; SELECT or another short LFO tap
  switches LFO 1/2 and the title identifies the target. SEL-held fine rate/depth edits are preserved.
  HOME exits without cycling the view. Long LFO retains its selected waveform editor and key-sync bank.
  Focused tests cover all three HOME views, both targets, exact knob routing, fine gestures and
  full-editor transitions. MSVC instrument regressions and strict GCC ASan/UBSan checks pass.
  All 18 new scene artifacts match between toolchains; twelve regenerated native-rate audio/flash
  artifacts are unchanged. Twelve actual emulator frames were inspected.
  Real-target checks pass: image 358,540 bytes, RAM 76,488, pool 288,760, RAM-text 6,716;
  2,247 call-free instructions. Only wt_ui.c changed among target inputs.
  No package or flash; 0.2.4-test remains installed and 0.1.8-test remains the working rollback.
- **Quality controls and FM title/LED parity, 0.2.4-test check-only:** short FX uses FX MIX;
  HOME EDIT uses EDIT WT 1/2 and lights the selected OP1/OP2 and MONO/POLY keys.
  Long-menu label/value/bar rows now use one composed LCD transfer, removing the blank-value pass
  that caused right-hand text flicker.
  Clockwise bit-depth changes increase quality, with 24-bit full-bar, preserving stored bit encodings.
  GLO page 3 is VELOCITY / SAMPLE RATE / BIT DEPTH / OUTPUT; ENV > CUT remains in ENV routing.
  Sample-and-hold rates span 8/11.025/16/22.05/32/44.1 kHz before FX, with 44.1K default/full-bar;
  synth/filter/envelope/effects timing remains native, with no oversampling or CPU-saving claim.
  Old settings load unchanged; native-rate saves retain schema 1. Reduced-rate settings use
  schema 2, so return to 44.1K and await quiet autosave before rolling back to older firmware.
  Tests cover LEDs/titles, coherent row transfers, quality direction/bounds, exact fractional
  holding clocks, native-output integration and settings migration/full-phrase round-trip.
  MSVC regressions and GCC ASan/UBSan checks pass; all 18 new scene artifacts match exactly.
  Twelve regenerated native-rate audio/flash artifacts remain unchanged; the old 8-bit fixture
  reverses its encoder delta to preserve its intended sound. Actual emulator frames were inspected.
  Target passes: image 358,652 bytes, RAM 76,488, pool 288,760, RAM-text 6,716; 2,247 call-free
  instructions. DSP kernels, table/filter data and platform inputs are unchanged.
  Subsequently packaged and installed with fresh muted-output/stable-power approval, without force.
  Packaged app/loader/source match the tested check build; independent package CRC, running identity
  and USB audio/MIDI checks pass. Versioned build/device records and matching source ZIP are retained.
  The user confirmed BUILD `0.2.4-test`, normal HOME and reported that it looks good.
  Exhaustive physical UI/quality, listening and CPU/load checks remain pending; 0.1.8-test remains the working rollback.
- **Compact HOME headings and control-only ENV, 0.2.3-test check-only:** each wavetable name replaces its OSC 1/OSC 2
  heading, retaining the selected-side accent. Active panels no longer duplicate their names;
  muted/zero-level panels retain separate OFF/SILENT indications. Waveforms and bars are unchanged.
  Short ENV preserves the current wavetable/filter/scope HOME plot and spectrum peaks, assigning
  AMP ADSR on entry. SELECT or another short ENV switches AMP/motion ADSR; the title identifies
  the active envelope. Long ENV retains its existing graph, parameter cells and routing bank.
  Framebuffer tests cover all eight tables, both panels, selection/status, all three HOME views,
  both ADSR knob mappings, fresh AMP entry and full-editor transitions.
  The MSVC instrument regression suite and GCC ASan/UBSan application tests pass.
  Actual emulator HOME/AMP/motion/full-editor frames were inspected in `build/ui-env-home`.
  Twelve regenerated audio/flash regression artifacts remain byte-identical to 0.2.2-test.
  Target checks pass at 357,988 image bytes, 76,472 RAM, 288,760 pool and 6,716 RAM-text
  (2,247 call-free instructions). Subsequently packaged and installed with fresh safety approval.
  Packaged app/loader/source match the tested check build; independent package CRC, running identity
  and USB audio/MIDI checks pass. The user confirmed BUILD `0.2.3-test`, normal HOME and ENV behavior.
  Versioned build/validation/device records and matching source ZIP are retained.
- **Paired HOME controls and EDIT state, 0.2.2-test check-only:** dual HOME now has larger eight-tick
  position bars with separate base/live markers, and configured-level bars without permanent numbers.
  Muting dims level fill without losing the gain setting. Exact frame/blend details remain in full EDIT.
  Primary HOME knobs are LVL1/POS1/LVL2/POS2; SELECT switches to DET1/PENV1/DET2/PENV2.
  DETUNE combines existing tuning fields, with semitone steps normally and 1/256-semitone steps
  while SEL is held. PENV exposes each oscillator's bipolar position-envelope depth.
  Touched paired controls select/highlight their oscillator. Table selection wraps cyclically.
  Short EDIT preserves the current HOME visualization and knob bank while enabling OP1/OP2,
  MONO/POLY and high-F-sharp A/B. The header identifies EDIT state and selected oscillator.
  Filter/span controls, ENV/LFO visuals, DSP and storage formats are unchanged.
  Focused routing, detune-boundary/precision, bar/mute, key-context and transition tests pass.
  All 59 existing and 17 new scene artifacts match MSVC/GCC/ASan/UBSan; all 15 existing audio/flash
  artifacts remain identical. The stress fixture uses the new POS2 knob for its original setup.
  Fifteen emulator frames are in `build/ui-home-controls`. Target checks pass at 358,024 image bytes,
  76,472 RAM, 288,760 pool and 6,716 call-free RAM-text. Subsequently packaged/flashed with explicit
  safety approval. Package app/loader/source exactly match the tested check build; independent
  package CRC, running identity and USB checks pass. The user confirmed BUILD `0.2.2-test` and normal
  HOME with the new bars. Further hardware UI/load testing remains pending.
  Verified source and device records are retained.
- **HOME/filter/scope parity, 0.2.1-test check-only:** short FX changes the four wet-mix shortcuts
  without replacing or clearing the underlying HOME plot/title. Detailed FX values join Hz/ms/s
  without spaces. Filter HOME gains the green base-response curve using native WT cutoff/Q,
  played note and harmonic ruler, wrapping TYPE, compact Hz/kHz and unitless RESO feedback.
  Fresh span is 0-5K; saved span preferences are preserved. Scope shows played note and actual
  milliseconds, triggers from the note rather than the selected oscillator, and retains FM's
  exact fill color. WT ENV/LFO visuals are unchanged.
  Cutoff extends to 20 kHz with five extra knots; all 6,192 old coefficients, 129 frequencies
  and 16 resonance values are unchanged. Existing modulation scales remain, with the higher ceiling.
  Saving base cutoff above 16 kHz prevents older WT firmware from loading that entire 32-slot bank.
  Focused UI, cutoff-boundary, high-resonance/transition and storage tests pass. All 59 artifacts
  match MSVC/GCC/ASan/UBSan; all 15 existing audio/flash artifacts match 0.2.0.
  Thirteen paired FM/WT frames are in `build/ui-home-parity`. Target checks pass at 357,456 image
  bytes, 76,472 RAM, 288,760 pool and 6,716 call-free RAM-text. No package or flash.
- **UI behavior parity and dual HOME, 0.2.0-test check-only:** gray battery with the FM Lab
  outline/ADC thresholds and cumulative 600 ms charging bars. Removed the HOME title's six-pixel
  overlap with the patch line, the source of its partial flicker. HOME knob feedback now replaces
  the patch line for 1.5 seconds instead of appearing below the hints. Default HOME shows both
  oscillators side by side; EDIT retains the full-width selected preview.
  Short FX now exposes the four effect mixes, while hold FX retains detailed banks. Removed FX
  percent/degree suffixes, made width unitless 0..100, and matched DEPTH/FEEDBK/PRE-DELAY labels.
  Focused transfer/charging/feedback/mapping/formatting tests and MSVC/GCC/ASan/UBSan pass with
  59 matching artifacts. Audio/flash match archived 0.1.9 source under identical inputs; two
  older workflow WAVs change because the fixture now uses held FX entry. Twelve actual-emulator
  before/after views are in `build/ui-parity`. Target checks pass at 354,576 image bytes,
  76,472 RAM, 288,760 pool and 6,716 RAM-text. Subsequently packaged/flashed with explicit safety
  approval; independent identity/USB checks pass and the user reports normal operation.
  Exact screen BUILD was not separately reported; verified source and device records are retained.
- **FM Lab visual alignment, 0.1.9-test check-only:** ran both actual emulators through eleven
  corresponding views and inspected side-by-side before/after renders. Restored FM Lab's large
  centered HOME banner, centered editor headers/patch lines, battery/view pips, full-height HOME
  plots, 32-pixel editor rows with amount bars, compact ENV/LFO cells, lilac/white/gray styling and
  subdued legends. SAVE now edits the patch line without replacing the graph; HOME knob values
  appear temporarily below the legend. Layout changes clear obsolete pixels, including unused
  LFO sync cells. Native WT fields, gestures, DSP and storage formats are unchanged.
  Focused layout regressions, MSVC/GCC and ASan/UBSan pass. All 44 new screens match across builds;
  all 15 original audio/flash artifacts are unchanged. Real-target checks pass at 354,052 image
  bytes, 76,472 RAM, 288,760 pool and 6,716 RAM-text. Comparison PNGs/provenance are in
  `build/ui-comparison`. Subsequently packaged and flashed with explicit user approval; independent
  identity and USB checks pass, and the user confirmed BUILD 0.1.9-test and normal revised UI startup.
  Package app/loader exactly match the tested check-only build. Device UI/load testing remains pending.
  Preserved a verified source archive alongside the unchanged `0.1.8-test` working rollback.
- **Selected-output filter optimization, 0.1.8-test:** bypasses filter-mix ramp checks and unused
  output arithmetic when the filter type is settled, caching coefficients when possible and
  preserving their sample-by-sample ramps when cutoff/resonance moves. Filter-type transitions
  retain the general crossfade path. New scalar-reference tests verify samples and complete voice
  state across all filter types, coefficient/type retargets and cutoff/resonance extremes.
  MSVC/GCC/sanitizers pass and all 59 original artifacts remain byte-identical.
  The first iteration, 0.1.7-test, improved F7 to 71% but regressed F5 to 100%; it was not accepted.
  Final user-confirmed 0.1.8-test results: F7 CPU/peak/LATE 56%/68%/0 and F5 52%/82%/0.
  The requested F7 peak below 85% is met without reducing polyphony or changing preset sound.
  Target image 327,356 bytes, RAM 76,216, pool 288,760 and RAM-text 6,716; all safety checks pass.
  Final-build listening and full hardware acceptance remain pending.
- **Latest CPU iteration, 0.1.6-test:** removed a waveform-cache experiment that worsened F3 peak
  from 83% to 90%; retained call-free fast-RAM oscillator/voice kernels. Bit-exact 32-bit sample
  interpolation replaces unnecessary wide arithmetic; added peak-block oscillator/voice/control
  timing alongside SYN/FX. Strict MSVC/GCC, sanitizers and target checks pass; all 59 baseline
  artifacts remain identical. After a host reboot, hashes/CRCs were reverified and the explicitly
  approved update installed successfully. User-confirmed F3 eight-note CPU/peak/LATE:
  66% / 80% / 0 (previously 76% / 90% / 0). Oscillator/voice/control contributions were
  37% / 21% / 2%. Controlled deadlines pass; headroom target, broader workloads and listening
  remain unverified.
- Independent development copy of the current FM Lab source, including its uncommitted improvements.
- WT Lab application name, source/build artifact names, and versioned settings/bank identifiers.
- Check-only firmware builds; packaging requires an explicit identity separate from FM Lab's.
- Implementation plan in `IMPLEMENTATION.md`; steps 1 through 5 and step 6's pre-hardware work are verified.
- Standalone fixed-point wavetable oscillator with sample/frame interpolation, seven smoothly blended
  harmonic levels, phase-preserving live edits, finite pitch/position/gain ramps, and table crossfades.
- Reproducible factory bank: eight tables of eight frames, seven original and one derived from retained
  CC0 AKWF vocal sources, with pinned provenance and licenses.
- Windows/Linux audio and behavior checks, sanitizer validation, a real JieLi link probe, and a
  16-second table-scanning WAV preview.
- **Playable wavetable instrument:** eight voices with two oscillators each, amp and filter/motion
  ADSRs, per-voice resonant filters/tracking, velocity response, and two key-synced/free-running LFOs
  targeting positions, pitch, or cutoff.
- Released-first voice stealing, last-note mono/fallback, phase/envelope-preserving live edits,
  retrigger/steal declicking, overflow-to-panic event handling, and safe key/MIDI note-off ownership.
- Basic WT HOME/EDIT/ENV/LFO/GLO controls and live waveform preview. The FM baseline is archived in
  `fm_baseline.c`; it is not the active instrument.
- Strict voice/application tests, tuning/filter/velocity fixtures, actual panel/MIDI audio checks,
  byte-identical MSVC/GCC audio, full-app sanitizer checks, and successful real JieLi firmware build.
- **Familiar editor:** EDIT/ENV/LFO tap/hold shortcuts/full pages, shared control/legend mappings,
  SELECT object/bank navigation, oscillator selection, SEL fine/mute/solo, and RAM-only A/B swap/copy.
- Neighboring-frame/live-morph previews, envelope/LFO context plots, native fixed-point spectrum and
  output waveform. Immutable audio/UI snapshots keep plotting and FFTs off live audio-owned state.
- Separate global transpose/fine tuning and output preferences, including real bit-depth processing.
  Page/octave/channel changes preserve held-key note ownership, including keys that become shortcuts.
- Focused gesture/ownership tests and a real UI/audio scenario; matching Windows/Linux audio/screens,
  sanitizers, and real target resource/build checks. Nothing packaged or flashed.
- **Complete native workflow:** eight factory sounds, 256 user slots, naming/saving, bank jumps and
  MIDI bank/program selection; A/B includes the full effects chain.
- Fixed-point chorus, phaser, delay and stereo reverb, with main-computed published coefficients and
  audio-owned histories. Post-effects scope/spectrum and real stereo-to-mono routing.
- Sample-clocked arp with latch/pattern/octaves/gate/rate/swing/velocity, plus a 64-note phrase looper
  with count-in, quantized panel/MIDI recording, overdub, step editing/audition and separate note ownership.
- Explicit versioned 172-byte patches / 240-byte settings; native `WTB2` / `WTS2` records reject old FM
  data and malformed fields. Quiet-acknowledged writes verify flash before committing RAM/success.
  Settings and phrase autosave separately; boot never restarts recording or transport.
- All-slot, corruption, write/readback failure, deferred/canceled/snapshot save and real process-restart
  tests. Nine WAVs, 39 screenshots and persisted flash are byte-identical between MSVC/GCC; sanitizers pass.
  Real JieLi build: 322,416 image bytes, 76,168 RAM bytes, 288,760 pool bytes, all within budget.
- Flash addresses still overlap FM data; no dual-data preservation or redundant-bank power-loss rollback.
  Hardware timing/listening and packaging remain step 6; no device was flashed.
- **Pre-hardware validation:** GLO diagnostics for app CPU/peak/deadline misses, audio-owned reset,
  active voices and build. Host explicitly says `NOT DEVICE`; these are not whole-ISR measurements.
- Combined eight-voice/two-oscillator/modulation/filter/four-effects UI/audio workload, ninth-note
  stealing/panic recovery, timing/reset and boot-guard policy tests; repeatable sanitizer runner.
  Eleven native WAVs, 44 screenshots and flash, plus three standalone WAVs, match across MSVC/GCC
  and sanitizer builds.
- WT MIDI bench helper with argument checks, explicit ambiguous-port selection, channel-correct
  cleanup and backend-free dry run. Its pre-hardware tests used dry runs/mocks only.
- Explicit pool/RAM-text/RAM build bounds and deterministic `wtlab-build.json` artifact/source hashes,
  memory/check metadata and package status; failed builds invalidate stale success reports.
  Repeat real-target app/loader/report bytes match. Current target: 323,004 image bytes, 76,184 RAM,
  288,760 pool and 2,924 RAM-text, within all limits.
- Native quick reference and hardware workload/measurement/recovery guide; archived FM map retained.
  Hardware measurements, confirmed identity, installable device-test packaging and release approval
  remain outstanding.
- **First local device test:** user-approved `FM-1_922` / `0.1.0-test`, not a public allocation.
  Brought over FM Lab's two-chip flash allowlist and RAM-only timeout reset into app/rescue/loader;
  strict host/sanitizer/target checks pass and all 59 host artifacts remain byte-identical.
  Real package installed successfully, independently re-queried as `FM-1_922`, with normal USB
  audio/MIDI enumeration. The user confirmed normal WT Lab startup and clean local-key sine/release.
  Package uses 323,436 image bytes, 76,184 RAM, 288,760 pool and 3,308 RAM-text. Full bench acceptance
  and public-release allocation remain pending; a validated FM Lab rollback package is retained.
- **Hardware CPU regression / 0.1.1-test:** original INIT WAVE measured approximately 28/52/77/105%
  app CPU at 1/2/3/4 voices, with distortion; other presets reached 264% peak. Hoisted settled
  oscillator lookup geometry, skipped silent sample reads while retaining all oscillator state,
  inlined hot helpers and bypassed settled filter-mix ramp work. Eight voices and sound behavior
  are unchanged. Exact scalar sample/state tests and MSVC/GCC/sanitizers pass; all 59 original
  artifacts remain byte-identical. Real target image: 325,948 bytes; other memory budgets unchanged.
  User-approved update installed successfully as `FM-1_922`. INIT peak improved to 49%, but controlled
  F3 HOLLOW PAD still reached 132% / 410 LATE with eight notes: performance acceptance failed.
- **0.1.2-test:** cached frame pointers across position modulation/frame crossings, efficient signed
  interpolation products, skipped unused LFO waveform evaluation while preserving phase, hoisted
  settled filter/gain checks and exact block control-envelope advancement. New scalar-state tests
  and MSVC/GCC/sanitizers pass; all 59 original artifacts remain unchanged. Target image: 326,476
  bytes, unchanged RAM/pool/RAM-text. Approved installation/reboot query succeeded; controlled
  hardware performance and listening remain pending.

## Inherited FM Lab history

FM Lab's releases, newest first. Each one is on the
[releases page](https://github.com/jsmith13/fm_lab/releases) with its `.fwsc`; the version a build is shows in
GLOBAL's top-right corner. Versions are `MAJOR.MINOR.PATCH`: a patch fixes what was there, a minor adds to it.

## Unreleased

### Added
- **256 user slots**, as eight banks of 32: `U1-01` to `U8-32`. Bank `U1` is where it was, so the voices
  you have already saved are still there. Hold **SEL** while turning **PRESETS** to jump a bank at a time,
  in the voice list and while picking a slot to save to.
- Bank Select (CC 0) with program change: bank 1–8 and program *N* is slot *N*+1 of `U1`–`U8`; bank 0,
  or none, is the voice list as before. **PRESETS** sends the bank before the program.
- A bulk dump in goes into the bank you are in, and a bank request sends that bank back; the screen
  says which (`BANK 2 RECEIVED`).
- **OUTPUT** (GLOBAL, fifth bank): `STEREO`, or `MONO` for a mono 3.5 mm plug, with left and right summed
  onto both sides so nothing the effects put on the right is lost.

### Changed
- **OVERSAMPLE is now SAMPLE RATE** (GLOBAL, fourth bank): `88.2K` (oversampled, as `ON` was, and where it
  starts), `44.1K` (as `OFF` was), then `32K`, `22K`, `16K`, `11K` and `8K`, which hold the voices at that
  rate, aliasing and all, for the sound of older hardware. A saved `OFF` comes back as `44.1K`.

## 1.1.0 — 2026-10-18

### Added
- **OVERSAMPLE** (GLOBAL, last bank, `ON` to start). The operators run at 88.2 kHz and a half-band filter
  brings the sound back to 44.1 kHz, so the harmonics FM makes above 22 kHz no longer fold back into the
  audible range as tones that don't belong to the note: about 40 dB less of them on a bright patch played
  high. It roughly doubles the engine's CPU: ~72 % (82 % peak) with eight voices, the filter and every
  effect on. `OFF` gives that back.
- **BIT DEPTH** (GLOBAL, last bank): the voices cut to 16, 12 or 8 bits before the filter and the effects,
  for the sound of older hardware. It starts at `24 BIT`, which is FM Lab as before.

### Changed
- **24-bit output.** The sound used to be cut to 16 bits right after the voices were mixed, and only then
  sent to the 24-bit DAC. The mix leaves room for an 8-voice chord, so a single soft note used only ~11 of
  those 16 bits, and its decay ran out of steps and turned gritty (the DC blocker's rounding added a
  distortion that followed the note). The mix, the filter, the effects and MASTER now carry 24 bits to the
  DAC. USB audio stays 16-bit and is dithered. The delay's echoes are still stored at 16 bits, which saves
  128 KiB of RAM.

## 1.0.1 — 2026-10-18

### Fixed
- **The 3.5 mm MIDI IN jack works.** It was switched off in the platform FM Lab is built on, so a keyboard
  or pad controller on the jack played nothing; only USB-MIDI was heard. The jack now feeds the same input
  as USB — the GLOBAL `MIDI CHANNEL` filter and `PROG CHANGE` apply to both. It takes notes and program
  changes; SysEx and MIDI out stay on USB. Through a TRS-to-DIN adapter: if nothing plays, try the other
  type (A / B). The reader is Felucca's, as dxsloop carries it.

## 1.0.0 — 2026-10-18

The first release: a six-operator FM synthesizer for the M-VAVE FM-1, built to learn FM on.

- The DX7 engine (Dexed's, by way of dxsloop): 8 voices, 32 algorithms, feedback, per-operator envelopes,
  keyboard scaling, the LFO and the pitch envelope
- The algorithm drawn live, a spectrum analyser and a scope, and envelopes drawn to scale
- Every parameter under a knob with the sound playing; operator mute, solo and an A/B compare
- Operator waves, a global amp envelope, a filter, and chorus, phaser, delay and reverb
- A small arpeggiator and a one-phrase looper with a step view
- 20 factory voices and 32 user slots; DX7 SysEx in and out; MIDI channel and program change
- The firmware version on the GLOBAL page
## 1.0.0 - first public WT Lab release, 2026-11-15

- Publish the user-accepted 0.5.0-waves firmware source as a version-labelled 1.0.0 rebuild.
- Fourteen eight-frame tables, eight voices/two oscillators, native presets/saving, effects,
  arp/phrase workflows, MIDI Learn and familiar four-page GLOBAL UI.
- Preserve original eight sample tables, storage layout, DSP, cache, word copies, CPU1 batching,
  deadlines and power policy. The published package was not re-flashed during publication.
- Make independent serial-oracle tests self-contained and retain all source/license provenance.
- Record confirmed prototype musical acceptance separately from incomplete exhaustive qualification.

