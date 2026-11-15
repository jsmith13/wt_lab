# WT Lab quick reference

WT Lab 1.0.0 native wavetable firmware; not the archived FM control map.
The installed prototype has user musical acceptance; exhaustive hardware qualification remains pending.
HOME returns from an editor or shortcut; the bottom strip names the four knobs.

HOME has a large centered title and side-by-side oscillator previews headed by their wavetable names;
a knob edit replaces the
patch name with its label/value for 1.5 seconds. Full editors show parameter amounts as lilac bars, with compact four-knob cells
under ENV/LFO graphs. The patch name sits below the HOME title or above the editor legend.
The selected oscillator's heading is accented; silent/muted oscillators are identified.
Each panel has an eight-tick POS bar (bright live position, dim base position) and a LVL bar.
LVL shows the configured oscillator gain, not a signal meter; muting dims it without losing the setting.
Exact frame/blend numbers remain in the full EDIT screen. Charging fills three gray battery bars
left-to-right, then repeats.

## Knobs and buttons

**MIDI Learn:** hold **GLO** 400 ms.
**SELECT** picks a slot; another **GLO** press advances it. **PRESETS** picks a destination
group; **ALGORITHM** picks its target. Knobs 1..4 set **SOURCE / DEPTH / POLAR / BEND**.
**SEL** arms/cancels Learn; move a CC or channel-pressure controller.
**SAVE** clears only the source, retaining the other settings; **HOME** exits.
`*` means the global mapping is still waiting for a quiet settings save. See [MIDI.md](MIDI.md).

| Control | Action |
| --- | --- |
| PRESETS | Eight factory sounds, then U1-01 through U8-32. SEL + PRESETS jumps banks. Changes under held notes. |
| SELECT | Wavetable/scope HOME, including EDIT state: mix/position or detune/PENV bank. Other shortcuts: envelope, LFO or ARP bank. FX MIX and SEQ are fixed. Full editor: parameter bank. Step view: cursor. |
| ALGORITHM | Selected oscillator's table, cyclic in either direction (GRIT wraps to SINE-SAW). During SAVE: name character. |
| Knobs 1..4 | The four labeled parameters. SEL held makes continuous edits finer. |
| HOME | Leave the editor/shortcut. Repeated on plain HOME: wavetable, spectrum, output waveform. During SAVE: cancel. |
| EDIT | Tap enters EDIT state without changing the current HOME plot or knobs; OP1/OP2, MONO/POLY and high-F-sharp A/B become shortcuts. HOME leaves the state without changing the view. Hold 400 ms for the full editor. |
| ENV / LFO / FX | Tap for shortcut controls without changing the current HOME plot; hold 400 ms for the full editor. ENV starts with AMP ADSR; SELECT or another tap switches AMP/motion ADSR. LFO keeps the last selected LFO, exposing rate/shape/depth/destination; SELECT or another tap switches LFO 1/2. FX MIX is fixed. |
| GLO | Tap: global editor; SELECT or another tap advances its four banks. Hold 400 ms: MIDI assignments / Learn. |
| SEL | In oscillator contexts: tap mute, hold 400 ms solo, repeat hold restore both. Turning/navigating while held consumes the gesture. |
| OCT- / OCT+ | Shift local keys up to two octaves either way; note-off retains its original pitch/channel. That direction lights steadily at one octave, slowly pulses bright/dim at two, and goes off at zero. |
| SAVE | Name/slot in the patch line, with the current letter underlined; SELECT letter, ALGORITHM character, PRESETS slot. First three knobs duplicate these. SAVE again queues a snapshot. |
| ARP | First press borrows HOME knobs without changing its plot; another toggles the arp. SELECT switches the two banks, remembered when returning. Titles are ARP 1/2 and ARP 2/2. |
| SEQ | Tap borrows HOME knobs without changing its plot: TEMPO / BARS / DIV / CLICK, titled SEQ. SELECT/repeated taps keep this fixed bank. Hold for the step editor. |
| REC | From stopped: one-bar count-in, then one pass. Under PLAY: overdub. While recording: end. Hold 400 ms: clear/stop phrase. |
| PLAY | Start/stop phrase; empty phrases report `NOTHING RECORDED`. Dim light means a phrase is available; a bright beat flash follows playback/count-in. Stopping leaves the phrase's dim light on. |

