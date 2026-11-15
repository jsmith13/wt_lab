/* SPDX-License-Identifier: GPL-3.0-only */
/* WT voices use FM Lab's released-first allocation, last-note mono priority, and TPT SVF topology.
 * The envelopes, modulation, per-voice filtering, and declicking here are native WT implementations.
 * Like the inherited firmware, this is a single-translation-unit build. */
#include "wt_synth.h"
#include "wt_osc.c"
#include "wt_dsp_data.h"
#include <stddef.h>

#ifdef WT_DSP_TICKS
static uint32_t wts_profile_osc, wts_profile_voice, wts_profile_control;
#endif

static void wts_clear(void *memory, size_t count)
{
    uint8_t *p = (uint8_t *)memory;
    while (count--) *p++ = 0;
}

WT_INLINE int32_t wts_clamp(int32_t n, int32_t lo, int32_t hi)
{
    return n < lo ? lo : n > hi ? hi : n;
}

WT_INLINE uint32_t wts_samples(uint32_t ms)
{
    uint32_t n = ms * WT_SAMPLE_RATE / 1000u;
    return n < WT_SMOOTH_SAMPLES ? WT_SMOOTH_SAMPLES : n;
}

uint32_t wt_filter_hz(uint32_t position)
{
    uint32_t pos, i;
    if (position > WT_CUTOFF_MAX) position = WT_CUTOFF_MAX;
    pos = position * 128u; i = pos >> 16;
    return (uint32_t)wt_lerp(WT_FILTER_HZ[i], WT_FILTER_HZ[i < WT_FILTER_STEPS ? i + 1u : i], pos & 65535u);
}

void wt_patch_init(wt_patch_t *p)
{
    static const uint8_t effects[16] = {0,45,40,70, 0,35,70,40, 0,65,35,60, 0,55,30,10};
    static const char name[] = "INIT WAVE";
    uint32_t i;
    wts_clear(p, sizeof *p);
    p->osc[0].gain = WT_GAIN_MAX;
    p->osc[1].fine = 7;                        /* ~2.7 cents; silent until its level is raised */
    p->amp.attack = 5; p->amp.decay = 250; p->amp.sustain = 24576; p->amp.release = 180;
    p->envelope.attack = 5; p->envelope.decay = 350; p->envelope.release = 250;
    p->cutoff = 52000; p->velocity = WT_GAIN_MAX;
    p->lfo[0].rate = p->lfo[1].rate = 5000;
    p->lfo[0].key_sync = p->lfo[1].key_sync = 1;
    for (i = 0; i < 16u; i++) p->fx[i] = effects[i];
    for (i = 0; i < sizeof name; i++) p->name[i] = name[i];
}

static int wts_adsr_valid(const wt_adsr_t *p)
{
    return p->attack <= 5000u && p->decay <= 5000u && p->release <= 5000u &&
           p->sustain <= WT_GAIN_MAX;
}

int wt_patch_valid(const wt_patch_t *p)
{
    uint32_t i, ended = 0;
    if (!wts_adsr_valid(&p->amp) || !wts_adsr_valid(&p->envelope) ||
        p->filter_type > WT_FILTER_HP || p->cutoff > WT_CUTOFF_MAX ||
        p->resonance > WT_GAIN_MAX || p->keytrack > WT_GAIN_MAX || p->velocity > WT_GAIN_MAX ||
        p->filter_env < -(int32_t)WT_POSITION_MAX || p->filter_env > (int32_t)WT_POSITION_MAX)
        return 0;
    for (i = 0; i < 16u; i++) if (p->fx[i] > 99u) return 0;
    if (!p->name[0] || p->name[WT_NAME_LEN - 1u]) return 0;
    for (i = 0; i < WT_NAME_LEN; i++) {
        uint32_t c = (uint8_t)p->name[i];
        if (!c) ended = 1;
        else if (ended || c < 32u || c > 126u) return 0;
    }
    for (i = 0; i < 2u; i++)
        if (p->osc[i].table >= WT_TABLE_COUNT || p->osc[i].position > WT_POSITION_MAX ||
            p->osc[i].gain > WT_GAIN_MAX || p->osc[i].semitone < -24 || p->osc[i].semitone > 24 ||
            p->osc[i].fine < -256 || p->osc[i].fine > 256 ||
            p->osc[i].position_env < -(int32_t)WT_POSITION_MAX ||
            p->osc[i].position_env > (int32_t)WT_POSITION_MAX ||
            p->lfo[i].rate > 20000u || p->lfo[i].shape >= WT_LFO_SHAPES ||
            p->lfo[i].depth > WT_GAIN_MAX || p->lfo[i].destination >= WT_MOD_DESTS ||
            p->lfo[i].key_sync > 1u)
            return 0;
    return 1;
}

