/* SPDX-License-Identifier: GPL-3.0-only */
/* Link-only probe, never packaged: keep the complete WT oscillator and tables in a real FM-1 image. */
#define APP_NAME "WT PROBE"
#include "../firmware/src/platform.c"
#include "../firmware/src/wt_osc.c"

static wt_osc_t probe;
static volatile uint32_t probe_ready;

static int app_sysex(const uint8_t *p, uint32_t n)
{
    (void)p; (void)n;
    return 0;
}

static void app_audio(int32_t *out, uint32_t n)
{
    static int16_t block[64];
    uint32_t b, i, count;
    if (!probe_ready)
        return;
    for (b = 0; b < n; b += count) {
        count = n - b < 64u ? n - b : 64u;
        wt_osc_render(&probe, block, count);
        for (i = 0; i < count; i++) {
            out[2u * (b + i)] += (int32_t)block[i] * 128;
            out[2u * (b + i) + 1u] += (int32_t)block[i] * 128;
        }
    }
}

static void app_main(void)
{
    wt_controls_t c = {0, 0, 0, WT_GAIN_MAX};
    c.increment = wt_pitch_increment(60 * 256);
    if (wt_osc_start(&probe, &c) != 0) {
        draw_text_box(0, 100, 240, &FONT_S, "WT INIT ERROR", C_WHITE, 1);
        for (;;) platform_poll();
    }
    RING_PUBLISH();
    probe_ready = 1;
    draw_text_box(0, 100, 240, &FONT_S, wt_table_info[0].name, C_WHITE, 1);
    for (;;) platform_poll();
}
