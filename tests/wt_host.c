/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/src/wt_osc.c"

static uint32_t failures;
#define CHECK(test, message) do { if (!(test)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, message); failures++; } } while (0)

static wt_controls_t controls(uint32_t table, uint32_t pos, int32_t pitch)
{
    wt_controls_t c;
    c.table = table; c.position = pos;
    c.increment = wt_pitch_increment(pitch); c.gain = WT_GAIN_MAX;
    return c;
}

static void interpolation_checks(void)
{
    wt_osc_t a, b;
    int16_t x, y;
    wt_controls_t c = controls(6, WT_POSITION_MAX, 60 * 256);
    uint32_t i, level;
    c.increment = 1u << 20;
    CHECK(wt_osc_start(&a, &c) == 0, "start");
    a.phase = 0xfff00000u;
    wt_osc_render(&a, &x, 1);
    CHECK(a.phase == 0 && x == WT_WAVES[6][0][7][0], "phase wrap and final frame endpoint");
    c.table = 0; c.position = 4681; c.increment = 0;
    CHECK(wt_osc_start(&a, &c) == 0, "mid-frame start");
    a.phase = 0x30000000u;
    wt_osc_render(&a, &x, 1);
    y = (int16_t)((WT_WAVES[0][0][0][48] + WT_WAVES[0][0][1][48]) / 2);
    CHECK(abs((int)x - y) <= 2, "position interpolates adjacent frames");
    c.position = 0;
    CHECK(wt_osc_start(&a, &c) == 0, "between-samples start");
    a.phase = 0x30800000u;
    wt_osc_render(&a, &x, 1);
    y = (int16_t)((WT_WAVES[0][0][0][48] + WT_WAVES[0][0][0][49]) / 2);
    CHECK(abs((int)x - y) <= 1, "phase interpolates adjacent samples");
    c.position = WT_POSITION_MAX;
    CHECK(wt_osc_set(&a, &c) == 0 && a.phase == 0x30800000u, "editing never resets phase");
    wt_osc_render(&a, &x, 1);

    for (level = 0; level + 1u < WT_LEVEL_COUNT; level++) {
        uint32_t edge = 1u << (24u + level);
        for (i = 0; i < 128u; i++) {
            c.table = i % WT_TABLE_COUNT; c.position = (i * 521u) % (WT_POSITION_MAX + 1u);
            c.increment = edge - 1u;
            CHECK(wt_osc_start(&a, &c) == 0, "lower mip boundary");
            c.increment = edge + 1u;
            CHECK(wt_osc_start(&b, &c) == 0, "upper mip boundary");
            a.phase = b.phase = i * 0x02000000u;
            wt_osc_render(&a, &x, 1); wt_osc_render(&b, &y, 1);
            CHECK(abs((int)x - y) <= 3, "no jump crossing a harmonic-level boundary");
        }
    }
}