WT_INLINE void wts_env_segment(wt_env_t *e, uint32_t stage, int32_t target, uint32_t count)
{
    int32_t delta = target - e->value;
    e->stage = stage; e->target = target; e->left = e->divisor = count;
    e->step = delta / (int32_t)count;
    e->remainder = delta % (int32_t)count;
    e->error = 0;
}

static void wts_env_start(wt_env_t *e, const wt_adsr_t *p)
{
    e->value = 0;
    e->span = wts_samples(p->attack);
    wts_env_segment(e, WT_ENV_ATTACK, WT_ENV_ONE, e->span);
}

WT_INLINE int32_t wts_env_next(wt_env_t *e, const wt_adsr_t *p)
{
    if (e->left) {
        e->value += e->step;
        e->error += e->remainder;
        if (e->error >= (int32_t)e->divisor) { e->value++; e->error -= (int32_t)e->divisor; }
        if (e->error <= -(int32_t)e->divisor) { e->value--; e->error += (int32_t)e->divisor; }
        if (!--e->left) {
            e->value = e->target;
            if (e->stage == WT_ENV_ATTACK) {
                e->span = wts_samples(p->decay);
                wts_env_segment(e, WT_ENV_DECAY, (int32_t)p->sustain * 512, e->span);
            } else if (e->stage == WT_ENV_DECAY) {
                e->stage = WT_ENV_SUSTAIN;
            } else if (e->stage == WT_ENV_RELEASE) {
                e->stage = WT_ENV_OFF;
            }
        }
    }
    return e->value;
}

static void wts_env_advance(wt_env_t *e, const wt_adsr_t *p, uint32_t count)
{
    while (count && e->left) {
        uint32_t n = count < e->left ? count : e->left;
        int32_t error;
        /* Leave the segment's final sample to the normal stage-transition path. */
        if (n == e->left) n--;
        error = e->error + e->remainder * (int32_t)n;
        e->value += e->step * (int32_t)n + error / (int32_t)e->divisor;
        e->error = error % (int32_t)e->divisor;
        e->left -= n; count -= n;
        if (count) { wts_env_next(e, p); count--; }
    }
}

static void wts_env_release(wt_env_t *e, uint32_t ms)
{
    if (e->stage == WT_ENV_OFF)
        return;
    e->span = wts_samples(ms);
    wts_env_segment(e, WT_ENV_RELEASE, 0, e->span);
}

static void wts_env_edit(wt_env_t *e, const wt_adsr_t *old, const wt_adsr_t *p)
{
    uint32_t before, after, left;
    int32_t target = e->target;
    if (e->stage == WT_ENV_OFF)
        return;
    if (e->stage == WT_ENV_SUSTAIN) {
        if (old->sustain != p->sustain)
            wts_env_segment(e, WT_ENV_SUSTAIN, (int32_t)p->sustain * 512, WT_SMOOTH_SAMPLES);
        return;
    }
    before = e->stage == WT_ENV_ATTACK ? old->attack : e->stage == WT_ENV_DECAY ? old->decay : old->release;
    after = e->stage == WT_ENV_ATTACK ? p->attack : e->stage == WT_ENV_DECAY ? p->decay : p->release;
    if (e->stage == WT_ENV_DECAY)
        target = (int32_t)p->sustain * 512;
    if (before == after && target == e->target)
        return;
    /* Q14 remaining fraction keeps both products within 32 bits for the five-second maximum. */
    left = e->left;
    if (before != after) {
        uint32_t fraction = left * 16384u / e->span;
        e->span = wts_samples(after);
        left = (e->span * fraction) >> 14;
    }
    wts_env_segment(e, e->stage, target, left ? left : 1u);
}

