/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/src/wt_synth.c"

static uint32_t failures;
#define CHECK(test, message) do { if (!(test)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, message); failures++; } } while (0)
static wt_synth_t a, b, c, snapshot;
static int32_t whole[16384], split[16384], third[16384];

static void advance(wt_synth_t *s, uint32_t count)
{
    int32_t samples[256];
    while (count) {
        uint32_t n = count < 256u ? count : 256u;
        CHECK(wt_synth_render(s, samples, n) == 0, "render succeeds");
        count -= n;
    }
}

static uint32_t voices(const wt_synth_t *s)
{
    uint32_t i, n = 0;
    for (i = 0; i < WT_VOICES; i++) n += s->voice[i].active;
    return n;
}

static void envelope_advance_checks(void)
{
    static const uint32_t times[] = {0, 3, 73, 5000};
    uint32_t i, j, k;
    for (i = 0; i < 16u; i++) {
        wt_adsr_t p = {times[i % 4u], times[i / 4u], i * 2184u, times[(i+1u) % 4u]};
        wt_env_t bulk = {0}, scalar;
        wts_env_start(&bulk, &p); scalar = bulk;
        for (j = 0; j < 1000u; j++) {
            uint32_t n = j % (WT_CONTROL_SAMPLES + 1u);
            if (j == 80u) {
                wt_adsr_t old = p;
                p.sustain = WT_GAIN_MAX - p.sustain;
                p.attack = times[(i+2u) % 4u];
                wts_env_edit(&bulk, &old, &p); wts_env_edit(&scalar, &old, &p);
            }
            if (j == 400u) {
                wts_env_release(&bulk, p.release); wts_env_release(&scalar, p.release);
            }
            wts_env_advance(&bulk, &p, n);
            for (k = 0; k < n; k++) wts_env_next(&scalar, &p);
            CHECK(!memcmp(&bulk, &scalar, sizeof bulk),
                  "block envelope advance exactly preserves scalar state across edits and stage boundaries");
        }
    }
}

static int32_t reference_filter(wt_voice_t *v, int32_t x)
{
    int32_t coefficients[4], mix[4], v1, v2, v3, band;
    int64_t y;
    uint32_t i;
    for (i = 0; i < 4u; i++) mix[i] = (int32_t)wt_ramp_next(&v->filter_mix[i]);
    if (mix[0] == (int32_t)WT_GAIN_MAX) {
        v->ic1 = v->ic2 = 0;
        return x;
    }
    for (i = 0; i < 4u; i++) coefficients[i] = (int32_t)wt_ramp_next(&v->coefficient[i]);
    v3 = x - v->ic2;
    v1 = (int32_t)(((int64_t)coefficients[0] * v->ic1 + (int64_t)coefficients[1] * v3) >> 30);
    v2 = v->ic2 + (int32_t)(((int64_t)coefficients[1] * v->ic1 + (int64_t)coefficients[2] * v3) >> 30);
    v->ic1 = wts_clamp(2 * v1 - v->ic1, -1048576, 1048576);
    v->ic2 = wts_clamp(2 * v2 - v->ic2, -1048576, 1048576);
    band = (int32_t)(((int64_t)coefficients[3] * v1) >> 30);
    y = (int64_t)x * mix[0] + (int64_t)v2 * mix[1] +
        (int64_t)band * mix[2] + (int64_t)(x - band - v2) * mix[3];
    return wts_clamp((int32_t)(y >> 15), -524288, 524288);
}

