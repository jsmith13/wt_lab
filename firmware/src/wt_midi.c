/* SPDX-License-Identifier: GPL-3.0-only */
static const char *const mi_extra_names[] = {
    "WT1 POSITION","WT1 LEVEL","WT1 TUNE","WT1 ENV POS",
    "WT2 POSITION","WT2 LEVEL","WT2 TUNE","WT2 ENV POS",
    "AMP ATTACK","AMP DECAY","AMP SUSTAIN","AMP RELEASE",
    "ENV ATTACK","ENV DECAY","ENV SUSTAIN","ENV RELEASE",
    "LFO1 RATE","LFO1 DEPTH","LFO2 RATE","LFO2 DEPTH",
    "KEY TRACK","ENV CUTOFF","VELOCITY",
    "CHO RATE","CHO DEPTH","CHO WIDTH","PHA RATE","PHA DEPTH","PHA FEEDBACK",
    "DLY TIME","DLY FEEDBACK","DLY TONE","REV DECAY","REV DAMP","REV PRE"
};
#define MI_TARGET_COUNT (13u+sizeof mi_extra_names/sizeof mi_extra_names[0])
static const char *const mi_group_names[] = {
    "PERFORM","TONE","WT 1","WT 2","AMP ENV","MOD ENV","LFO",
    "CHORUS","PHASER","DELAY","REVERB"
};
/* First byte is the count; destination IDs retain their stored MIDI meaning. */
static const uint8_t mi_groups[][8] = {
    {7,0,1,2,3,4,5,35}, {5,6,7,8,33,34},
    {4,13,14,15,16}, {4,17,18,19,20},
    {4,21,22,23,24}, {4,25,26,27,28}, {4,29,30,31,32},
    {4,10,36,37,38}, {4,12,39,40,41}, {4,11,42,43,44}, {4,9,45,46,47}
};
#include "midi_control.h"
static const uint8_t wt_midi_fx_targets[16] = {MI_CHORUS,36,37,38,MI_PHASER,39,40,41,
                                             MI_DELAY,42,43,44,MI_REVERB,45,46,47};
#ifdef WT_DSP_TICKS
static uint32_t wt_midi_fx_ticks;
#endif

static void wt_midi_patch(wt_patch_t *p, const wt_patch_t *base, fx_t *effects)
{
    uint32_t i;
    *p = *base;
    p->cutoff = (uint32_t)mi_value(MI_CUTOFF,(int32_t)p->cutoff,0,WT_CUTOFF_MAX);
    p->resonance = (uint32_t)mi_value(MI_RESONANCE,(int32_t)p->resonance,0,WT_GAIN_MAX);
    for (i = 0; i < 2u; i++) {
        wt_osc_patch_t *o = &p->osc[i];
        uint32_t id = 13u+i*4u;
        int32_t detune;
        o->position = (uint32_t)mi_value(MI_BRIGHTNESS,(int32_t)o->position,0,WT_POSITION_MAX);
        o->position = (uint32_t)mi_value(id,(int32_t)o->position,0,WT_POSITION_MAX);
        o->gain = (uint32_t)mi_value(id+1u,(int32_t)o->gain,0,WT_GAIN_MAX);
        detune = mi_value(id+2u,o->semitone*256+o->fine,-25*256,25*256);
        o->semitone = wts_clamp(detune/256,-24,24); o->fine = detune-o->semitone*256;
        o->position_env = mi_value(id+3u,o->position_env,-65536,65536);
    }
    for (i = 0; i < 2u; i++) {
        wt_adsr_t *e = i ? &p->envelope : &p->amp;
        uint32_t id = 21u+i*4u;
        e->attack = (uint32_t)mi_value(id,(int32_t)e->attack,0,5000);
        e->decay = (uint32_t)mi_value(id+1u,(int32_t)e->decay,0,5000);
        e->sustain = (uint32_t)mi_value(id+2u,(int32_t)e->sustain,0,WT_GAIN_MAX);
        e->release = (uint32_t)mi_value(id+3u,(int32_t)e->release,0,5000);
        p->lfo[i].rate = (uint32_t)mi_value(29u+i*2u,(int32_t)p->lfo[i].rate,0,20000);
        p->lfo[i].depth = (uint32_t)mi_value(30u+i*2u,(int32_t)p->lfo[i].depth,0,WT_GAIN_MAX);
    }
    p->keytrack = (uint32_t)mi_value(33,(int32_t)p->keytrack,0,WT_GAIN_MAX);
    p->filter_env = mi_value(34,p->filter_env,-65536,65536);
    p->velocity = (uint32_t)mi_value(35,(int32_t)p->velocity,0,WT_GAIN_MAX);
    for (i = 0; i < 16u; i++)
        p->fx[i] = (uint8_t)mi_value(wt_midi_fx_targets[i],p->fx[i],0,99);
#ifdef WT_DSP_TICKS
    uint32_t config_start = WT_DSP_TICKS();
#endif
    fx_config(effects,p->fx);
#ifdef WT_DSP_TICKS
    wt_midi_fx_ticks += WT_DSP_TICKS()-config_start;
#endif
    effects->ch_mix = mi_value(MI_CHORUS,base->fx[0]*16384/99,0,16384);
    effects->ph_mix = mi_value(MI_PHASER,base->fx[4]*16384/99,0,16384);
    effects->dl_mix = mi_value(MI_DELAY,base->fx[8]*32767/99,0,32767);
    effects->rv_mix = mi_value(MI_REVERB,base->fx[12]*32767/99,0,32767);
    effects->on = (effects->ch_mix ? 1u : 0u) | (effects->ph_mix ? 2u : 0u) |
                  (effects->dl_mix ? 4u : 0u) | (effects->rv_mix ? 8u : 0u);
}