static void control_checks(void)
{
    wt_osc_t a, b, before;
    wt_controls_t c = controls(0, 0, 48 * 256), invalid;
    int16_t whole[512], chunks[512], last, x;
    uint32_t i, offset;
    CHECK(wt_osc_start(&a, &c) == 0, "control start");
    b = a;
    c.table = 7; c.position = WT_POSITION_MAX; c.gain = 12345;
    c.increment = wt_pitch_increment(96 * 256);
    CHECK(wt_osc_set(&a, &c) == 0 && wt_osc_set(&b, &c) == 0, "live control change");
    wt_osc_render(&a, whole, 512);
    for (offset = 0; offset < 512u;) {
        uint32_t n = offset % 31u + 1u;
        if (n > 512u - offset) n = 512u - offset;
        wt_osc_render(&b, chunks + offset, n);
        offset += n;
    }
    CHECK(memcmp(whole, chunks, sizeof whole) == 0, "render block sizes do not change the sound");
    CHECK(a.gain.current == 12345 && a.position.current == (int32_t)WT_POSITION_MAX &&
          a.increment.current == (int32_t)c.increment, "all control ramps reach their targets");
    before = a;
    for (i = 0; i < 4u; i++) {
        invalid = c;
        if (i == 0) invalid.table = WT_TABLE_COUNT;
        if (i == 1) invalid.position = WT_POSITION_MAX + 1u;
        if (i == 2) invalid.increment = WT_MAX_INCREMENT + 1u;
        if (i == 3) invalid.gain = WT_GAIN_MAX + 1u;
        CHECK(wt_osc_set(&a, &invalid) == -1 && wt_osc_start(&a, &invalid) == -1,
              "invalid controls are rejected");
        CHECK(memcmp(&a, &before, sizeof a) == 0, "rejected controls leave the note intact");
    }

    c = controls(0, WT_POSITION_MAX, 60 * 256); c.increment = 0;
    CHECK(wt_osc_start(&a, &c) == 0, "table crossfade start");
    a.phase = 0x20000000u;
    wt_osc_render(&a, &last, 1);
    for (i = 0; i < 512u; i++) {
        if (i == 0) c.table = 2;
        if (i == 17) c.table = 7;
        if (i == 34) c.table = 4;
        if (i == 162) c.table = 1;
        CHECK(wt_osc_set(&a, &c) == 0, "rapid table selection");
        wt_osc_render(&a, &x, 1);
        CHECK(abs((int)x - last) <= 471, "retargeting a table crossfade stays continuous");
        last = x;
    }
    CHECK(a.table == 1u && !a.table_left && a.phase == 0x20000000u,
          "rapid selection settles on the last table without resetting phase");

    c.table = 0; c.gain = 0;
    CHECK(wt_osc_start(&a, &c) == 0, "gain ramp start");
    a.phase = 0x40000000u; last = 0; c.gain = WT_GAIN_MAX;
    for (i = 0; i < WT_SMOOTH_SAMPLES; i++) {
        CHECK(wt_osc_set(&a, &c) == 0, "unchanged controls");
        wt_osc_render(&a, &x, 1);
        CHECK(x >= last && (int)x - last <= 235, "gain changes are smoothed, not stepped");
        last = x;
    }
    CHECK(a.gain.current == (int32_t)WT_GAIN_MAX && !a.gain.left,
          "repeated publication does not restart the ramp");
    c.gain = 0;
    CHECK(wt_osc_set(&a, &c) == 0, "fade to zero");
    wt_osc_render(&a, chunks, WT_SMOOTH_SAMPLES);
    CHECK(chunks[WT_SMOOTH_SAMPLES - 1u] == 0, "gain can reach exact silence");
}

static void range_checks(void)
{
    static const uint32_t positions[] = {0, 1, WT_POSITION_MAX / 2u, WT_POSITION_MAX - 1u, WT_POSITION_MAX};
    static const int32_t notes[] = {0, 24, 60, 96, 127, 134};
    wt_osc_t s;
    int16_t block[256];
    uint32_t table, note, pos, i;
    for (table = 0; table < WT_TABLE_COUNT; table++)
        for (note = 0; note < 7u; note++)
            for (pos = 0; pos < 5u; pos++) {
                wt_controls_t c = controls(table, positions[pos], note < 6u ? notes[note] * 256 : 0);
                if (note == 6u) c.increment = WT_MAX_INCREMENT;
                CHECK(wt_osc_start(&s, &c) == 0, "full-range playback start");
                s.phase = 0xfff00000u;
                wt_osc_render(&s, block, 256);
                for (i = 0; i < 256u; i++)
                    CHECK(block[i] >= -30000 && block[i] <= 30000,
                          "every table stays bounded at low/high pitch and frame edges");
            }
}