static void settled_filter_checks(void)
{
    static const uint32_t cutoffs[] = {0, 25000, 50000, WT_POSITION_MAX, 65537, WT_CUTOFF_MAX};
    int16_t left[32], right[32];
    int32_t optimized[32], reference[32];
    wt_voice_t fast, scalar;
    uint32_t test, block, i, k;
    for (test = 0; test < 72u; test++) {
        wt_patch_t p;
        wt_synth_init(&a); p = a.patch;
        p.filter_type = test % 4u; p.cutoff = cutoffs[test / 4u % 6u];
        p.resonance = test / 24u * (WT_GAIN_MAX / 2u);
        CHECK(!wt_synth_patch(&a, &p) && !wt_synth_note_on(&a, 60, 127), "filter equivalence setup");
        fast = a.voice[0];
        fast.ic1 = 900000; fast.ic2 = -900000;
        scalar = fast;
        for (block = 0; block < 24u; block++) {
            uint32_t n = block == 0u ? 1u : 32u;
            if (block == 4u || block == 14u) {
                uint32_t type = (test + block) % 4u;
                for (k = 0; k < 4u; k++) {
                    wt_ramp_set(&fast.filter_mix[k], k == type ? WT_GAIN_MAX : 0);
                    wt_ramp_set(&scalar.filter_mix[k], k == type ? WT_GAIN_MAX : 0);
                }
            }
            if (block == 6u || block == 9u) {
                uint32_t cutoff = cutoffs[(test + block) % 6u];
                wts_filter_target(&fast, cutoff, p.resonance);
                wts_filter_target(&scalar, cutoff, p.resonance);
            }
            for (i = 0; i < n; i++) {
                left[i] = (int16_t)(i * 7919u + block * 1237u);
                right[i] = (int16_t)(i * 17389u - block * 937u);
                optimized[i] = reference[i] = 12345;
            }
            wts_voice_render(&fast, &p.amp, left, right, optimized, n);
            for (i = 0; i < n; i++) {
                int32_t x = reference_filter(&scalar, ((int32_t)left[i] + right[i]) / 2);
                int32_t env = wts_env_next(&scalar.amp, &p.amp);
                int32_t vel = (int32_t)wt_ramp_next(&scalar.velocity);
                int32_t tail = (int32_t)(((int64_t)scalar.tail * wt_ramp_next(&scalar.tail_gain)) >> 15);
                int32_t y = (int32_t)(((int64_t)x * env) >> 19);
                y = (int32_t)(((int64_t)y * vel) >> 15) + tail;
                scalar.last_output = wts_clamp(y, -16777216, 16777216);
                reference[i] += scalar.last_output;
            }
            CHECK(!memcmp(optimized, reference, n * sizeof optimized[0]) &&
                  !memcmp(&fast, &scalar, sizeof fast),
                  "settled filters and ramped transitions preserve exact samples and full voice state");
        }
    }
}

static void allocation_checks(void)
{
    wt_patch_t p;
    uint32_t i;
    wt_synth_init(&a); p = a.patch;
    p.amp.sustain = WT_GAIN_MAX; p.amp.release = 500;
    CHECK(wt_synth_patch(&a, &p) == 0, "allocation patch");
    for (i = 60; i < 68u; i++) CHECK(wt_synth_note_on(&a, i, 100) == 0, "chord note");
    advance(&a, 512);
    CHECK(voices(&a) == 8u, "eight held voices");
    CHECK(wt_synth_note_off(&a, 62) == 0, "release a middle voice");
    CHECK(wt_synth_note_on(&a, 80, 100) == 0, "steal a releasing voice");
    CHECK(a.voice[2].note == 80u && a.voice[0].note == 60u, "released-first stealing");
    CHECK(wt_synth_note_off(&a, 62) == 0 && a.voice[2].held, "stolen note-off cannot release its replacement");
    CHECK(wt_synth_note_on(&a, 81, 100) == 0 && a.voice[0].note == 81u, "oldest held voice is next");
    CHECK(wt_synth_note_on(&a, 81, 90) == 0 && voices(&a) == 8u && a.voice[0].vel == 90u,
          "repeated note reuses its voice");
    wt_synth_panic(&a); advance(&a, 32);
    p.amp.release = p.envelope.release = 5000;
    CHECK(wt_synth_patch(&a, &p) == 0, "edit during panic");
    advance(&a, 224);
    CHECK(!voices(&a), "panic clears all voices after the declick ramp");

    wt_synth_init(&a); a.age = 0xFFFFFFFCu;
    for (i = 60; i < 68u; i++) CHECK(wt_synth_note_on(&a, i, 100) == 0, "age-wrap chord");
    CHECK(wt_synth_note_on(&a, 80, 100) == 0 && a.voice[0].note == 80u,
          "voice age wrap preserves oldest-first stealing");
}

