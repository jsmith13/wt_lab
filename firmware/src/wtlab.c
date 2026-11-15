/* SPDX-License-Identifier: GPL-3.0-only */
/* WT Lab: audio-owned voices, main-owned editor, published controls and immutable display captures.
 * The FM reference remains in fm_baseline.c; sounds and device preferences have separate storage. */
#define APP_NAME "WT LAB"
#include "platform.c"
#ifndef DINGHY_HOST
#define WT_DSP_CODE __attribute__((section(".ram_text"), noinline))
#define WT_DSP_TICKS() fm1_ticks()
#elif defined(WT_PROFILE_TEST)
#define WT_DSP_TICKS() fm1_ticks()
#endif
#include "wt_synth.c"
#include "wt_fx.c"
#include "wt_midi.c"

static int32_t wt_spectrum_sine(int32_t phase)
{
    return wt_read(WT_LFO_SIN, (uint32_t)phase << 8) * 512;
}
#define SP_SIN_Q24 wt_spectrum_sine
#include "spectrum.h"

enum { EV_ON = 1, EV_OFF, EV_MONO, EV_PANIC, EV_SUSTAIN, EV_RELEASE_ALL, EV_SILENCE };
typedef struct { uint8_t kind, note, velocity, source; } wt_event_t;
#define WT_SAMPLE_RATE_COUNT 6u
static const uint32_t WT_SAMPLE_RATES[WT_SAMPLE_RATE_COUNT] = {44100,32000,22050,16000,11025,8000};
typedef struct { int32_t transpose, fine; uint32_t bits, output_mono, pc_send, sample_rate; } wt_preferences_t;
typedef struct { wt_patch_t patch; wt_preferences_t preferences; fx_t effects; } wt_publication_t;
typedef struct {
    uint32_t render, wait, fx, other, osc, voice, control, late;
    uint32_t total, events, prep, job, output, view, rest, fx_config;
    uint32_t cache_hits, cache_misses;
    uint32_t job_setup, job_stage, job_send, job_accept, job_mix, job_state, job_rest;
} wt_profile_t;
typedef struct {
    wt_osc_t osc[2];
    wt_env_t amp, envelope;
    int32_t lfo[2];
    uint32_t phase[2], voices, note;
    wt_profile_t profile;
} wt_view_t;
#define WT_EVENT_COUNT 64u
static wt_event_t events[WT_EVENT_COUNT];
static volatile uint32_t event_write, event_read, event_overflow;
static wt_synth_t synth;
static wt_patch_t audio_base_patch;
static wt_patch_t prepared_patch;
static fx_t prepared_effects;
static int32_t prepared_vibrato;
static uint32_t prepared_valid, prepared_base_generation, prepared_midi_generation, prepared_mutes;
static uint32_t control_cache_hits, control_cache_misses;
static wt_patch_t edit;
static wt_preferences_t preferences, audio_preferences;
static uint32_t sample_rate_phase;
static int32_t sample_rate_held;
static fx_t audio_effects;
static wt_publication_t published[2];
static volatile uint32_t patch_index, patch_generation, ready, audio_fault, lit, cpu_percent, cpu_peak;
static volatile uint32_t cpu_late, cpu_reset_generation;
static wt_profile_t cpu_profile_peak;
static uint32_t cpu_reset_seen;
#define WT_BUDGET_US (HALF_FRAMES * 1000000u / FS)
static uint32_t seen_generation;
/* 0: UI may request; 1: audio may write; 2: immutable until UI finishes drawing. */
static volatile uint32_t view_mailbox;
static wt_view_t captured;
static int16_t captured_audio[SP_N] __attribute__((section(".pool")));
static uint32_t oscillator_mutes;
static volatile uint32_t flash_hold, quiet_blocks;
static uint8_t direct_source[155], direct_count[128], legacy_held[128], generated_count[128];
static void say(const char *text);
static void play_audio_panic(void);

static int ev_push_source(uint32_t kind, uint32_t note, uint32_t velocity, uint32_t source)
{
    uint32_t w = event_write;
    if (event_overflow || w - event_read >= WT_EVENT_COUNT) {
        event_overflow = 1; say("EVENT OVERFLOW"); return -1;
    }
    events[w % WT_EVENT_COUNT].kind = (uint8_t)kind;
    events[w % WT_EVENT_COUNT].note = (uint8_t)note;
    events[w % WT_EVENT_COUNT].velocity = (uint8_t)velocity;
    events[w % WT_EVENT_COUNT].source = (uint8_t)source;
    RING_PUBLISH(); event_write = w + 1u;
    return 0;
}

