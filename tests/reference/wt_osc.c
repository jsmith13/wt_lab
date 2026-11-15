/* SPDX-License-Identifier: MIT AND GPL-3.0-only
 *
 * Interpolation adapted to C from stmlib::Interpolate824 / Crossfade, used by
 * Braids' DigitalOscillator::RenderWavetables. Copyright 2012 Emilie Gillet.
 * Upstream revisions and modifications are recorded in LICENSING.md.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * WT Lab additions are GPL-3.0-only: pitch-dependent harmonic levels, exact
 * finite control ramps, queued table crossfades, bounds checking and 44.1 kHz pitch.
 */
#include "wt_osc.h"
#include "wt_data.h"

WT_INLINE int32_t wt_lerp(int32_t a, int32_t b, uint32_t fraction)
{
    return a + (int32_t)(((int64_t)(b - a) * (int32_t)fraction) >> 16);
}

WT_INLINE int32_t wt_lerp_sample(int32_t a, int32_t b, uint32_t fraction)
{
    /* Q16-weighted int16 endpoints fit int32; unsigned products make intermediate wrap defined. */
    uint32_t weighted = ((uint32_t)a << 16) + (uint32_t)(b - a) * fraction;
    return (int32_t)weighted >> 16;
}

WT_INLINE int32_t wt_read(const int16_t *wave, uint32_t phase)
{
    uint32_t i = phase >> 24;
    return wt_lerp_sample(wave[i], wave[i + 1u], (phase >> 8) & 65535u);
}

typedef struct {
    const int16_t *first, *second;
    uint32_t mix;
} wt_frame_pair_t;

WT_INLINE wt_frame_pair_t wt_frame_pair(uint32_t table, uint32_t level, uint32_t position)
{
    uint32_t p = position * (WT_FRAME_COUNT - 1u), frame = p >> 16;
    uint32_t next = frame + (frame + 1u < WT_FRAME_COUNT);
    wt_frame_pair_t pair;
    pair.first = WT_WAVES[table][level][frame];
    pair.second = WT_WAVES[table][level][next];
    pair.mix = p & 65535u;
    return pair;
}

WT_INLINE int32_t wt_pair_read(const wt_frame_pair_t *pair, uint32_t phase)
{
    int32_t a = wt_read(pair->first, phase);
    return pair->mix ? wt_lerp_sample(a, wt_read(pair->second, phase), pair->mix) : a;
}

WT_INLINE int32_t wt_frames(uint32_t table, uint32_t level, uint32_t position, uint32_t phase)
{
    wt_frame_pair_t pair = wt_frame_pair(table, level, position);
    return wt_pair_read(&pair, phase);
}

WT_INLINE int32_t wt_sample(uint32_t table, uint32_t level, uint32_t mix,
                            uint32_t position, uint32_t phase)
{
    int32_t a = wt_frames(table, level, position, phase);
    if (!mix || level + 1u == WT_LEVEL_COUNT)
        return a;
    return wt_lerp_sample(a, wt_frames(table, level + 1u, position, phase), mix);
}

static void wt_ramp_reset(wt_ramp_t *r, uint32_t value)
{
    r->current = r->target = (int32_t)value;
    r->step = r->remainder = r->error = 0;
    r->left = 0;
}

WT_INLINE void wt_ramp_set(wt_ramp_t *r, uint32_t value)
{
    int32_t delta;
    if ((int32_t)value == r->target)
        return;
    r->target = (int32_t)value;
    delta = r->target - r->current;
    r->step = delta / (int32_t)WT_SMOOTH_SAMPLES;
    r->remainder = delta % (int32_t)WT_SMOOTH_SAMPLES;
    r->error = 0;
    r->left = WT_SMOOTH_SAMPLES;
}

WT_INLINE uint32_t wt_ramp_next(wt_ramp_t *r)
{
    if (r->left) {
        r->current += r->step;
        r->error += r->remainder;
        if (r->error >= (int32_t)WT_SMOOTH_SAMPLES) {
            r->current++;
            r->error -= (int32_t)WT_SMOOTH_SAMPLES;
        } else if (r->error <= -(int32_t)WT_SMOOTH_SAMPLES) {
            r->current--;
            r->error += (int32_t)WT_SMOOTH_SAMPLES;
        }
        if (!--r->left)
            r->current = r->target;
    }
    return (uint32_t)r->current;
}

static int wt_valid(const wt_controls_t *c)
{
    return c->table < WT_TABLE_COUNT && c->position <= WT_POSITION_MAX &&
           c->increment <= WT_MAX_INCREMENT && c->gain <= WT_GAIN_MAX;
}

uint32_t wt_pitch_increment(int32_t midi_q8)
{
    uint32_t note, fraction;
    if (midi_q8 <= 0)
        return WT_PITCH[0];
    if (midi_q8 >= (int32_t)(WT_PITCH_MAX * 256u))
        return WT_PITCH[WT_PITCH_MAX];
    note = (uint32_t)midi_q8 >> 8;
    fraction = (uint32_t)midi_q8 & 255u;
    return WT_PITCH[note] + (uint32_t)(
        ((uint64_t)(WT_PITCH[note + 1u] - WT_PITCH[note]) * fraction) >> 8);
}

