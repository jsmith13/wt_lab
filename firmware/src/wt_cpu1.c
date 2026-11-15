/* SPDX-License-Identifier: GPL-3.0-only */
/* CPU0 owns allocation/publication; each renderer owns control and DSP for its voice copies. */
#ifndef WT_CPU1_JOB_SAMPLES
#define WT_CPU1_JOB_SAMPLES WT_CONTROL_SAMPLES
#endif
#if WT_CPU1_JOB_SAMPLES != 32 && WT_CPU1_JOB_SAMPLES != 64
#error WT_CPU1_JOB_SAMPLES must be 32 or 64
#endif
typedef struct {
    wt_patch_t patch;
    wt_modulation_t modulation;
    uint32_t frames, control_left, increment[2];
    uint32_t osc_ticks, voice_ticks, control_ticks, fault;
} wt_cpu1_config_t;
typedef struct {
    wt_cpu1_config_t config;
    wt_voice_t voice[4];
    uint32_t index[4], count;
    int32_t mix[WT_CPU1_JOB_SAMPLES];
} wt_cpu1_job_t;

static wt_cpu1_job_t wt_cpu1_job;
static volatile uint32_t wt_cpu1_enabled = 1;
static volatile uint32_t wt_cpu1_jobs, wt_cpu1_failures, wt_cpu1_wait_peak, wt_cpu1_busy_peak;
static volatile uint32_t wt_cpu1_skipped, wt_cpu1_batches;
static uint32_t wt_cpu1_deadline;
#ifdef WT_DSP_TICKS
static uint32_t wt_cpu1_render_ticks, wt_cpu1_wait_ticks;
typedef struct {
    uint32_t setup, stage, send, accept, mix, state;
} wt_cpu1_profile_t;
static wt_cpu1_profile_t wt_cpu1_profile;
#endif

#if !defined(_MSC_VER) && (defined(__GNUC__) || defined(__clang__))
/* may_alias permits representation copies without violating struct aliasing rules. */
typedef uint32_t wt_cpu1_copy_word_t __attribute__((__may_alias__));
_Static_assert(_Alignof(wt_voice_t) >= _Alignof(wt_cpu1_copy_word_t), "voice copy alignment");
_Static_assert(sizeof(wt_voice_t) % sizeof(wt_cpu1_copy_word_t) == 0, "whole-word voice copy");
#endif

WT_INLINE void wt_cpu1_copy_voice(wt_voice_t *dest, const wt_voice_t *src)
{
#if !defined(_MSC_VER) && (defined(__GNUC__) || defined(__clang__))
    wt_cpu1_copy_word_t *to = (void *)dest;
    const wt_cpu1_copy_word_t *from = (const void *)src;
    uint32_t words = sizeof *dest / sizeof *to;
    while (words--) *to++ = *from++;
#else
    *dest = *src;
#endif
}

static int wt_cpu1_has_budget(uint32_t now)
{
    return (int32_t)(wt_cpu1_deadline-now) > (int32_t)(FM1_CPU1_TIMEOUT_US*FM1_TICKS_PER_US);
}

static void wt_cpu1_begin(uint32_t ticks, uint32_t frames)
{
    fm1_cpu1_check();
#ifdef WT_DSP_TICKS
    wt_cpu1_render_ticks = wt_cpu1_wait_ticks = 0;
    wts_clear(&wt_cpu1_profile,sizeof wt_cpu1_profile);
#endif
    /* A provisional recovery reserve, not a guarantee of glitch-free fallback. */
    uint32_t budget = frames * 1000000u / 44100u;
    wt_cpu1_deadline = ticks + (budget > 1250u ? budget-1250u : 0u) * FM1_TICKS_PER_US;
}

static void wt_cpu1_advance(wt_modulation_t *m, const uint32_t increment[2], uint32_t n)
{
    uint32_t k;
    for (k = 0; k < 2u; k++) {
        uint32_t old = m->phase[k];
        m->phase[k] += increment[k]*n;
        if (m->phase[k] < old) m->random[k] = wts_random(m->random[k]);
    }
}