Operator-labeled black keys OP1/OP2 select oscillators only in the EDIT shortcut/editor or ENV
position-routing bank. OP3..OP6 remain notes. MONO/POLY select last-note mono/eight-voice poly in
those same contexts. F#5 is tap A/B swap / hold A/B copy in sound/global editors and sound shortcuts.
On plain HOME, ARP, SEQ and the step view it remains a note. A/B includes effects/name, not mute
masks, device preferences or the phrase.

## Four-knob banks

| View | Bank | Knobs 1 / 2 / 3 / 4 |
| --- | --- | --- |
| Wavetable/scope HOME | Mix/position | LVL 1 / POS 1 / LVL 2 / POS 2 |
| Wavetable/scope HOME | Detune/motion | DET 1 / PENV 1 / DET 2 / PENV 2 |
| Full EDIT | Primary | Table / position / level / semitone |
| Full EDIT | Fine | Fine cents / envelope-to-position / unused / unused |
| ENV | Amp | Attack / decay / sustain / release |
| ENV | Motion/filter | Attack / decay / sustain / release |
| ENV | Routing | Envelope-to-position 1 / position 2 / cutoff / key tracking |
| LFO 1 or 2 | Primary | Rate / shape / depth / destination |
| LFO 1 or 2 | Sync | Key sync / unused / unused / unused |
| FX shortcut | FX MIX | Chorus mix / phaser mix / delay mix / reverb mix |
| FX chorus | | Mix / rate / depth / stereo width |
| FX phaser | | Mix / rate / depth / feedback |
| FX delay | | Mix / time / feedback / tone |
| FX reverb | | Mix / decay / damp / pre-delay |
| GLO | 1 | Transpose / master fine / MIDI receive channel / mono-poly |
| GLO | 2 | Cutoff / resonance / filter type / key tracking |
| GLO | 3 | Velocity response / sample rate / bit depth / stereo-mono |
| GLO | 4 | Program transmission / app CPU (read-only) / app peak (read-only) / build (read-only) |
| ARP | 1 | TEMPO / PATTERN / OCTAVE / LATCH |
| ARP | 2 | GATE / RATE / SWING / VELO |
| SEQ | | TEMPO / BARS / DIV / CLICK |
| STEP | | Length at cursor / tempo / bars / quantize |
| HOME spectrum | | Cutoff / resonance / filter type / frequency span |

Full LFO SELECT cycles primary/sync for LFO 1, then primary/sync for LFO 2. ENV/LFO shortcuts show
the selected object's primary bank. The initial sound is oscillator 1's sine; oscillator 2 starts silent.
Turning a paired HOME control selects/highlights its oscillator, so ALGORITHM follows that side.
DETUNE combines semitone/fine offsets without changing the saved patch format: ordinary turns step
one semitone; SEL-held turns step 1/256 semitone (about 0.39 cents). Feedback shows signed semitones
to three decimals, e.g. `+12.051ST`. The combined legacy range remains +/-25 semitones.
PENV is the bipolar motion/filter-envelope depth into that oscillator's position: positive scans
toward the end, negative toward the start, zero disables it. It is not a volume envelope.
The endpoints clamp rather than wrap. Short EDIT retains these assignments, or filter/span controls
on spectrum HOME; its title is EDIT WT 1 or EDIT WT 2. The selected OP1/OP2 and MONO/POLY
keys light in oscillator edit contexts; ordinary note lights return when leaving them.
Short ENV keeps the current dual-oscillator/spectrum/scope HOME plot and assigns ATK/DEC/SUS/REL.
Each new shortcut session starts at AMP; SELECT switches to motion/filter ADSR and back.
The heading identifies ENV AMP or ENV FILTER. Hold ENV for the unchanged graph/routing editor.
Short FX uses the FX MIX title and changes the bottom shortcuts, keeping the current
dual-oscillator/spectrum/scope HOME view. Its four assignments do not cycle with SELECT or
another short FX press. Hold FX to enter
the detailed banks, then SELECT or tap FX to change effects. Effect amounts and chorus width
display unitless 0..100; physical rate/time suffixes directly follow their values, e.g. `0.56HZ`.
Spectrum HOME overlays the base filter response, played note and harmonic ruler. TYPE wraps in
either direction; CUT switches to kHz above 1000 Hz and resonance feedback is `RESO 25`.
Fresh span is 0-5K; existing saved spans survive updates, so turn SPAN to 5K once if necessary.
Scope shows the played note on the left and its actual window duration on the right.
Short ARP/SEQ also preserve the current HOME plot. ARP remembers its bank independently of
other editors; its button light indicates on/off, not just menu focus. The SEQ button lights
in its shortcut or step editor. PLAY/REC behavior is unchanged, and the SEQ title remains fixed
while playing or recording; those states are shown by their buttons and patch-line feedback.