static int ev_push(uint32_t kind, uint32_t note, uint32_t velocity)
{
    return ev_push_source(kind, note, velocity, 255);
}

static void audio_release(uint32_t note)
{
    if (!direct_count[note] && !legacy_held[note] && !generated_count[note])
        if (wt_synth_note_off(&synth, note)) audio_fault = 1;
}

static void audio_manual_on(uint32_t note, uint32_t velocity, uint32_t source)
{
    if (source < 155u) {
        uint32_t old = direct_source[source];
        if (old != note + 1u) {
            if (old) { direct_count[old-1u]--; audio_release(old-1u); }
            direct_source[source] = (uint8_t)(note+1u); direct_count[note]++;
        }
    } else legacy_held[note] = 1;
    if (wt_synth_note_on(&synth, note, velocity)) audio_fault = 1;
}

static void audio_manual_off(uint32_t note, uint32_t source)
{
    if (source < 155u) {
        if (direct_source[source] != note+1u) return;
        direct_source[source] = 0; direct_count[note]--;
    } else legacy_held[note] = 0;
    audio_release(note);
}

static void audio_generated_on(uint32_t note, uint32_t velocity)
{
    generated_count[note]++;
    if (wt_synth_note_on(&synth, note, velocity)) audio_fault = 1;
}
static void audio_generated_off(uint32_t note)
{
    if (generated_count[note]) generated_count[note]--;
    audio_release(note);
}
static void audio_panic(void)
{
    wts_clear(direct_source, sizeof direct_source); wts_clear(direct_count, sizeof direct_count);
    wts_clear(legacy_held, sizeof legacy_held); wts_clear(generated_count, sizeof generated_count);
    play_audio_panic();
    wt_synth_panic(&synth);
}

#include "wt_play.c"
#include "wt_storage.c"

static void patch_publish(void)
{
    uint32_t i, next = patch_index ^ 1u;
    if (!wt_patch_valid(&edit) || !wt_preferences_valid(&preferences)) {
        say("INVALID PATCH"); return;
    }
    published[next].patch = edit;
    published[next].preferences = preferences;
    fx_config(&published[next].effects, edit.fx);
    for (i = 0; i < 2u; i++)
        if (oscillator_mutes >> i & 1u) published[next].patch.osc[i].gain = 0;
    RING_PUBLISH(); patch_index = next;
    RING_PUBLISH(); patch_generation++;
}

static int app_sysex(const uint8_t *p, uint32_t n)
{
    if (n && p[0] == 0x43u) say("DX7 DATA NOT SUPPORTED");
    return 0;                                 /* updater SysEx remains the platform's */
}

static int32_t wt_output_sample(int32_t value, uint32_t bits)
{
    static const uint8_t shifts[] = {0, 8, 12, 16};
    uint32_t sh = shifts[bits];
    if (sh) value = (int32_t)(((int64_t)value + ((int64_t)1 << (sh - 1u))) >> sh) * (int32_t)(1u << sh);
    return wts_clamp(value, -8388607, 8388607);
}

static void wt_sample_rate_reset(uint32_t rate)
{
    sample_rate_phase = FS-WT_SAMPLE_RATES[rate];
    sample_rate_held = 0;
}

static int32_t wt_sample_rate_sample(int32_t value, uint32_t hz)
{
    sample_rate_phase += hz;
    if (sample_rate_phase >= FS) {
        sample_rate_phase -= FS;
        sample_rate_held = value;
    }
    return sample_rate_held;
}