static void wt_cpu1_voice(wt_voice_t *v, wt_cpu1_config_t *job, int32_t *out)
{
    int16_t a[WT_CONTROL_SAMPLES], b[WT_CONTROL_SAMPLES];
    wt_modulation_t m = job->modulation;
    uint32_t offset = 0, left = job->control_left, k;
    void (*volatile osc)(wt_osc_t *, int16_t *, uint32_t) = wt_osc_render;
    void (*volatile voice)(wt_voice_t *, const wt_adsr_t *, const int16_t *,
                           const int16_t *, int32_t *, uint32_t) = wts_voice_render;
    while (offset < job->frames && v->active) {
        uint32_t n;
#ifdef WT_DSP_TICKS
        uint32_t t = WT_DSP_TICKS();
#endif
        if (!left) {
            if (wts_control_voice(v,&job->patch,&m,0)) { job->fault = 1; return; }
            left = WT_CONTROL_SAMPLES;
        }
#ifdef WT_DSP_TICKS
        job->control_ticks += WT_DSP_TICKS()-t; t = WT_DSP_TICKS();
#endif
        n = job->frames-offset; if (n > left) n = left;
        osc(&v->osc[0],a,n); osc(&v->osc[1],b,n);
#ifdef WT_DSP_TICKS
        job->osc_ticks += WT_DSP_TICKS()-t; t = WT_DSP_TICKS();
#endif
        voice(v,&job->patch.amp,a,b,out+offset,n);
        wts_env_advance(&v->envelope,&job->patch.envelope,n);
#ifdef WT_DSP_TICKS
        job->voice_ticks += WT_DSP_TICKS()-t;
#endif
        if (v->amp.stage == WT_ENV_OFF && !v->tail_gain.current) v->active = 0;
        for (k = 0; k < 2u; k++) {
            uint32_t old = v->phase[k];
            v->phase[k] += job->increment[k]*n;
            if (v->phase[k] < old) v->random[k] = wts_random(v->random[k]);
        }
        wt_cpu1_advance(&m,job->increment,n);
        left -= n; offset += n;
    }
}

static void wt_cpu1_work(uint32_t arg)
{
    wt_cpu1_job_t *job = &wt_cpu1_job;
    uint32_t i;
    (void)arg;
    for (i = 0; i < job->count; i++) {
        wt_cpu1_voice(&job->voice[i],&job->config,job->mix);
        if (job->config.fault) return;
    }
}

