/* SPDX-License-Identifier: GPL-3.0-only */
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t fm1_ticks(void);
#define WT_DSP_TICKS() fm1_ticks()
#include "wt_synth.c"
extern int wt_reference_render(wt_synth_t *, int32_t *, uint32_t);

#define FM1_TICKS_PER_US 24u
#define FM1_CPU1_TIMEOUT_US 600u
static atomic_uint ticks;
static uint32_t fm1_ticks(void) { return atomic_fetch_add(&ticks,1); }
static uint32_t fm1_cpu1_on, fm1_cpu1_gave_up;
enum { FM1_CPU1_MISREAD = 1 };
static void fm1_cpu1_retire(uint32_t why) { fm1_cpu1_on = 0; fm1_cpu1_gave_up = why; }
static void fm1_cpu1_check(void) {}
static uint32_t mode, fail_after, jobs, failed, control_reject, injected_wait;
static pthread_t thread;
static void (*work)(uint32_t);
static void *host_work(void *arg);
static int fm1_cpu1_run(void (*fn)(uint32_t), uint32_t arg)
{
    (void)arg;
    if (!fm1_cpu1_on) return 0;
    work = fn; jobs++;
    if (mode) {
        int rc = pthread_create(&thread,0,host_work,0);
        if (rc) { fprintf(stderr,"pthread_create: %d\n",rc); exit(2); }
    } else host_work(0);
    return 1;
}
static int fm1_cpu1_wait_until(uint32_t deadline)
{
    (void)deadline;
    atomic_fetch_add(&ticks,injected_wait);
    if (mode) {
        int rc = pthread_join(thread,0);
        if (rc) { fprintf(stderr,"pthread_join: %d\n",rc); exit(2); }
    }
    if (failed) { fm1_cpu1_on = 0; fm1_cpu1_gave_up = 2; return -1; }
    return 0;
}
#include "wt_cpu1.c"

static void *host_work(void *arg)
{
    uint32_t i;
    (void)arg;
    if (jobs == 3u && fail_after != UINT32_MAX) {
        for (i = 0; i < wt_cpu1_job.count && i < fail_after; i++)
            wt_cpu1_voice(&wt_cpu1_job.voice[i],&wt_cpu1_job.config,wt_cpu1_job.mix);
        if (control_reject) wt_cpu1_job.config.fault = 1;
        else failed = 1;
    } else work(0);
    return 0;
}

static int copy_checks(void)
{
    wt_voice_t source[2], saved_source[2], dest[3], saved_dest[3];
    uint32_t variant, i;
    for (variant = 0; variant < 4u; variant++) {
        uint8_t *bytes = (void *)source;
        for (i = 0; i < sizeof source; i++)
            bytes[i] = (uint8_t)(variant == 0 ? 0u : variant == 1 ? 255u : i*37u+variant*53u);
        memcpy(saved_source,source,sizeof source);
        memset(dest,0xa5,sizeof dest); memcpy(saved_dest,dest,sizeof dest);
        wt_cpu1_copy_voice(&dest[1],&source[1]);
        if (memcmp(&dest[1],&source[1],sizeof dest[1]) ||
            memcmp(&dest[0],&saved_dest[0],sizeof dest[0]) ||
            memcmp(&dest[2],&saved_dest[2],sizeof dest[2]) ||
            memcmp(source,saved_source,sizeof source)) {
            fputs("Voice representation copy changed source or adjacent records\n",stderr);
            return 1;
        }
    }
    printf("WT voice copy: %zu bytes, full representation and adjacent/source preservation passed\n",sizeof(wt_voice_t));
    return 0;
}

static int events(wt_synth_t *s, uint32_t step)
{
    wt_patch_t p = s->patch;
    uint32_t i;
    s->controller_pitch = ((int32_t)(step % 9u)-4)*128;
    s->controller_vibrato = ((int32_t)(step % 5u)-2)*128;
    p.osc[0].position = step*997u % (WT_POSITION_MAX+1u);
    p.osc[0].table = (step / 8u) % WT_TABLE_COUNT;
    p.osc[1].table = (step / 8u + 7u) % WT_TABLE_COUNT;
    p.cutoff = 8000u+step*619u % 60000u;
    p.resonance = step*397u % (WT_GAIN_MAX+1u);
    if (wt_synth_patch(s,&p)) return -1;
    switch (step) {
    case 4: wt_synth_sustain(s,1); break;
    case 5:
        for (i = 0; i < 8u; i++) if (wt_synth_note_off(s,48u+3u*i)) return -1;
        break;
    case 6: return wt_synth_note_on(s,60,127) || wt_synth_note_on(s,60,99);
    case 7: return wt_synth_note_off(s,60);
    case 16:
        for (i = 0; i < 10u; i++) if (wt_synth_note_on(s,72u+i,90u+i)) return -1;
        break;
    case 24: wt_synth_all_notes_off(s); break;
    case 28: wt_synth_sustain(s,0); break;
    case 40:
        wt_patch_init(&p); p.osc[1].gain = WT_GAIN_MAX;
        return wt_synth_patch(s,&p);
    case 64: wt_synth_silence(s); break;
    default: break;
    }
    return 0;
}