static void view_capture(void)
{
    uint32_t i, newest = WT_VOICES, r = sp_wr - SP_N;
    if (view_mailbox != 1u) return;
    wts_clear(&captured, sizeof captured);
    captured.profile = cpu_profile_peak;
    captured.profile.late = cpu_late;
    for (i = 0; i < WT_VOICES; i++)
        if (synth.voice[i].active) {
            captured.voices++;
            if (newest == WT_VOICES || synth.age - synth.voice[i].age < synth.age - synth.voice[newest].age)
                newest = i;
        }
    if (newest < WT_VOICES) {
        const wt_voice_t *v = &synth.voice[newest];
        captured.amp = v->amp; captured.envelope = v->envelope; captured.note = v->note;
        for (i = 0; i < 2u; i++) {
            captured.osc[i] = v->osc[i];
            captured.lfo[i] = wts_lfo(&synth, v, i);
            captured.phase[i] = synth.patch.lfo[i].key_sync ? v->phase[i] : synth.phase[i];
        }
    } else {
        for (i = 0; i < 2u; i++) {
            wt_controls_t c;
            c.table = synth.patch.osc[i].table; c.position = synth.patch.osc[i].position;
            c.gain = synth.patch.osc[i].gain;
            c.increment = wt_pitch_increment(60 * 256 + synth.patch.osc[i].semitone * 256 +
                                             synth.patch.osc[i].fine + synth.tuning);
            if (wt_osc_start(&captured.osc[i], &c) != 0) audio_fault = 1;
            captured.phase[i] = synth.phase[i];
        }
    }
    for (i = 0; i < SP_N; i++) captured_audio[i] = sp_ring[(r + i) & (SP_N - 1u)];
    RING_PUBLISH(); view_mailbox = 2;
}

static int wt_controls_settled(void)
{
    uint32_t i;
    if (mi_seen != mi_generation || mi_bend.left) return 0;
    for (i = 0; i < MI_SLOTS; i++)
        if (mi_ramp[i].left || mi_mix[i].left) return 0;
    return 1;
}

static void wt_prepare_controls(void)
{
    uint32_t settled = (uint32_t)wt_controls_settled(), mutes = oscillator_mutes;
    if (prepared_valid && settled && prepared_base_generation == seen_generation &&
        prepared_midi_generation == mi_seen && prepared_mutes == mutes) {
        control_cache_hits++;
        return;
    }
    wt_midi_patch(&prepared_patch,&audio_base_patch,&prepared_effects);
    if (mutes & 1u) prepared_patch.osc[0].gain = 0;
    if (mutes & 2u) prepared_patch.osc[1].gain = 0;
    prepared_vibrato = mi_value(MI_VIBRATO,0,-256,256);
    prepared_base_generation = seen_generation; prepared_midi_generation = mi_seen;
    prepared_mutes = mutes; prepared_valid = settled;
    control_cache_misses++;
}

#if defined(WT_CPU1) && WT_CPU1_JOB_SAMPLES == 64
static int wt_batch_ready(const play_t *p)
{
    if (p->run || p->arp || pl_run || pl_an || arp_note != 0xFFu) return 0;
    return wt_controls_settled();
}
#endif