static int wt_cpu1_render(wt_synth_t *s, int32_t *out, uint32_t count)
{
    uint32_t offset = 0, i, k;
#ifdef WT_DSP_TICKS
    uint32_t stage_start = WT_DSP_TICKS();
#endif
    for (i = 0; i < count; i++) out[i] = 0;
#ifdef WT_DSP_TICKS
    wt_cpu1_profile.setup += WT_DSP_TICKS()-stage_start;
#endif
    if (s->fault) return -1;
    while (offset < count) {
        wt_cpu1_config_t local;
        wt_cpu1_job_t *remote = &wt_cpu1_job;
        uint32_t active = 0, mask = 0, dispatched = 0, n, wait_start, busy, left, remaining;
#ifdef WT_DSP_TICKS
        uint32_t render_start;
        stage_start = WT_DSP_TICKS();
#endif
        n = count-offset; if (n > WT_CPU1_JOB_SAMPLES) n = WT_CPU1_JOB_SAMPLES;
        remote->count = 0; remote->config.frames = n;
        remote->config.patch = s->patch; wts_modulation(s,&remote->config.modulation);
        remote->config.control_left = s->control_left;
        remote->config.osc_ticks = remote->config.voice_ticks = remote->config.control_ticks = 0;
        remote->config.fault = 0;
        for (i = 0; i < n; i++) remote->mix[i] = 0;
        for (k = 0; k < 2u; k++) remote->config.increment[k] = wts_lfo_inc(&s->patch.lfo[k]);
        local = remote->config;
#ifdef WT_DSP_TICKS
        wt_cpu1_profile.setup += WT_DSP_TICKS()-stage_start;
        stage_start = WT_DSP_TICKS();
#endif
        if (wt_cpu1_enabled && fm1_cpu1_on) {
            for (i = 0; i < WT_VOICES; i++) {
                if (!s->voice[i].active) continue;
                if (active++ & 1u) {
                    uint32_t slot = remote->count++;
                    remote->index[slot] = i; wt_cpu1_copy_voice(&remote->voice[slot],&s->voice[i]);
                    mask |= 1u << i;
                }
            }
#ifdef WT_DSP_TICKS
            wt_cpu1_profile.stage += WT_DSP_TICKS()-stage_start;
            stage_start = WT_DSP_TICKS();
#endif
            if (remote->count) {
                if (wt_cpu1_has_budget(fm1_ticks())) dispatched = fm1_cpu1_run(wt_cpu1_work,0);
                else wt_cpu1_skipped++;
            }
#ifdef WT_DSP_TICKS
            wt_cpu1_profile.send += WT_DSP_TICKS()-stage_start;
        } else {
            wt_cpu1_profile.stage += WT_DSP_TICKS()-stage_start;
#endif
        }
        if (!dispatched) mask = 0;
#ifdef WT_DSP_TICKS
        render_start = WT_DSP_TICKS();
#endif
        for (i = 0; i < WT_VOICES; i++)
            if (s->voice[i].active && !(mask & (1u << i)))
                wt_cpu1_voice(&s->voice[i],&local,out+offset);
#ifdef WT_DSP_TICKS
        wt_cpu1_render_ticks += WT_DSP_TICKS()-render_start;
#endif
        if (dispatched) {
            int rejected;
            wt_cpu1_jobs++; if (n > WT_CONTROL_SAMPLES) wt_cpu1_batches++;
            wait_start = fm1_ticks();
            rejected = fm1_cpu1_wait_until(wt_cpu1_deadline);
#ifdef WT_DSP_TICKS
            wt_cpu1_wait_ticks += WT_DSP_TICKS()-wait_start;
            stage_start = WT_DSP_TICKS();
#endif
            if (!rejected && remote->config.fault) {
                fm1_cpu1_retire(FM1_CPU1_MISREAD);
                rejected = 1;
            }
#ifdef WT_DSP_TICKS
            wt_cpu1_profile.accept += WT_DSP_TICKS()-stage_start;
#endif
            if (rejected) {
                wt_cpu1_failures++;
#ifdef WT_DSP_TICKS
                render_start = WT_DSP_TICKS();
#endif
                for (i = 0; i < remote->count; i++)
                    wt_cpu1_voice(&s->voice[remote->index[i]],&local,out+offset);
#ifdef WT_DSP_TICKS
                wt_cpu1_render_ticks += WT_DSP_TICKS()-render_start;
#endif
            } else {
#ifdef WT_DSP_TICKS
                stage_start = WT_DSP_TICKS();
#endif
                for (i = 0; i < remote->count; i++)
                    wt_cpu1_copy_voice(&s->voice[remote->index[i]],&remote->voice[i]);
#ifdef WT_DSP_TICKS
                wt_cpu1_profile.accept += WT_DSP_TICKS()-stage_start;
                stage_start = WT_DSP_TICKS();
#endif
                for (i = 0; i < n; i++) out[offset+i] += remote->mix[i];
#ifdef WT_DSP_TICKS
                wt_cpu1_profile.mix += WT_DSP_TICKS()-stage_start;
#endif
            }
            busy = (fm1_ticks()-wait_start)/FM1_TICKS_PER_US;
            if (busy > wt_cpu1_wait_peak) wt_cpu1_wait_peak = busy;
            busy = (remote->config.osc_ticks+remote->config.voice_ticks+
                    remote->config.control_ticks)/FM1_TICKS_PER_US;
            if (busy > wt_cpu1_busy_peak) wt_cpu1_busy_peak = busy;
        }
#ifdef WT_DSP_TICKS
        wts_profile_osc += local.osc_ticks; wts_profile_voice += local.voice_ticks;
        wts_profile_control += local.control_ticks;
#endif
        if (local.fault) { s->fault = 1; return -1; }
#ifdef WT_DSP_TICKS
        stage_start = WT_DSP_TICKS();
#endif
        left = s->control_left; remaining = n;
        while (remaining) {
            uint32_t step;
            if (!left) left = WT_CONTROL_SAMPLES;
            step = remaining < left ? remaining : left;
            wt_cpu1_advance(&remote->config.modulation,remote->config.increment,step);
            left -= step; remaining -= step;
        }
        for (k = 0; k < 2u; k++) {
            s->phase[k] = remote->config.modulation.phase[k];
            s->random[k] = remote->config.modulation.random[k];
        }
        s->control_left = left; offset += n;
#ifdef WT_DSP_TICKS
        wt_cpu1_profile.state += WT_DSP_TICKS()-stage_start;
#endif
    }
#ifdef WT_DSP_TICKS
    stage_start = WT_DSP_TICKS();
#endif
    for (i = 0; i < count; i++) out[i] = wts_clamp(out[i],-8388607,8388607);
#ifdef WT_DSP_TICKS
    wt_cpu1_profile.state += WT_DSP_TICKS()-stage_start;
#endif
    return 0;
}