static uint32_t wts_random(uint32_t n)
{
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    return n;
}

static uint32_t wts_lfo_inc(const wt_lfo_patch_t *p) { return p->rate * 97391u / 1000u; }

static int32_t wts_lfo_wave(uint32_t shape, uint32_t phase, uint32_t random)
{
    switch (shape) {
    case WT_LFO_SINE: return wt_read(WT_LFO_SIN, phase);
    case WT_LFO_TRIANGLE: {
        uint32_t t = phase >> 15;
        return (int32_t)(t < 65536u ? t : 131071u - t) - 32768;
    }
    case WT_LFO_SAW: return (int32_t)(phase >> 16) - 32768;
    case WT_LFO_SQUARE: return phase >> 31 ? -32768 : 32767;
    default: return (int32_t)(random >> 16) - 32768;
    }
}

typedef struct {
    int32_t tuning, pitch, vibrato;
    uint32_t phase[2], random[2];
} wt_modulation_t;

static int32_t wts_lfo_input(const wt_lfo_patch_t *p, const wt_voice_t *v,
                             uint32_t phase, uint32_t random, uint32_t i)
{
    return wts_lfo_wave(p->shape, p->key_sync ? v->phase[i] : phase,
                        p->key_sync ? v->random[i] : random);
}

static int32_t wts_lfo(const wt_synth_t *s, const wt_voice_t *v, uint32_t i)
{
    return wts_lfo_input(&s->patch.lfo[i],v,s->phase[i],s->random[i],i);
}

static void wts_filter_target(wt_voice_t *v, uint32_t cutoff, uint32_t resonance)
{
    uint32_t c = cutoff * 128u, r = resonance * 30u;
    uint32_t ci = c >> 16, cn = ci < WT_FILTER_STEPS ? ci + 1u : ci;
    uint32_t ri = r >> 16, rn = ri < 15u ? ri + 1u : ri, k;
    for (k = 0; k < 3u; k++) {
        int32_t a = wt_lerp(WT_FILTER_A[ri][ci][k], WT_FILTER_A[ri][cn][k], c & 65535u);
        int32_t b = wt_lerp(WT_FILTER_A[rn][ci][k], WT_FILTER_A[rn][cn][k], c & 65535u);
        wt_ramp_set(&v->coefficient[k], (uint32_t)wt_lerp(a, b, r & 65535u));
    }
    wt_ramp_set(&v->coefficient[3], (uint32_t)wt_lerp(WT_FILTER_K[ri], WT_FILTER_K[rn], r & 65535u));
}

static int wts_control_voice(wt_voice_t *v, const wt_patch_t *p,
                             const wt_modulation_t *m, int start)
{
    int32_t pos[2], pitch = m->pitch, cut = (int32_t)p->cutoff;
    uint32_t i, fault = 0;
    if (m->vibrato)
        pitch += (int32_t)(((int64_t)wts_lfo_input(&p->lfo[0],v,m->phase[0],
                             m->random[0],0)*m->vibrato) >> 15);
    for (i = 0; i < 2u; i++)
        pos[i] = (int32_t)p->osc[i].position +
                 (int32_t)(((int64_t)v->envelope.value * p->osc[i].position_env) >> 24);
    cut += (int32_t)(((int64_t)v->envelope.value * p->filter_env) >> 24);
    cut += ((int32_t)v->note - 60) * 566 * (int32_t)p->keytrack / 32768;
    for (i = 0; i < 2u; i++) {
        int32_t depth;
        if (!p->lfo[i].depth || p->lfo[i].destination == WT_MOD_NONE) continue;
        depth = (wts_lfo_input(&p->lfo[i],v,m->phase[i],m->random[i],i) *
                 (int32_t)p->lfo[i].depth) >> 15;
        switch (p->lfo[i].destination) {
        case WT_MOD_POS1: pos[0] += depth * 2; break;
        case WT_MOD_POS2: pos[1] += depth * 2; break;
        case WT_MOD_BOTH: pos[0] += depth * 2; pos[1] += depth * 2; break;
        case WT_MOD_PITCH: pitch += depth * 3072 / 32768; break;
        case WT_MOD_FILTER: cut += depth * 2; break;
        default: break;
        }
    }
    for (i = 0; i < 2u; i++) {
        wt_controls_t c;
        c.table = p->osc[i].table;
        c.position = (uint32_t)wts_clamp(pos[i], 0, WT_POSITION_MAX);
        c.increment = wt_pitch_increment((int32_t)v->note * 256 + p->osc[i].semitone * 256 +
                                         p->osc[i].fine + pitch + m->tuning);
        c.gain = p->osc[i].gain;
        v->position[i] = (int32_t)c.position;
        if ((start ? wt_osc_start(&v->osc[i], &c) : wt_osc_set(&v->osc[i], &c)) != 0)
            fault = 1;
    }
    v->cutoff = wts_clamp(cut, 0, WT_CUTOFF_MAX);
    if (p->filter_type || v->filter_mix[0].current != (int32_t)WT_GAIN_MAX)
        wts_filter_target(v, (uint32_t)v->cutoff, p->resonance);
    return fault ? -1 : 0;
}

