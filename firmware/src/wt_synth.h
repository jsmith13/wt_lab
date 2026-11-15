/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef WT_SYNTH_H
#define WT_SYNTH_H
#include "wt_osc.h"

#define WT_VOICES 8u
#define WT_CONTROL_SAMPLES 32u
#define WT_ENV_ONE 16777216
#define WT_NAME_LEN 12u
/* Keep the original 0..65536 cutoff grid; five extra knots extend 16 kHz to 20 kHz. */
#define WT_CUTOFF_MAX 68096u
enum { WT_ENV_OFF, WT_ENV_ATTACK, WT_ENV_DECAY, WT_ENV_SUSTAIN, WT_ENV_RELEASE };
enum { WT_FILTER_OFF, WT_FILTER_LP, WT_FILTER_BP, WT_FILTER_HP };
enum { WT_LFO_SINE, WT_LFO_TRIANGLE, WT_LFO_SAW, WT_LFO_SQUARE, WT_LFO_RANDOM, WT_LFO_SHAPES };
enum { WT_MOD_NONE, WT_MOD_POS1, WT_MOD_POS2, WT_MOD_BOTH, WT_MOD_PITCH, WT_MOD_FILTER, WT_MOD_DESTS };

/* Times: 0..5000 ms, with a 128-sample fast minimum. Sustain: Q15, 0..32768. */
typedef struct { uint32_t attack, decay, sustain, release; } wt_adsr_t;
typedef struct {
    uint32_t table, position, gain;
    int32_t semitone, fine, position_env;       /* fine: semitone Q8; envelope depth: bipolar Q16 */
} wt_osc_patch_t;
typedef struct { uint32_t rate, shape, depth, destination, key_sync; } wt_lfo_patch_t; /* mHz; depth Q15 */
typedef struct {
    wt_osc_patch_t osc[2];
    wt_adsr_t amp, envelope;
    uint32_t filter_type, cutoff;              /* cutoff: legacy grid plus extension, 0..WT_CUTOFF_MAX */
    uint32_t resonance, keytrack, velocity;     /* normalized Q15, 0..32768 */
    int32_t filter_env;                        /* bipolar normalized Q16 */
    wt_lfo_patch_t lfo[2];
    uint8_t fx[16];                            /* mix/rate/depth/width, phaser, delay, reverb: 0..99 */
    char name[WT_NAME_LEN];
} wt_patch_t;

typedef struct {
    int32_t value, target, step, remainder, error;
    uint32_t stage, left, span, divisor;
} wt_env_t;
typedef struct {
    wt_osc_t osc[2];
    wt_env_t amp, envelope;
    wt_ramp_t velocity, coefficient[4], filter_mix[4], tail_gain;
    int32_t ic1, ic2, tail, last_output, position[2], cutoff;
    uint32_t phase[2], random[2], age, note, vel, active, held, killed, sustained;
} wt_voice_t;
typedef struct {
    wt_patch_t patch;
    wt_voice_t voice[WT_VOICES];
    uint32_t age, mono, held, control_left, phase[2], random[2], fault, sustain;
    uint8_t notes[128], velocities[128];
    int32_t tuning;                            /* instrument-wide Q8 semitones, outside the patch */
    int32_t controller_pitch, controller_vibrato;
} wt_synth_t;

void wt_patch_init(wt_patch_t *patch);
int wt_patch_valid(const wt_patch_t *patch);
/* Display conversion for cutoff; preserves the legacy grid and clamps to WT_CUTOFF_MAX. */
uint32_t wt_filter_hz(uint32_t position);
/* One serialized audio owner calls all synth functions; no allocation or hardware access. */
void wt_synth_init(wt_synth_t *synth);
int wt_synth_patch(wt_synth_t *synth, const wt_patch_t *patch);
int wt_synth_note_on(wt_synth_t *synth, uint32_t note, uint32_t velocity);
int wt_synth_note_off(wt_synth_t *synth, uint32_t note);
int wt_synth_mono(wt_synth_t *synth, uint32_t mono);
int wt_synth_tune(wt_synth_t *synth, int32_t semitone, int32_t fine);
void wt_synth_panic(wt_synth_t *synth);
void wt_synth_sustain(wt_synth_t *synth, uint32_t down);
void wt_synth_all_notes_off(wt_synth_t *synth);
void wt_synth_silence(wt_synth_t *synth);
/* Overwrites mono Q23 output with eight-voice headroom; nonzero reports an internal DSP fault. */
int wt_synth_render(wt_synth_t *synth, int32_t *out, uint32_t count);
#endif