static void reference_render(wt_osc_t *s, int16_t *out, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; i++) {
        uint32_t increment = wt_ramp_next(&s->increment);
        uint32_t gain;
        wt_ramp_next(&s->position);
        gain = wt_ramp_next(&s->gain);
        if (!s->table_left && s->table != s->target_table) {
            s->next_table = s->target_table;
            s->table_left = WT_SMOOTH_SAMPLES;
        }
        s->phase += increment;
        if (s->table_left && !--s->table_left) s->table = s->next_table;
        out[i] = (int16_t)((wt_osc_peek(s, s->phase) * (int32_t)gain) >> 15);
    }
}

static void sample_lerp_checks(void)
{
    static const int32_t endpoints[] = {-32768, -30000, -1, 0, 1, 30000, 32767};
    uint32_t i, j, fraction;
    for (i = 0; i < 7u; i++)
        for (j = 0; j < 7u; j++)
            for (fraction = 0; fraction <= 65536u; fraction++)
                CHECK(wt_lerp_sample(endpoints[i], endpoints[j], fraction) ==
                      wt_lerp(endpoints[i], endpoints[j], fraction),
                      "32-bit sample interpolation exactly matches wide arithmetic across full Q16 fractions");
}

static void optimized_render_checks(void)
{
    static const uint32_t positions[] = {0, 1, 32768, 65535, 65536};
    static const uint32_t increments[] = {0, 1u << 20, (1u << 24) - 1u, 1u << 26, WT_MAX_INCREMENT};
    wt_osc_t optimized, reference;
    int16_t a[193], b[193];
    uint32_t i, j;
    for (i = 0; i < 80u; i++) {
        wt_controls_t c = controls(i % WT_TABLE_COUNT, positions[i % 5u], 60 * 256);
        c.increment = increments[i / 5u % 5u];
        c.gain = i & 1u ? 12345u : 0;
        CHECK(!wt_osc_start(&optimized,&c), "optimized reference start");
        optimized.phase = 0xfff00000u + i * 7919u;
        reference = optimized;
        for (j = 0; j < 5u; j++) {
            if (j == 1u || j == 3u) {
                c.table = (c.table + 3u) % WT_TABLE_COUNT;
                c.position = positions[(i+j) % 5u];
                c.increment = increments[(i+j) % 5u];
                c.gain = j == 1u ? 0u : WT_GAIN_MAX;
                CHECK(!wt_osc_set(&optimized,&c) && !wt_osc_set(&reference,&c), "reference control change");
            }
            wt_osc_render(&optimized,a,193);
            reference_render(&reference,b,193);
            CHECK(!memcmp(a,b,sizeof a) && !memcmp(&optimized,&reference,sizeof optimized),
                  "cached/silent/ramping rendering preserves every sample and complete oscillator state");
        }
    }
    for (i = 0; i < 40u; i++) {
        wt_controls_t c = controls(i % WT_TABLE_COUNT, positions[i % 5u], 60 * 256);
        c.increment = increments[i / 5u % 5u];
        c.gain = i & 1u ? WT_GAIN_MAX : 0;
        CHECK(!wt_osc_start(&optimized,&c), "position-cache reference start");
        optimized.phase = 0xfff00000u;
        reference = optimized;
        for (j = 0; j < 5u; j++) {
            c.position = positions[(i+j+1u) % 5u];
            CHECK(!wt_osc_set(&optimized,&c) && !wt_osc_set(&reference,&c), "position-only modulation");
            wt_osc_render(&optimized,a,193);
            reference_render(&reference,b,193);
            CHECK(!memcmp(a,b,sizeof a) && !memcmp(&optimized,&reference,sizeof optimized),
                  "position caching preserves samples/state across every frame and ramp endpoint");
        }
    }
}

static void put16(FILE *f, uint32_t v)
{
    fputc(v & 255u, f); fputc((v >> 8) & 255u, f);
}

static void put32(FILE *f, uint32_t v)
{
    put16(f, v); put16(f, v >> 16);
}