static void wts_modulation(const wt_synth_t *s, wt_modulation_t *m)
{
    uint32_t i;
    m->tuning = s->tuning; m->pitch = s->controller_pitch; m->vibrato = s->controller_vibrato;
    for (i = 0; i < 2u; i++) { m->phase[i] = s->phase[i]; m->random[i] = s->random[i]; }
}

static void wts_control(wt_synth_t *s, wt_voice_t *v, int start)
{
    wt_modulation_t m;
    wts_modulation(s,&m);
    if (wts_control_voice(v,&s->patch,&m,start)) s->fault = 1;
}

void wt_synth_init(wt_synth_t *s)
{
    wts_clear(s, sizeof *s);
    wt_patch_init(&s->patch);
    s->random[0] = 0x1234567u; s->random[1] = 0x7654321u;
}

int wt_synth_patch(wt_synth_t *s, const wt_patch_t *p)
{
    uint32_t i, k;
    if (!wt_patch_valid(p))
        return -1;
    for (i = 0; i < WT_VOICES; i++) {
        wt_voice_t *v = &s->voice[i];
        if (!v->active) continue;
        if (!v->killed) {
            wts_env_edit(&v->amp, &s->patch.amp, &p->amp);
            wts_env_edit(&v->envelope, &s->patch.envelope, &p->envelope);
        }
        wt_ramp_set(&v->velocity, WT_GAIN_MAX - p->velocity + p->velocity * v->vel / 127u);
        for (k = 0; k < 4u; k++)
            wt_ramp_set(&v->filter_mix[k], k == p->filter_type ? WT_GAIN_MAX : 0);
    }
    s->patch = *p;
    s->control_left = 0;
    return 0;
}

static void wts_start(wt_synth_t *s, uint32_t pick, uint32_t note, uint32_t vel, int again)
{
    wt_voice_t *v = &s->voice[pick];
    uint32_t i;
    if (again) {
        int32_t tail = v->active ? v->last_output : 0;
        wts_clear(v, sizeof *v);
        v->tail = tail;
        wt_ramp_reset(&v->tail_gain, tail ? WT_GAIN_MAX : 0);
        wt_ramp_set(&v->tail_gain, 0);
        wts_env_start(&v->amp, &s->patch.amp);
        wts_env_start(&v->envelope, &s->patch.envelope);
        wt_ramp_reset(&v->velocity, WT_GAIN_MAX - s->patch.velocity + s->patch.velocity * vel / 127u);
        for (i = 0; i < 4u; i++)
            wt_ramp_reset(&v->filter_mix[i], i == s->patch.filter_type ? WT_GAIN_MAX : 0);
        for (i = 0; i < 2u; i++)
            v->random[i] = wts_random((s->random[i] ^ ((note + 1u) * 7919u)) | 1u);
    } else {
        wt_ramp_set(&v->velocity, WT_GAIN_MAX - s->patch.velocity + s->patch.velocity * vel / 127u);
    }
    v->note = note; v->vel = vel; v->held = v->active = 1; v->sustained = 0; v->age = ++s->age;
    wts_control(s, v, again);
    if (again)
        for (i = 0; i < 4u; i++)
            wt_ramp_reset(&v->coefficient[i], (uint32_t)v->coefficient[i].target);
}