static int budget_checks(void)
{
    static const uint32_t remaining[] = {0,1,599u*24u,600u*24u,600u*24u+1u,600u*24u+256u};
    uint32_t test;
    for (test = 0; test < sizeof remaining/sizeof remaining[0]; test++) {
        wt_synth_t reference, candidate;
        int32_t serial[32], parallel[32];
        uint32_t i, admitted = test == 5u;
        wt_synth_init(&reference);
        for (i = 0; i < WT_VOICES; i++)
            if (wt_synth_note_on(&reference,48u+i,100)) return 1;
        candidate = reference;
        fm1_cpu1_on = wt_cpu1_enabled = 1;
        mode = jobs = failed = wt_cpu1_skipped = 0; fail_after = UINT32_MAX;
        atomic_store(&ticks,0xfffffff0u);
        wt_cpu1_deadline = 0xfffffff0u+remaining[test];
        if (wt_cpu1_has_budget(0xfffffff0u) != (test >= 4u)) return 1;
        if (wt_reference_render(&reference,serial,32) || wt_cpu1_render(&candidate,parallel,32) ||
            memcmp(serial,parallel,sizeof serial) || memcmp(&reference,&candidate,sizeof reference) ||
            jobs != admitted || wt_cpu1_skipped != !admitted || !fm1_cpu1_on) {
            fprintf(stderr,"Dispatch budget/state mismatch: remaining=%u\n",remaining[test]);
            return 1;
        }
    }
    puts("WT CPU1 dispatch: full job budget, serial skip equivalence and timer wrap passed");
    return 0;
}

static int context_checks(void)
{
    uint32_t filter, shape, sync, step, i, checks = 0;
    for (filter = 0; filter <= WT_FILTER_HP; filter++)
        for (shape = 0; shape < WT_LFO_SHAPES; shape++)
            for (sync = 0; sync < 2u; sync++) {
                wt_synth_t reference, candidate;
                wt_synth_init(&reference);
                reference.patch.filter_type = filter;
                reference.patch.osc[0].table = (shape*3u+filter*2u+sync) % WT_TABLE_COUNT;
                reference.patch.osc[1].table = (reference.patch.osc[0].table+7u) % WT_TABLE_COUNT;
                reference.patch.osc[1].gain = WT_GAIN_MAX;
                reference.patch.osc[0].position_env = 65536;
                reference.patch.osc[1].position_env = -65536;
                reference.patch.filter_env = -65536;
                reference.patch.lfo[0].shape = reference.patch.lfo[1].shape = shape;
                reference.patch.lfo[0].key_sync = reference.patch.lfo[1].key_sync = sync;
                reference.patch.lfo[0].rate = reference.patch.lfo[1].rate = 20000;
                reference.patch.lfo[0].depth = reference.patch.lfo[1].depth = WT_GAIN_MAX;
                reference.patch.lfo[0].destination = WT_MOD_BOTH;
                reference.patch.lfo[1].destination = WT_MOD_FILTER;
                reference.phase[0] = reference.phase[1] = 0xfffffff0u;
                reference.controller_pitch = 333; reference.controller_vibrato = 256;
                reference.tuning = 17;
                for (i = 0; i < WT_VOICES; i++)
                    if (wt_synth_note_on(&reference,48u+3u*i,100)) return 1;
                candidate = reference;
                fm1_cpu1_on = wt_cpu1_enabled = 1;
                mode = 1; jobs = failed = 0; fail_after = UINT32_MAX;
                for (step = 0; step < 24u; step++) {
                    static const uint32_t sizes[] = {17,64,7,256,33,1};
                    int32_t serial[256], parallel[256];
                    uint32_t n = sizes[step % 6u];
                    wt_cpu1_begin(fm1_ticks(),256);
                    if (wt_reference_render(&reference,serial,n) ||
                        wt_cpu1_render(&candidate,parallel,n) ||
                        memcmp(serial,parallel,n*sizeof *serial) ||
                        memcmp(&reference,&candidate,sizeof reference)) {
                        fprintf(stderr,"Control snapshot mismatch: filter=%u shape=%u sync=%u step=%u\n",
                                filter,shape,sync,step);
                        return 1;
                    }
                    checks++;
                }
            }
    {
        wt_synth_t s;
        int32_t out[32];
        wt_synth_init(&s);
        for (i = 0; i < WT_VOICES; i++)
            if (wt_synth_note_on(&s,48u+i,100)) return 1;
        s.patch.osc[1].table = WT_TABLE_COUNT;
        fm1_cpu1_on = wt_cpu1_enabled = 1; jobs = failed = 0; mode = 1;
        wt_cpu1_begin(fm1_ticks(),256);
        if (wt_cpu1_render(&s,out,32) != -1 || !s.fault) return 1;
    }
    printf("WT staged controls: %u full-state/output checks, global/key LFO wrap, filters and fault propagation passed\n",checks);
    return 0;
}