void wt_osc_init(wt_osc_t *s)
{
    wt_osc_t empty = {0};
    *s = empty;
}

int wt_osc_start(wt_osc_t *s, const wt_controls_t *c)
{
    if (!wt_valid(c))
        return -1;
    wt_osc_init(s);
    s->table = s->next_table = s->target_table = c->table;
    wt_ramp_reset(&s->increment, c->increment);
    wt_ramp_reset(&s->position, c->position);
    wt_ramp_reset(&s->gain, c->gain);
    return 0;
}

int wt_osc_set(wt_osc_t *s, const wt_controls_t *c)
{
    if (!wt_valid(c))
        return -1;
    s->target_table = c->table;
    wt_ramp_set(&s->increment, c->increment);
    wt_ramp_set(&s->position, c->position);
    wt_ramp_set(&s->gain, c->gain);
    return 0;
}

WT_INLINE uint32_t wt_level(uint32_t increment, uint32_t *mix)
{
    uint32_t level = 0;
    *mix = 0;
    /* Remove the richer level before its highest harmonic reaches Nyquist. */
    while (level + 1u < WT_LEVEL_COUNT && increment >= (1u << (24u + level))) level++;
    if (level + 1u < WT_LEVEL_COUNT && increment >= (1u << (23u + level)))
        *mix = (increment - (1u << (23u + level))) >> (7u + level);
    return level;
}

WT_INLINE int16_t wt_peek(const wt_osc_t *s, uint32_t phase)
{
    uint32_t pos = (uint32_t)s->position.current, mix;
    uint32_t level = wt_level((uint32_t)s->increment.current, &mix);
    int32_t sample;
    sample = wt_sample(s->table, level, mix, pos, phase);
    if (s->table_left) {
        uint32_t fade = (WT_SMOOTH_SAMPLES - s->table_left) * (65536u / WT_SMOOTH_SAMPLES);
        sample = wt_lerp_sample(sample, wt_sample(s->next_table, level, mix, pos, phase), fade);
    }
    return (int16_t)sample;
}

int16_t wt_osc_peek(const wt_osc_t *s, uint32_t phase)
{
    return wt_peek(s, phase);
}

WT_INLINE void wt_render_fixed(wt_osc_t *s, int16_t *out, uint32_t count)
{
    uint32_t increment = (uint32_t)s->increment.current, gain = (uint32_t)s->gain.current;
    uint32_t phase = s->phase, i, level, mix, frame;
    wt_frame_pair_t first, second;
    if (!gain) {
        s->phase += increment * count;
        for (i = 0; i < count && s->position.left; i++) wt_ramp_next(&s->position);
        for (i = 0; i < count; i++) out[i] = 0;
        return;
    }
    level = wt_level(increment, &mix);
    frame = (uint32_t)s->position.current * (WT_FRAME_COUNT - 1u) >> 16;
    first = wt_frame_pair(s->table, level, (uint32_t)s->position.current);
    second = first;
    if (mix) second = wt_frame_pair(s->table, level + 1u, (uint32_t)s->position.current);
    if (s->position.left) {
        for (i = 0; i < count; i++) {
            uint32_t position = wt_ramp_next(&s->position);
            uint32_t p = position * (WT_FRAME_COUNT - 1u), next_frame = p >> 16;
            int32_t sample;
            if (next_frame != frame) {
                frame = next_frame;
                first = wt_frame_pair(s->table, level, position);
                if (mix) second = wt_frame_pair(s->table, level + 1u, position);
            }
            first.mix = second.mix = p & 65535u;
            phase += increment;
            sample = wt_pair_read(&first, phase);
            if (mix) sample = wt_lerp_sample(sample, wt_pair_read(&second, phase), mix);
            out[i] = (int16_t)((sample * (int32_t)gain) >> 15);
        }
        s->phase = phase;
        return;
    }
    for (i = 0; i < count; i++) {
        int32_t sample;
        phase += increment;
        sample = wt_pair_read(&first, phase);
        if (mix) sample = wt_lerp_sample(sample, wt_pair_read(&second, phase), mix);
        out[i] = (int16_t)((sample * (int32_t)gain) >> 15);
    }
    s->phase = phase;
}

WT_DSP_CODE void wt_osc_render(wt_osc_t *s, int16_t *out, uint32_t count)
{
    uint32_t i;
    if (!count) return;
    /* Position modulation reuses its frame pointers until it crosses an actual frame boundary. */
    if (!(s->increment.left | s->gain.left | s->table_left) &&
        s->table == s->target_table) {
        wt_render_fixed(s, out, count);
        return;
    }
    for (i = 0; i < count; i++) {
        uint32_t inc = wt_ramp_next(&s->increment);
        wt_ramp_next(&s->position);
        uint32_t gain = wt_ramp_next(&s->gain);
        int32_t sample;
        if (!s->table_left && s->table != s->target_table) {
            s->next_table = s->target_table;
            s->table_left = WT_SMOOTH_SAMPLES;
        }
        s->phase += inc;
        if (s->table_left && !--s->table_left) s->table = s->next_table;
        sample = gain ? wt_peek(s, s->phase) : 0;
        out[i] = (int16_t)((sample * (int32_t)gain) >> 15);
    }
}