static void mono_checks(void)
{
    wt_patch_t p;
    wt_env_t env;
    uint32_t phase, i;
    wt_synth_init(&a); p = a.patch; p.amp.attack = 1000;
    CHECK(wt_synth_patch(&a, &p) == 0 && wt_synth_mono(&a, 1) == 0, "mono setup");
    CHECK(wt_synth_note_on(&a, 60, 100) == 0, "mono first key");
    advance(&a, 500); env = a.voice[0].amp; phase = a.voice[0].osc[0].phase;
    CHECK(wt_synth_note_on(&a, 64, 80) == 0, "mono slur");
    CHECK(a.voice[0].note == 64u && a.held == 2u &&
          a.voice[0].osc[0].phase == phase && !memcmp(&env, &a.voice[0].amp, sizeof env),
          "legato preserves phase and envelope progress");
    CHECK(wt_synth_note_off(&a, 60) == 0 && a.voice[0].note == 64u &&
          !memcmp(&env, &a.voice[0].amp, sizeof env), "releasing a non-top key does not retrigger");
    CHECK(wt_synth_note_on(&a, 67, 110) == 0 && wt_synth_note_off(&a, 67) == 0 &&
          a.voice[0].note == 64u && a.voice[0].vel == 80u, "last-note fallback restores note and velocity");
    CHECK(wt_synth_note_off(&a, 64) == 0 && !a.held && !a.voice[0].held, "last mono key releases");
    advance(&a, 10000); CHECK(!voices(&a), "mono release finishes");
    for (i = 0; i < 128u; i++) CHECK(wt_synth_note_on(&a, i, 100) == 0, "full MIDI mono stack");
    CHECK(a.held == 128u && wt_synth_note_on(&a, 0, 90) == 0 && a.held == 128u,
          "all 128 keys fit, and duplicates do not grow the stack");
    for (i = 0; i < 128u; i++) CHECK(wt_synth_note_off(&a, i) == 0, "full stack release");
    advance(&a, 10000); CHECK(!a.held && !voices(&a), "full mono stack leaves no stuck notes");
}

static void edit_checks(void)
{
    wt_patch_t p, invalid;
    wt_env_t env;
    uint32_t i, phase;
    wt_synth_init(&a); p = a.patch; p.amp.attack = 1000;
    CHECK(wt_synth_patch(&a, &p) == 0 && wt_synth_note_on(&a, 60, 100) == 0, "live-edit setup");
    advance(&a, 11025); env = a.voice[0].amp; phase = a.voice[0].osc[0].phase;
    p.osc[0].table = 7; p.osc[0].position = WT_POSITION_MAX; p.osc[0].semitone = 12;
    p.osc[1].gain = 20000; p.filter_type = WT_FILTER_LP; p.filter_env = -20000;
    CHECK(wt_synth_patch(&a, &p) == 0 && a.voice[0].osc[0].phase == phase &&
          !memcmp(&env, &a.voice[0].amp, sizeof env), "ordinary edits never reset phase or envelope");
    p.amp.attack = 2000;
    CHECK(wt_synth_patch(&a, &p) == 0 && a.voice[0].amp.value == env.value &&
          abs((int)a.voice[0].amp.left - (int)(2u * env.left)) <= 16,
          "editing attack preserves its level and fractional time progress");
    snapshot = a;
    for (i = 0; i < 24u; i++) {
        invalid = p;
        switch (i) {
        case 0: invalid.osc[0].table = WT_TABLE_COUNT; break;
        case 1: invalid.osc[1].position = WT_POSITION_MAX + 1u; break;
        case 2: invalid.osc[0].gain = WT_GAIN_MAX + 1u; break;
        case 3: invalid.osc[1].semitone = 25; break;
        case 4: invalid.osc[0].fine = -257; break;
        case 5: invalid.osc[0].position_env = -65537; break;
        case 6: invalid.amp.attack = 5001; break;
        case 7: invalid.amp.decay = 5001; break;
        case 8: invalid.amp.sustain = WT_GAIN_MAX + 1u; break;
        case 9: invalid.amp.release = 5001; break;
        case 10: invalid.envelope.release = 5001; break;
        case 11: invalid.filter_type = 4; break;
        case 12: invalid.cutoff = WT_CUTOFF_MAX + 1u; break;
        case 13: invalid.resonance = WT_GAIN_MAX + 1u; break;
        case 14: invalid.keytrack = WT_GAIN_MAX + 1u; break;
        case 15: invalid.velocity = WT_GAIN_MAX + 1u; break;
        case 16: invalid.filter_env = 65537; break;
        case 17: invalid.lfo[0].rate = 20001; break;
        case 18: invalid.lfo[1].shape = WT_LFO_SHAPES; break;
        case 19: invalid.lfo[0].depth = WT_GAIN_MAX + 1u; break;
        case 20: invalid.lfo[1].destination = WT_MOD_DESTS; break;
        case 21: invalid.lfo[0].key_sync = 2; break;
        case 22: invalid.osc[1].semitone = -25; break;
        default: invalid.osc[0].fine = 257; break;
        }
        CHECK(wt_synth_patch(&a, &invalid) == -1 && !memcmp(&a, &snapshot, sizeof a),
              "invalid patch is rejected atomically");
    }
    CHECK(wt_synth_note_on(&a, 128, 100) == -1 && wt_synth_note_on(&a, 60, 128) == -1 &&
          wt_synth_note_off(&a, 128) == -1 && wt_synth_mono(&a, 2) == -1 &&
          !memcmp(&a, &snapshot, sizeof a), "invalid note/mode inputs are atomic");

    wt_synth_init(&a); p = a.patch;
    p.amp.attack = p.amp.decay = 0; p.amp.sustain = WT_GAIN_MAX;
    CHECK(wt_synth_patch(&a, &p) == 0 && wt_synth_note_on(&a, 69, 127) == 0, "declick setup");
    advance(&a, 1000);
    for (i = 0; i < 256u && a.voice[0].last_output < 150000; i++) advance(&a, 1);
    CHECK(a.voice[0].last_output >= 150000, "find a nonzero waveform peak");
    phase = (uint32_t)a.voice[0].last_output;
    CHECK(wt_synth_note_on(&a, 69, 127) == 0, "retrigger at a nonzero waveform peak");
    advance(&a, 1);
    CHECK(abs(a.voice[0].last_output - (int32_t)phase) < 20000, "retrigger fades the old sample instead of cutting it");
    advance(&a, 512);
    p.amp.sustain = 16384;
    env = a.voice[0].amp;
    CHECK(wt_synth_patch(&a, &p) == 0 && a.voice[0].amp.value == env.value, "live sustain edit starts continuously");
    advance(&a, 128);
    CHECK(a.voice[0].amp.value == WT_ENV_ONE / 2, "live sustain reaches its target");
}