static int profile_checks(void)
{
    uint32_t variant, i, normal_render = 0;
    for (variant = 0; variant < 3u; variant++) {
        wt_synth_t reference, candidate;
        int32_t serial[32], parallel[32];
        wt_synth_init(&reference);
        for (i = 0; i < WT_VOICES; i++)
            if (wt_synth_note_on(&reference,48u+i,100)) return 1;
        candidate = reference;
        fm1_cpu1_on = wt_cpu1_enabled = 1;
        mode = failed = fm1_cpu1_gave_up = 0;
        jobs = 2; fail_after = variant ? 1u : UINT32_MAX;
        control_reject = variant == 2u;
        injected_wait = 100;
        atomic_store(&ticks,0xfffffff0u);
        wt_cpu1_begin(fm1_ticks(),256);
        if (wt_cpu1_render_ticks || wt_cpu1_wait_ticks || wt_cpu1_profile.setup ||
            wt_cpu1_profile.stage || wt_cpu1_profile.send || wt_cpu1_profile.accept ||
            wt_cpu1_profile.mix || wt_cpu1_profile.state) return 1;
        if (wt_reference_render(&reference,serial,32) ||
            wt_cpu1_render(&candidate,parallel,32) ||
            memcmp(serial,parallel,sizeof serial) || memcmp(&reference,&candidate,sizeof reference) ||
            wt_cpu1_wait_ticks != 101u || !wt_cpu1_render_ticks) {
            fprintf(stderr,"CPU0 profile/output mismatch: variant=%u render=%u wait=%u\n",
                    variant,wt_cpu1_render_ticks,wt_cpu1_wait_ticks);
            return 1;
        }
        if (!variant) normal_render = wt_cpu1_render_ticks;
        else if (wt_cpu1_render_ticks <= normal_render) {
            fputs("Retired-worker recovery missing from CPU0 render profile\n",stderr);
            return 1;
        }
        if (!wt_cpu1_profile.setup || !wt_cpu1_profile.stage || !wt_cpu1_profile.send ||
            !wt_cpu1_profile.accept || !wt_cpu1_profile.state ||
            (!variant && !wt_cpu1_profile.mix) || (variant && wt_cpu1_profile.mix)) {
            fputs("Job profile omits work or merges rejected worker output\n",stderr);
            return 1;
        }
    }
    injected_wait = control_reject = 0;
    puts("CPU0 profiling: callback reset, timer wrap, pure wait and recovery rendering passed");
    return 0;
}

int main(void)
{
    static const uint32_t sizes[] = {1,7,32,17,64,33,256};
    static const uint32_t failures[] = {UINT32_MAX,UINT32_MAX,0,1,4,1};
    uint32_t voices, variant, step, i, comparisons = 0, recoveries = 0;
    if (copy_checks() || budget_checks() || context_checks() || profile_checks()) return 1;
    for (voices = 0; voices <= WT_VOICES; voices++) {
        wt_synth_t initial;
        wt_synth_init(&initial);
        initial.patch.osc[1].gain = WT_GAIN_MAX;
        initial.patch.amp.attack = 0;
        for (i = 0; i < voices; i++)
            if (wt_synth_note_on(&initial,48u+3u*i,127)) return 2;
        for (variant = 0; variant < 6u; variant++) {
            wt_synth_t reference = initial, candidate = initial;
            fm1_cpu1_on = 1; fm1_cpu1_gave_up = jobs = failed = 0;
            wt_cpu1_enabled = 1; wt_cpu1_failures = 0;
            mode = variant != 0; fail_after = failures[variant];
            control_reject = variant == 5u;
            for (step = 0; step < 96u; step++) {
                int32_t serial[256], parallel[256];
                uint32_t n = sizes[step % 7u];
                if (events(&reference,step) || events(&candidate,step)) return 2;
                wt_cpu1_begin(fm1_ticks(),256);
                if (wt_reference_render(&reference,serial,n) || wt_cpu1_render(&candidate,parallel,n)) return 2;
                if (memcmp(serial,parallel,n*sizeof *serial) ||
                    memcmp(&reference,&candidate,sizeof reference)) {
                    fprintf(stderr,"Mismatch: voices=%u variant=%u step=%u\n",voices,variant,step);
                    return 1;
                }
                comparisons++;
            }
            if (wt_cpu1_failures) {
                if (wt_cpu1_failures != 1u || fm1_cpu1_on) return 2;
                recoveries++;
            }
        }
    }
    if (!recoveries || !wt_cpu1_jobs) return 2;
    printf("WT CPU1: %u output/state comparisons, %u retired-worker recoveries passed\n",
           comparisons,recoveries);
    return 0;
}