static void write_wave(const char *prefix, const char *suffix, const int16_t *samples, uint32_t n)
{
    char name[512];
    FILE *f;
    uint32_t i;
    if (snprintf(name, sizeof name, "%s-%s.wav", prefix, suffix) >= (int)sizeof name) {
        fprintf(stderr, "output path too long\n"); exit(1);
    }
    f = fopen(name, "wb");
    if (!f) { perror(name); exit(1); }
    fwrite("RIFF", 1, 4, f); put32(f, 36u + 2u * n); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 1); put32(f, WT_SAMPLE_RATE);
    put32(f, WT_SAMPLE_RATE * 2u); put16(f, 2); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, n * 2u);
    for (i = 0; i < n; i++) put16(f, (uint16_t)samples[i]);
    if (ferror(f)) { perror(name); fclose(f); exit(1); }
    if (fclose(f)) { perror(name); exit(1); }
    printf("audio -> %s\n", name);
}

static void audio_checks(const char *prefix)
{
    static int16_t tuning[6u * 22050u], alias[8u * 8192u];
    static int16_t demo[WT_TABLE_COUNT * WT_SAMPLE_RATE * 2u];
    static const int32_t pitches[6] = {24 * 256, 60 * 256, 69 * 256, 96 * 256, 127 * 256, 69 * 256 + 128};
    static const uint32_t bins[4] = {7u, 137u, 589u, 1477u};
    wt_osc_t s;
    wt_controls_t c;
    uint32_t t, i, offset;
    for (t = 0; t < 6u; t++) {
        c = controls(0, 0, pitches[t]);
        CHECK(wt_osc_start(&s, &c) == 0, "tuning start");
        wt_osc_render(&s, tuning + t * 22050u, 22050u);
    }
    write_wave(prefix, "tuning", tuning, 6u * 22050u);
    for (t = 0; t < 4u; t++) {
        uint32_t phase = 0;
        c = controls(0, WT_POSITION_MAX, 60 * 256);
        c.increment = bins[t] << 19;
        CHECK(wt_osc_start(&s, &c) == 0, "alias comparison start");
        wt_osc_render(&s, alias + t * 16384u, 8192u);
        for (i = 0; i < 8192u; i++) {
            phase += c.increment;
            alias[t * 16384u + 8192u + i] = (int16_t)wt_frames(0, 0, WT_POSITION_MAX, phase);
        }
    }
    write_wave(prefix, "alias", alias, 8u * 8192u);
    c = controls(0, 0, 48 * 256);
    CHECK(wt_osc_start(&s, &c) == 0, "demo start");
    for (t = 0; t < WT_TABLE_COUNT; t++) {
        c.table = t;
        for (offset = 0; offset < WT_SAMPLE_RATE * 2u;) {
            uint32_t n = WT_SAMPLE_RATE * 2u - offset;
            if (n > 64u) n = 64u;
            c.position = (uint32_t)(((uint64_t)offset * WT_POSITION_MAX) / (WT_SAMPLE_RATE * 2u - 1u));
            CHECK(wt_osc_set(&s, &c) == 0, "demo scan");
            wt_osc_render(&s, demo + t * WT_SAMPLE_RATE * 2u + offset, n);
            offset += n;
        }
    }
    for (i = 0; i < WT_TABLE_COUNT * WT_SAMPLE_RATE * 2u; i++)
        CHECK(demo[i] >= -30000 && demo[i] <= 30000, "scanning stays bounded");
    write_wave(prefix, "demo", demo, WT_TABLE_COUNT * WT_SAMPLE_RATE * 2u);
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: wt_host OUT_PREFIX\n"); return 1; }
    interpolation_checks();
    control_checks();
    range_checks();
    sample_lerp_checks();
    optimized_render_checks();
    audio_checks(argv[1]);
    printf("wavetable C checks: %u failures\n", failures);
    return failures ? 1 : 0;
}