static void partition_render(wt_synth_t *s, int32_t *out, uint32_t count)
{
    uint32_t offset = 0;
    while (offset < count) {
        uint32_t n = offset % 113u + 1u;
        if (n > count - offset) n = count - offset;
        CHECK(wt_synth_render(s, out + offset, n) == 0, "partitioned render");
        offset += n;
    }
}

static void filter_and_block_checks(void)
{
    wt_patch_t p;
    uint32_t i;
    wt_synth_init(&a); p = a.patch;
    p.osc[0].position = 48000; p.osc[1].table = 7; p.osc[1].gain = 20000;
    p.filter_type = WT_FILTER_LP; p.cutoff = 25000; p.resonance = 15000;
    p.filter_env = 30000; p.keytrack = WT_GAIN_MAX;
    CHECK(wt_synth_patch(&a, &p) == 0, "per-voice filter setup");
    b = c = a;
    CHECK(wt_synth_note_on(&a, 60, 100) == 0 && wt_synth_note_on(&a, 72, 100) == 0 &&
          wt_synth_note_on(&b, 60, 100) == 0 && wt_synth_note_on(&c, 72, 100) == 0, "filter chord");
    CHECK(wt_synth_render(&a, whole, 8192) == 0 && wt_synth_render(&b, split, 8192) == 0 &&
          wt_synth_render(&c, third, 8192) == 0, "filtered individual voices");
    for (i = 0; i < 8192u; i++)
        CHECK(whole[i] == split[i] + third[i], "chord equals sum of independent per-note filters");
    CHECK(a.voice[0].cutoff != a.voice[1].cutoff &&
          (a.voice[0].ic1 != a.voice[1].ic1 || a.voice[0].ic2 != a.voice[1].ic2),
          "key tracking and filter histories are per voice");
    CHECK(wt_synth_note_on(&a, 67, 100) == 0 && a.voice[2].envelope.value < a.voice[0].envelope.value,
          "a new note has its own filter envelope");

    p.lfo[0].destination = WT_MOD_POS1; p.lfo[0].depth = 14000;
    p.lfo[1].destination = WT_MOD_FILTER; p.lfo[1].depth = 18000;
    p.lfo[1].shape = WT_LFO_RANDOM; p.lfo[1].key_sync = 0; p.lfo[1].rate = 20000;
    wt_synth_init(&a);
    CHECK(wt_synth_patch(&a, &p) == 0 && wt_synth_note_on(&a, 60, 100) == 0 &&
          wt_synth_note_on(&a, 67, 110) == 0, "modulated partition setup");
    b = a;
    CHECK(wt_synth_render(&a, whole, 16384) == 0, "whole modulated block");
    partition_render(&b, split, 16384);
    CHECK(!memcmp(whole, split, sizeof whole), "modulation/filter/envelopes are render-partition invariant");
    CHECK(wt_synth_note_off(&a, 60) == 0 && wt_synth_note_off(&b, 60) == 0, "partitioned release");
    CHECK(wt_synth_render(&a, whole, 16384) == 0, "whole release");
    partition_render(&b, split, 16384);
    CHECK(!memcmp(whole, split, sizeof whole), "overlapping release is render-partition invariant");
}