static void app_audio(int32_t *out, uint32_t n)
{
#if defined(WT_CPU1) && WT_CPU1_JOB_SAMPLES == 64
    int32_t block[64], right[32];
    uint32_t cached = 0, buffer_from = 0;
#else
    int32_t block[32], right[32];
#endif
    uint32_t i, b, frames, t0, us, peak = 0;
#if !defined(DINGHY_HOST) || defined(WT_PROFILE_TEST)
    uint32_t synth_ticks = 0, fx_ticks = 0, stage_start, total_ticks;
    uint32_t event_ticks, prep_ticks = 0, output_ticks = 0, view_ticks;
    wts_profile_osc = wts_profile_voice = wts_profile_control = 0;
    wt_midi_fx_ticks = 0;
#endif
    const play_t *performance;
    if (!ready || flash_hold == 2u) return;
    t0 = fm1_ticks();
    control_cache_hits = control_cache_misses = 0;
#ifdef WT_CPU1
    wt_cpu1_begin(t0,n);
#endif
    if (cpu_reset_seen != cpu_reset_generation) {
        cpu_peak = cpu_late = 0; cpu_reset_seen = cpu_reset_generation;
        wts_clear(&cpu_profile_peak,sizeof cpu_profile_peak);
#ifdef WT_CPU1
        wt_cpu1_wait_peak = wt_cpu1_busy_peak = 0;
#endif
    }
#if !defined(DINGHY_HOST) || defined(WT_PROFILE_TEST)
    stage_start = fm1_ticks();
#endif
    if (seen_generation != patch_generation) {
        const wt_publication_t *p = &published[patch_index];
        audio_base_patch = p->patch;
        if (wt_synth_patch(&synth, &p->patch) != 0 ||
            wt_synth_tune(&synth, p->preferences.transpose, p->preferences.fine) != 0)
            audio_fault = 1;
        if (audio_preferences.sample_rate != p->preferences.sample_rate)
            wt_sample_rate_reset(p->preferences.sample_rate);
        audio_preferences = p->preferences; audio_effects = p->effects; seen_generation = patch_generation;
    }
    performance = &plbuf[plcur];
    if (event_overflow) {
        event_read = event_write; audio_panic(); synth.sustain = 0;
        RING_PUBLISH(); event_overflow = 0;
    }
    while (event_read != event_write) {
        wt_event_t e = events[event_read % WT_EVENT_COUNT];
        int rc = 0;
        if (e.note > 127u || e.velocity > 127u) rc = -1;
        else if (e.kind == EV_ON) audio_manual_on(e.note, e.velocity, e.source);
        else if (e.kind == EV_OFF) audio_manual_off(e.note, e.source);
        else if (e.kind == EV_MONO) { audio_panic(); rc = wt_synth_mono(&synth, e.note); }
        else if (e.kind == EV_PANIC) audio_panic();
        else if (e.kind == EV_SUSTAIN) wt_synth_sustain(&synth,e.note);
        else if (e.kind == EV_RELEASE_ALL) {
            wts_clear(direct_source,sizeof direct_source); wts_clear(direct_count,sizeof direct_count);
            wts_clear(legacy_held,sizeof legacy_held); wts_clear(generated_count,sizeof generated_count);
            play_audio_panic(); wt_synth_all_notes_off(&synth);
        } else if (e.kind == EV_SILENCE) {
            audio_panic(); wt_synth_silence(&synth);
            wts_clear(&xs,sizeof xs); xs.ph_a = PH_A[PH_LO >> 8];
            sample_rate_held = 0;
        }
        if (rc) audio_fault = 1;
        RING_PUBLISH(); event_read++;
    }
#if !defined(DINGHY_HOST) || defined(WT_PROFILE_TEST)
    event_ticks = fm1_ticks()-stage_start;
#endif
    for (b = 0; b < n; b += frames) {
        int32_t *samples = block;
        frames = n - b < 32u ? n - b : 32u;
#if !defined(DINGHY_HOST) || defined(WT_PROFILE_TEST)
        stage_start = fm1_ticks();
#endif
        mi_tick(frames);
        wt_prepare_controls();
        if (memcmp(&prepared_patch,&synth.patch,sizeof prepared_patch) &&
            wt_synth_patch(&synth,&prepared_patch)) audio_fault = 1;
        synth.controller_pitch = mi_pitch();
        synth.controller_vibrato = prepared_vibrato;
        pl_tick(performance);
#if !defined(DINGHY_HOST) || defined(WT_PROFILE_TEST)
        prep_ticks += fm1_ticks()-stage_start;
        stage_start = fm1_ticks();
#endif
#if defined(WT_CPU1) && WT_CPU1_JOB_SAMPLES == 64
        if (!cached) {
            uint32_t render_frames = frames;
            if (n-b >= 64u && wt_batch_ready(performance)) render_frames = 64;
            if (wt_synth_render(&synth,block,render_frames)) audio_fault = 1;
            cached = render_frames; buffer_from = 0;
        }
        samples = block+buffer_from;
#else
        if (wt_synth_render(&synth,block,frames)) audio_fault = 1;
#endif
#if !defined(DINGHY_HOST) || defined(WT_PROFILE_TEST)
        synth_ticks += fm1_ticks() - stage_start;
        stage_start = fm1_ticks();
#endif
        if (audio_preferences.sample_rate) {
            uint32_t hz = WT_SAMPLE_RATES[audio_preferences.sample_rate];
            for (i = 0; i < frames; i++) samples[i] = wt_sample_rate_sample(samples[i],hz);
        }
        for (i = 0; i < frames; i++) samples[i] = wt_output_sample(samples[i], audio_preferences.bits);
#if !defined(DINGHY_HOST) || defined(WT_PROFILE_TEST)
        output_ticks += fm1_ticks()-stage_start;
        stage_start = fm1_ticks();
#endif
        fx_run(&prepared_effects,samples,right,frames);
#if !defined(DINGHY_HOST) || defined(WT_PROFILE_TEST)
        fx_ticks += fm1_ticks() - stage_start;
        stage_start = fm1_ticks();
#endif
        for (i = 0; i < frames; i++) {
            int32_t l = samples[i], r = right[i], magnitude;
            if (clk_a > 0) {
                int32_t click = clk_ph >> 31 ? clk_a : -clk_a;
                l = sat24(l+click); r = sat24(r+click);
                clk_ph += clk_inc; clk_a -= CLK_A0 / CLK_LEN;
            }
            mi_stereo(&l,&r);
            if (audio_preferences.output_mono) l = r = (l + r) / 2;
            out[2u * (b + i)] += l; out[2u * (b + i) + 1u] += r;
            if (sp_want) sp_feed(((l+r)/2) >> 8);
            magnitude = l < 0 ? -l : l; if ((uint32_t)magnitude > peak) peak = (uint32_t)magnitude;
            magnitude = r < 0 ? -r : r; if ((uint32_t)magnitude > peak) peak = (uint32_t)magnitude;
        }
#if defined(WT_CPU1) && WT_CPU1_JOB_SAMPLES == 64
        cached -= frames; buffer_from += frames;
#endif
#if !defined(DINGHY_HOST) || defined(WT_PROFILE_TEST)
        output_ticks += fm1_ticks()-stage_start;
#endif
    }
#if !defined(DINGHY_HOST) || defined(WT_PROFILE_TEST)
    stage_start = fm1_ticks();
#endif
    lit = 0;
    for (i = 0; i < WT_VOICES; i++) if (synth.voice[i].active) lit |= 1u << i;
    if (!lit && peak < 256u && event_read == event_write) {
        if (quiet_blocks < 20u) quiet_blocks++;
    } else quiet_blocks = 0;
    if (flash_hold == 1u && quiet_blocks >= 20u && !performance->run &&
        !(performance->arp && performance->an)) { RING_PUBLISH(); flash_hold = 2; }
    view_capture();
#if !defined(DINGHY_HOST) || defined(WT_PROFILE_TEST)
    view_ticks = fm1_ticks()-stage_start;
    total_ticks = fm1_ticks()-t0;
    us = total_ticks / FM1_TICKS_PER_US;
#else
    us = (fm1_ticks() - t0) / FM1_TICKS_PER_US;
#endif
    cpu_percent = us * 100u / WT_BUDGET_US;
    if (cpu_percent > cpu_peak) {
#if !defined(DINGHY_HOST) || defined(WT_PROFILE_TEST)
        uint32_t render_ticks = synth_ticks, wait_ticks = 0;
#ifdef WT_CPU1
        render_ticks = wt_cpu1_render_ticks; wait_ticks = wt_cpu1_wait_ticks;
#endif
        uint32_t job_ticks = synth_ticks-render_ticks-wait_ticks;
#ifdef WT_CPU1
        uint32_t measured_job_ticks = wt_cpu1_profile.setup+wt_cpu1_profile.stage+
            wt_cpu1_profile.send+wt_cpu1_profile.accept+wt_cpu1_profile.mix+wt_cpu1_profile.state;
        cpu_profile_peak.job_setup = wt_cpu1_profile.setup / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.job_stage = wt_cpu1_profile.stage / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.job_send = wt_cpu1_profile.send / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.job_accept = wt_cpu1_profile.accept / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.job_mix = wt_cpu1_profile.mix / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.job_state = wt_cpu1_profile.state / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.job_rest = (job_ticks-measured_job_ticks) / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
#endif
        uint32_t other_ticks = total_ticks-render_ticks-wait_ticks-fx_ticks;
        uint32_t rest_ticks = other_ticks-event_ticks-prep_ticks-job_ticks-output_ticks-view_ticks;
        cpu_profile_peak.total = cpu_percent;
        cpu_profile_peak.cache_hits = control_cache_hits;
        cpu_profile_peak.cache_misses = control_cache_misses;
        cpu_profile_peak.events = event_ticks / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.prep = prep_ticks / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.job = job_ticks / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.output = output_ticks / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.view = view_ticks / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.rest = rest_ticks / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.fx_config = wt_midi_fx_ticks / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.render = render_ticks / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.wait = wait_ticks / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.fx = fx_ticks / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.other = other_ticks / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.osc = wts_profile_osc / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.voice = wts_profile_voice / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
        cpu_profile_peak.control = wts_profile_control / FM1_TICKS_PER_US * 100u / WT_BUDGET_US;
#endif
        cpu_peak = cpu_percent;
    }
    if (us > WT_BUDGET_US) cpu_late++;
}

#include "wt_ui.c"