static uint32_t wts_drop(wt_synth_t *s, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < s->held && s->notes[i] != note; i++) {}
    if (i == s->held) return 0;
    for (; i + 1u < s->held; i++) {
        s->notes[i] = s->notes[i + 1u]; s->velocities[i] = s->velocities[i + 1u];
    }
    s->held--;
    return 1;
}

int wt_synth_note_on(wt_synth_t *s, uint32_t note, uint32_t velocity)
{
    uint32_t i, pick = WT_VOICES, category = 4, oldest = 0;
    if (note > 127u || velocity > 127u) return -1;
    if (!velocity) return wt_synth_note_off(s, note);
    if (s->mono) {
        int legato = s->held && s->voice[0].active;
        wts_drop(s, note);
        s->notes[s->held] = (uint8_t)note; s->velocities[s->held++] = (uint8_t)velocity;
        wts_start(s, 0, note, velocity, !legato);
        return s->fault ? -1 : 0;
    }
    for (i = 0; i < WT_VOICES; i++)
        if (s->voice[i].active && (s->voice[i].held || s->voice[i].sustained) && s->voice[i].note == note) {
            pick = i; break;
        }
    if (pick == WT_VOICES)
        for (i = 0; i < WT_VOICES; i++) {
            const wt_voice_t *v = &s->voice[i];
            uint32_t cat = !v->active ? 0u : !v->held && !v->sustained ? 1u : v->sustained ? 2u : 3u;
            uint32_t age = s->age - v->age;
            if (cat < category || (cat == category && age > oldest)) {
                category = cat; oldest = age; pick = i;
            }
        }
    wts_start(s, pick, note, velocity, 1);
    return s->fault ? -1 : 0;
}

static void wts_release(wt_synth_t *s, wt_voice_t *v)
{
    v->held = v->sustained = 0;
    wts_env_release(&v->amp, s->patch.amp.release);
    wts_env_release(&v->envelope, s->patch.envelope.release);
}

static void wts_key_release(wt_synth_t *s, wt_voice_t *v)
{
    if (s->sustain) { v->held = 0; v->sustained = 1; }
    else wts_release(s,v);
}

int wt_synth_note_off(wt_synth_t *s, uint32_t note)
{
    uint32_t i;
    if (note > 127u) return -1;
    if (s->mono) {
        if (!wts_drop(s, note)) return 0;
        if (!s->held) wts_key_release(s, &s->voice[0]);
        else if (s->voice[0].note == note)
            wts_start(s, 0, s->notes[s->held - 1u], s->velocities[s->held - 1u], 0);
    } else {
        for (i = 0; i < WT_VOICES; i++)
            if (s->voice[i].active && s->voice[i].held && s->voice[i].note == note)
                wts_key_release(s, &s->voice[i]);
    }
    return 0;
}

void wt_synth_panic(wt_synth_t *s)
{
    uint32_t i;
    s->held = 0;
    for (i = 0; i < WT_VOICES; i++) {
        s->voice[i].held = s->voice[i].sustained = 0;
        s->voice[i].killed = 1;
        wts_env_release(&s->voice[i].amp, 0);
        wts_env_release(&s->voice[i].envelope, 0);
    }
}

void wt_synth_sustain(wt_synth_t *s, uint32_t down)
{
    uint32_t i;
    s->sustain = down != 0u;
    if (!s->sustain)
        for (i = 0; i < WT_VOICES; i++)
            if (s->voice[i].sustained && !s->voice[i].held) wts_release(s,&s->voice[i]);
}

void wt_synth_all_notes_off(wt_synth_t *s)
{
    uint32_t i;
    s->held = 0;
    for (i = 0; i < WT_VOICES; i++)
        if (s->voice[i].active && s->voice[i].held) wts_key_release(s,&s->voice[i]);
}

void wt_synth_silence(wt_synth_t *s)
{
    wts_clear(s->voice,sizeof s->voice);
    s->held = 0;
}

int wt_synth_mono(wt_synth_t *s, uint32_t mono)
{
    if (mono > 1u) return -1;
    if (mono != s->mono) {
        wt_synth_panic(s);
        s->mono = mono;
    }
    return 0;
}