static void modulation_checks(void)
{
    wt_patch_t p;
    uint32_t d, i;
    for (d = WT_MOD_POS1; d < WT_MOD_DESTS; d++) {
        wt_synth_init(&a); p = a.patch;
        p.osc[0].position = p.osc[1].position = 10000;
        p.lfo[0].shape = WT_LFO_SQUARE; p.lfo[0].rate = 0;
        p.lfo[0].depth = WT_GAIN_MAX; p.lfo[0].destination = d;
        CHECK(wt_synth_patch(&a, &p) == 0 && wt_synth_note_on(&a, 60, 100) == 0, "modulation destination");
        advance(&a, 160);
        if (d == WT_MOD_POS1 || d == WT_MOD_BOTH) CHECK(a.voice[0].position[0] == 65536, "LFO reaches oscillator 1");
        if (d == WT_MOD_POS2 || d == WT_MOD_BOTH) CHECK(a.voice[0].position[1] == 65536, "LFO reaches oscillator 2");
        if (d == WT_MOD_PITCH) CHECK(a.voice[0].osc[0].increment.current > (int32_t)wt_pitch_increment(71 * 256), "LFO reaches pitch");
        if (d == WT_MOD_FILTER) CHECK(a.voice[0].cutoff == WT_CUTOFF_MAX, "LFO reaches extended cutoff ceiling");
    }
    wt_synth_init(&a); p = a.patch;
    p.osc[0].position = 32768; p.lfo[0].destination = p.lfo[1].destination = WT_MOD_POS1;
    p.lfo[0].shape = WT_LFO_SQUARE; p.lfo[1].shape = WT_LFO_SAW;
    p.lfo[0].rate = p.lfo[1].rate = 0; p.lfo[0].depth = p.lfo[1].depth = WT_GAIN_MAX;
    CHECK(wt_synth_patch(&a, &p) == 0 && wt_synth_note_on(&a, 60, 100) == 0, "two-LFO sum");
    CHECK(abs(a.voice[0].position[0] - 32768) <= 2, "both LFO contributions are summed");
    for (i = 0; i < 2u; i++) {
        wt_synth_init(&a); p = a.patch;
        p.lfo[0].key_sync = i; p.lfo[0].destination = WT_MOD_POS1; p.lfo[0].depth = 10000;
        CHECK(wt_synth_patch(&a, &p) == 0 && wt_synth_note_on(&a, 60, 100) == 0, "LFO sync setup");
        advance(&a, 2205);
        CHECK(wt_synth_note_on(&a, 72, 100) == 0, "second LFO note");
        if (i) CHECK(wts_lfo(&a, &a.voice[1], 0) == 0 && wts_lfo(&a, &a.voice[0], 0) > 32000,
                     "key sync restarts only the new note's LFO");
        else CHECK(wts_lfo(&a, &a.voice[1], 0) == wts_lfo(&a, &a.voice[0], 0),
                   "free LFO follows the instrument clock");
    }
    wt_synth_init(&a); p = a.patch;
    p.osc[0].position = p.osc[1].position = p.cutoff = 32768;
    p.osc[0].position_env = 30000; p.osc[1].position_env = -30000; p.filter_env = -30000;
    p.envelope.attack = 100;
    CHECK(wt_synth_patch(&a, &p) == 0 && wt_synth_note_on(&a, 60, 100) == 0, "motion envelope setup");
    advance(&a, 2205);
    CHECK(a.voice[0].position[0] > 32768 && a.voice[0].position[1] < 32768 && a.voice[0].cutoff < 32768,
          "bipolar envelope depths move positions and cutoff independently");
}