GLO page 3 has VELOCITY / SAMPLE RATE / BIT DEPTH / OUTPUT; ENV > CUT remains in ENV routing.
Clockwise turns raise output quality: 24 BIT and 44.1K have full bars, 8 BIT and 8K empty bars.
Rates are 8,000 / 11,025 / 16,000 / 22,050 / 32,000 / 44,100 Hz. Lower rates hold the voice
signal before effects for intentional lo-fi aliasing; the synth, envelopes, filters and effects
still run at 44.1 kHz. This is not CPU-saving rendering or 88.2 kHz oversampling.

Existing preferences load unchanged at 44.1K. Reduced-rate preferences use settings schema 2,
which older firmware cannot restore (including the saved phrase); sound banks are unchanged.
**Before rollback, return to 44.1K and wait for quiet autosave (CFG* clears), or retain a settings backup.**
Do not overwrite rejected settings from an older build. Native-rate saves use the original schema 1.

## Sound and performance

Eight tables: SINE-SAW, ODD, PULSE, FORMANT, ORGAN, GLASS, HOLLOW, AKWF VOX. Each has eight morphing
frames and harmonic-limited playback. Each voice has two oscillators, amp ADSR, motion/filter ADSR
and its own filter. The two LFOs can target either/both positions, pitch or cutoff.

Oscillators tune +/-24 semitones and +/-100 cents; ADSRs span about 3 ms..5 s, and LFOs 0..20 Hz.
Cutoff spans 20 Hz..20 kHz; the old dial positions through 16 kHz retain their frequencies.
**Saving above 16 kHz makes that entire 32-slot bank unreadable by older WT firmware.**
Back up before rollback; do not overwrite that bank from an older build.
Filter types are off/low/band/high pass. Envelope-to-position/cutoff routes
are bipolar; turning one below zero reverses that movement. Filter resonance and bright/high notes can
get loud: start with MASTER low.

ARP supports UP, DOWN, UP/DOWN, RANDOM and PLAYED, one to four octaves, latch, six note rates,
gate and swing. The phrase stores up to 64 notes over one/two/four bars, with 1/16, 1/8 or 1/4
quantization. In the step view, keys toggle and audition notes at the cursor. LENGTH edits all notes
there. Moving the cursor briefly auditions a populated step; PLAY/leaving stops cursor audition.
Both local keys and incoming MIDI feed arp/recording.

## Save and MIDI

Names have 11 visible characters. SAVE snapshots oscillators, envelopes, filter, LFOs, effects and
name; later edits do not change that snapshot. Writes wait for released keys, stopped transport,
unlatched/no arp chord, inactive voices and quiet effect tails. `PATCH SAVED` means readback verified.
HOME cancels before writing. A sound asterisk means unsaved edits; `CFG*` means pending settings.

Settings/phrase autosave separately after two unchanged seconds and quiet. Stop PLAY and release
notes before powering off. Boot restores saved sound/settings/phrase, never running transport,
recording, latch or unsaved sound edits. Temporary oscillator mutes do not change saved levels.

MIDI CC 0 bank 0 + zero-based programs 0..7 selects factory sounds. Banks 1..8 + programs 0..31
select user slots. Incoming messages are not echoed. GLO's PROGRAM OUT controls PRESETS transmission,
not incoming programs. Velocity-zero note-ons release; CC 120/123 panic. DX7/table imports and native
bulk backup/export are not supported. The firmware updater remains separate.

**Back up before changing firmware.** WT and FM use the same physical data regions. A power cut during
an in-place save can invalidate all 32 slots in that bank; a cut settings write can lose preferences
and the phrase. There is no redundant-bank rollback.

## Device-test diagnostics

GLO bank 4 shows app CPU/peak as a percentage of the 256-frame, 44.1 kHz deadline (~5.80 ms), active
voices and LATE count. SEL requests peak/LATE reset, applied by audio on its next block. These time
the app callback, not all USB/platform/mastering work. Host builds explicitly display `NOT DEVICE`;
host timings must not be used to approve polyphony/headroom. See [the bench guide](HARDWARE-TESTS.md).
Long build versions end in `...` on screen; the build report retains the complete version.