int wt_synth_tune(wt_synth_t *s, int32_t semitone, int32_t fine)
{
    int32_t tuning;
    if (semitone < -24 || semitone > 24 || fine < -256 || fine > 256) return -1;
    tuning = semitone * 256 + fine;
    if (tuning != s->tuning) { s->tuning = tuning; s->control_left = 0; }
    return 0;
}

WT_INLINE int32_t wts_filter_state(wt_voice_t *v, int32_t x, const int32_t a[4], int32_t *v1)
{
    int32_t v3 = x - v->ic2;
    int32_t band_state = (int32_t)(((int64_t)a[0] * v->ic1 + (int64_t)a[1] * v3) >> 30);
    int32_t low = v->ic2 + (int32_t)(((int64_t)a[1] * v->ic1 + (int64_t)a[2] * v3) >> 30);
    v->ic1 = wts_clamp(2 * band_state - v->ic1, -1048576, 1048576);
    v->ic2 = wts_clamp(2 * low - v->ic2, -1048576, 1048576);
    *v1 = band_state;
    return low;
}

WT_INLINE int32_t wts_filter(wt_voice_t *v, int32_t x)
{
    int32_t a[4], mix[4], v1, v2, band, result;
    uint32_t i;
    int64_t y;
    if (v->filter_mix[0].current == (int32_t)WT_GAIN_MAX &&
        !(v->filter_mix[0].left | v->filter_mix[1].left |
          v->filter_mix[2].left | v->filter_mix[3].left)) {
        v->ic1 = v->ic2 = 0;
        return x;
    }
    for (i = 0; i < 4u; i++) mix[i] = (int32_t)wt_ramp_next(&v->filter_mix[i]);
    if (mix[0] == (int32_t)WT_GAIN_MAX) {
        v->ic1 = v->ic2 = 0;
        return x;
    }
    for (i = 0; i < 4u; i++) a[i] = (int32_t)wt_ramp_next(&v->coefficient[i]);
    v2 = wts_filter_state(v, x, a, &v1);
    band = (int32_t)(((int64_t)a[3] * v1) >> 30);
    y = (int64_t)x * mix[0] + (int64_t)v2 * mix[1] +
        (int64_t)band * mix[2] + (int64_t)(x - band - v2) * mix[3];
    result = (int32_t)(y >> 15);
    return wts_clamp(result, -524288, 524288);
}

static WT_DSP_CODE void wts_voice_render(wt_voice_t *v, const wt_adsr_t *amp,
                                       const int16_t *a, const int16_t *b, int32_t *out, uint32_t count)
{
    uint32_t i, bypass, moving, filter_type = 4u, filter_moving;
    int32_t env, vel, tail, coefficients[4] = {0};
    bypass = v->filter_mix[0].current == (int32_t)WT_GAIN_MAX &&
        !(v->filter_mix[0].left | v->filter_mix[1].left |
          v->filter_mix[2].left | v->filter_mix[3].left);
    if (bypass) v->ic1 = v->ic2 = 0;
    filter_moving = v->coefficient[0].left | v->coefficient[1].left |
                    v->coefficient[2].left | v->coefficient[3].left;
    if (!bypass &&
        !(v->filter_mix[0].left | v->filter_mix[1].left |
          v->filter_mix[2].left | v->filter_mix[3].left)) {
        uint32_t type;
        for (type = WT_FILTER_LP; type <= WT_FILTER_HP; type++) {
            uint32_t pure = 1;
            for (i = 0; i < 4u; i++)
                if (v->filter_mix[i].current != (i == type ? (int32_t)WT_GAIN_MAX : 0)) pure = 0;
            if (pure) { filter_type = type; break; }
        }
        if (filter_type != 4u)
            for (i = 0; i < 4u; i++) coefficients[i] = v->coefficient[i].current;
    }
    moving = v->amp.left | v->velocity.left | v->tail_gain.left;
    env = v->amp.value; vel = v->velocity.current;
    tail = (int32_t)(((int64_t)v->tail * v->tail_gain.current) >> 15);
    for (i = 0; i < count; i++) {
        int32_t x = ((int32_t)a[i] + b[i]) / 2, y;
        if (!bypass) {
            if (filter_type != 4u) {
                int32_t band_state, low;
                if (filter_moving) {
                    uint32_t k;
                    for (k = 0; k < 4u; k++) coefficients[k] = (int32_t)wt_ramp_next(&v->coefficient[k]);
                }
                low = wts_filter_state(v, x, coefficients, &band_state);
                if (filter_type == WT_FILTER_LP) x = low;
                else {
                    int32_t band = (int32_t)(((int64_t)coefficients[3] * band_state) >> 30);
                    x = filter_type == WT_FILTER_BP ? band : x - band - low;
                }
                x = wts_clamp(x, -524288, 524288);
            } else x = wts_filter(v, x);
        }
        if (moving) {
            env = wts_env_next(&v->amp, amp);
            vel = (int32_t)wt_ramp_next(&v->velocity);
            tail = (int32_t)(((int64_t)v->tail * wt_ramp_next(&v->tail_gain)) >> 15);
        }
        y = (int32_t)(((int64_t)x * env) >> 19);
        y = (int32_t)(((int64_t)y * vel) >> 15) + tail;
        v->last_output = wts_clamp(y, -16777216, 16777216);
        out[i] += v->last_output;
    }
}

