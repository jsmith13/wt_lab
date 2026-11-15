/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef WT_OSC_H
#define WT_OSC_H

#include <stdint.h>

#if defined(_MSC_VER)
#define WT_INLINE static __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define WT_INLINE static inline __attribute__((always_inline))
#else
#define WT_INLINE static inline
#endif

#ifndef WT_DSP_CODE
#define WT_DSP_CODE
#endif

#define WT_SAMPLE_RATE 44100u
#define WT_TABLE_COUNT 14u
#define WT_FRAME_COUNT 8u
#define WT_TABLE_SIZE 256u
#define WT_LEVEL_COUNT 7u
#define WT_POSITION_MAX 65536u
#define WT_GAIN_MAX 32768u
#define WT_MAX_INCREMENT 0x70000000u
#define WT_PITCH_MAX 134u
#define WT_SMOOTH_SAMPLES 128u

typedef struct {
    const char *name;
    const char *description;
    const char *source;
} wt_table_info_t;

extern const wt_table_info_t wt_table_info[WT_TABLE_COUNT];

typedef struct {
    uint32_t table;
    uint32_t position;                         /* normalized Q16: 0 = first frame, 65536 = last */
    uint32_t increment;                        /* cycles/sample * 2^32; 0 holds the phase */
    uint32_t gain;                             /* Q15: 32768 = unity */
} wt_controls_t;

typedef struct {
    int32_t current, target, step, remainder, error;
    uint32_t left;
} wt_ramp_t;

typedef struct {
    uint32_t phase, table, next_table, target_table, table_left;
    wt_ramp_t increment, position, gain;
} wt_osc_t;

/* Serialize oscillator access on the audio owner; setters must not race rendering. */
/* Pitch is a MIDI note in Q8, clamped to 0..134 notes, including fractional semitones. */
uint32_t wt_pitch_increment(int32_t midi_q8);
void wt_osc_init(wt_osc_t *osc);
/* Both return -1 for invalid controls without changing the oscillator; callers must report it. */
int wt_osc_start(wt_osc_t *osc, const wt_controls_t *controls);
int wt_osc_set(wt_osc_t *osc, const wt_controls_t *controls);
/* Fills signed Q15 mono samples. Only start resets phase; set ramps live controls over 128 samples. */
void wt_osc_render(wt_osc_t *osc, int16_t *out, uint32_t count);
/* Non-mutating timbre preview at a phase, including current mip/frame/table blends, before gain. */
int16_t wt_osc_peek(const wt_osc_t *osc, uint32_t phase);

#endif