static void stress_checks(void)
{
    wt_patch_t p;
    uint32_t i, j;
    wt_synth_init(&a);
    for (i = 0; i < 8u; i++) CHECK(wt_synth_note_on(&a, i & 1u ? 127u - i : i, 127) == 0, "edge-pitch chord");
    for (i = 0; i < 40u; i++) {
        p = a.patch;
        p.filter_type = i % 4u; p.cutoff = i & 1u ? 0u : WT_CUTOFF_MAX; p.resonance = WT_GAIN_MAX;
        p.osc[0].table = i % WT_TABLE_COUNT; p.osc[1].table = (i + 3u) % WT_TABLE_COUNT;
        p.osc[0].position = i & 1u ? 1u : WT_POSITION_MAX;
        p.osc[1].position = WT_POSITION_MAX - p.osc[0].position;
        p.osc[1].gain = WT_GAIN_MAX; p.osc[0].semitone = i & 1u ? -24 : 24;
        p.filter_env = i & 1u ? -65536 : 65536;
        for (j = 0; j < 2u; j++) {
            p.lfo[j].shape = (i + j) % WT_LFO_SHAPES; p.lfo[j].destination = (i + j) % WT_MOD_DESTS;
            p.lfo[j].rate = 20000; p.lfo[j].depth = WT_GAIN_MAX; p.lfo[j].key_sync = i & 1u;
        }
        CHECK(wt_synth_patch(&a, &p) == 0, "extreme patch remains valid");
        CHECK(wt_synth_render(&a, whole, 511) == 0, "extreme filter/modulation render");
        for (j = 0; j < 511u; j++) CHECK(whole[j] >= -8388607 && whole[j] <= 8388607, "Q23 output limiter stays bounded");
    }
    wt_synth_panic(&a);
    CHECK(wt_synth_render(&a, whole, 512) == 0 && !voices(&a), "extreme modulation still obeys panic");
    for (j = 256; j < 512u; j++) CHECK(whole[j] == 0, "panic leaves exact silence");
}

static void put16(FILE *f, uint32_t value)
{
    fputc(value & 255u, f); fputc((value >> 8) & 255u, f);
}
static void put32(FILE *f, uint32_t value) { put16(f, value); put16(f, value >> 16); }

static void audio_checks(const char *path)
{
    static int16_t samples[10u * WT_SAMPLE_RATE];
    int32_t block[256];
    wt_patch_t p;
    uint32_t segment, offset, n, i;
    FILE *f;
    for (segment = 0; segment < 10u; segment++) {
        uint32_t note = 69, vel = segment == 4u ? 32u : 127u;
        wt_synth_init(&a); p = a.patch;
        p.amp.attack = p.amp.decay = 0; p.amp.sustain = WT_GAIN_MAX;
        if (segment == 1u) p.osc[0].semitone = 12;
        if (segment == 2u || segment == 3u) {
            p.osc[0].gain = 0; p.osc[1].gain = WT_GAIN_MAX;
            p.osc[1].semitone = segment == 2u ? -12 : 0;
            p.osc[1].fine = segment == 3u ? 128 : 0;
        }
        if (segment >= 6u) {
            note = segment < 8u ? 96u : 60u; p.cutoff = 38353;
            p.filter_type = segment == 6u ? WT_FILTER_LP : segment == 8u ? WT_FILTER_HP : WT_FILTER_OFF;
        }
        CHECK(wt_synth_patch(&a, &p) == 0 && wt_synth_note_on(&a, note, vel) == 0, "audio fixture setup");
        for (offset = 0; offset < WT_SAMPLE_RATE; offset += n) {
            n = WT_SAMPLE_RATE - offset; if (n > 256u) n = 256u;
            CHECK(wt_synth_render(&a, block, n) == 0, "audio fixture render");
            for (i = 0; i < n; i++) samples[segment * WT_SAMPLE_RATE + offset + i] = (int16_t)(block[i] >> 8);
        }
    }
    f = fopen(path, "wb"); if (!f) { perror(path); exit(1); }
    fwrite("RIFF", 1, 4, f); put32(f, 36u + sizeof samples); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 1); put32(f, WT_SAMPLE_RATE); put32(f, WT_SAMPLE_RATE * 2u);
    put16(f, 2); put16(f, 16); fwrite("data", 1, 4, f); put32(f, sizeof samples);
    for (i = 0; i < 10u * WT_SAMPLE_RATE; i++) put16(f, (uint16_t)samples[i]);
    if (ferror(f)) { perror(path); fclose(f); exit(1); }
    if (fclose(f)) { perror(path); exit(1); }
    printf("voice audio -> %s\n", path);
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: wt_synth_host OUT.wav\n"); return 1; }
    envelope_advance_checks();
    settled_filter_checks();
    allocation_checks(); mono_checks(); edit_checks(); filter_and_block_checks(); modulation_checks(); stress_checks();
    audio_checks(argv[1]);
    printf("WT synth checks: %u failures\n", failures);
    return failures ? 1 : 0;
}