#ifdef WT_CPU1
#include "wt_cpu1.c"
#endif

int wt_synth_render(wt_synth_t *s, int32_t *out, uint32_t count)
{
#ifdef WT_CPU1
    return wt_cpu1_render(s,out,count);
#else
    int16_t a[WT_CONTROL_SAMPLES], b[WT_CONTROL_SAMPLES];
    uint32_t offset = 0, i, j, k, n;
#ifdef WT_DSP_TICKS
    uint32_t stage_start;
#endif
    /* Like the HAL's flash entry points, SRAM kernels exceed the target's direct-call range. */
    void (*volatile render_osc)(wt_osc_t *, int16_t *, uint32_t) = wt_osc_render;
    void (*volatile render_voice)(wt_voice_t *, const wt_adsr_t *, const int16_t *,
                                 const int16_t *, int32_t *, uint32_t) = wts_voice_render;
    for (i = 0; i < count; i++) out[i] = 0;
    if (s->fault) return -1;
    while (offset < count) {
        if (!s->control_left) {
#ifdef WT_DSP_TICKS
            stage_start = WT_DSP_TICKS();
#endif
            for (j = 0; j < WT_VOICES; j++)
                if (s->voice[j].active) wts_control(s, &s->voice[j], 0);
#ifdef WT_DSP_TICKS
            wts_profile_control += WT_DSP_TICKS() - stage_start;
#endif
            s->control_left = WT_CONTROL_SAMPLES;
        }
        if (s->fault) return -1;
        n = count - offset;
        if (n > s->control_left) n = s->control_left;
        for (j = 0; j < WT_VOICES; j++) {
            wt_voice_t *v = &s->voice[j];
            if (!v->active) continue;
#ifdef WT_DSP_TICKS
            stage_start = WT_DSP_TICKS();
#endif
            render_osc(&v->osc[0], a, n); render_osc(&v->osc[1], b, n);
#ifdef WT_DSP_TICKS
            wts_profile_osc += WT_DSP_TICKS() - stage_start;
            stage_start = WT_DSP_TICKS();
#endif
            render_voice(v, &s->patch.amp, a, b, out + offset, n);
            wts_env_advance(&v->envelope, &s->patch.envelope, n);
#ifdef WT_DSP_TICKS
            wts_profile_voice += WT_DSP_TICKS() - stage_start;
#endif
            if (v->amp.stage == WT_ENV_OFF && !v->tail_gain.current) v->active = 0;
            for (k = 0; k < 2u; k++) {
                uint32_t old = v->phase[k];
                v->phase[k] += wts_lfo_inc(&s->patch.lfo[k]) * n;
                if (v->phase[k] < old) v->random[k] = wts_random(v->random[k]);
            }
        }
        for (k = 0; k < 2u; k++) {
            uint32_t old = s->phase[k];
            s->phase[k] += wts_lfo_inc(&s->patch.lfo[k]) * n;
            if (s->phase[k] < old) s->random[k] = wts_random(s->random[k]);
        }
        s->control_left -= n; offset += n;
    }
    for (i = 0; i < count; i++) out[i] = wts_clamp(out[i], -8388607, 8388607);
    return 0;
#endif
}
