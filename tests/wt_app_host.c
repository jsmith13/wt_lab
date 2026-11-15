/* SPDX-License-Identifier: GPL-3.0-only */
#define DINGHY_HOST
#define APP_VER "1.2.3-abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
#define main wt_emulator_main
#include "../firmware/src/wtlab.c"
#include "../firmware/src/bootguard.h"
#undef main

static uint32_t failures;
#define CHECK(test, message) do { if (!(test)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, message); failures++; } } while (0)

static void input(uint32_t elapsed, uint32_t buttons, uint32_t keys)
{
    host_edges = buttons & ~fm1_in.buttons; host_rel = fm1_in.buttons & ~buttons;
    fm1_in.buttons = buttons; fm1_in.notes = keys; fm1_ms += elapsed; ui_poll();
}

static void tap(uint32_t button)
{
    input(1, 1u << button, fm1_in.notes); input(50, 0, fm1_in.notes);
}

static void turn(uint32_t encoder, int32_t delta)
{
    host_enc[encoder] = delta; input(1, fm1_in.buttons, fm1_in.notes);
}

static void audio(void)
{
    int32_t samples[512] = {0}; app_audio(samples, 256);
}

static void boot(void)
{
    host_flash_reset();
    fm1_ms = 0; fm1_in.buttons = fm1_in.notes = 0; host_edges = host_rel = mq_w = mq_r = 0;
    host_usb_connected = 0; batt_raw = 800;
    memset(host_enc, 0, sizeof host_enc); ui_init();
    memset(host_led_keys, 0, sizeof host_led_keys);
    memset(host_led_glow, 0, sizeof host_led_glow);
}

static void ui_checks(void)
{
    uint32_t i, gain, age, increment, clean = 1;
    wt_patch_t patch;
    wt_view_t frozen;
    wt_voice_t voice;
    int16_t samples[SP_N];
    boot();
    input(1, 1u << BTN_EDIT, 0); input(399, 0, 0);
    CHECK(page == PAGE_HOME && focus == FOCUS_EDIT, "399 ms EDIT is a HOME shortcut");
    CHECK(ui_field(0) == U_LEVEL1 && ui_field(3) == U_POSITION2,
          "short EDIT retains the HOME oscillator assignments");
    input(1,0,1u<<3); input(1,0,0);
    CHECK(selected == 1u && bank == 0u, "OP2 selects the second oscillator in the HOME edit state");
    turn(ENC_KNOB4, 1); gain = edit.osc[1].position;
    input(1, 1u << BTN_EDIT, 0); input(400, 0, 0);
    CHECK(page == PAGE_EDIT && bank == 0u && selected == 1u, "exactly 400 ms opens full EDIT once");
    ui_draw();
    for (i = 150u * 240u; i < 152u * 240u; i++) if (fb[i]) clean = 0;
    CHECK(clean, "HOME-to-editor transition clears the old graph out of row gutters");
    CHECK(ui_field(0) == U_TABLE && ui_field(3) == U_SEMI,
          "full EDIT retains its selected-oscillator parameter bank");
    turn(ENC_KNOB2, 1);
    CHECK(edit.osc[1].position == gain * 2u && !edit.osc[0].position, "both control paths edit the selected oscillator");
    tap(BTN_EDIT); CHECK(bank == 1u && ui_field(0) == U_FINE, "full-page tap advances the parameter bank");
    turn(ENC_SELECT, 1); CHECK(bank == 0u && selected == 1u, "full-page SELECT changes bank, not oscillator");
    input(1, 0, 1u << 1); input(1, 0, 0);
    CHECK(!selected && !sounding, "first operator key selects oscillator without playing a note");
    gain = edit.osc[0].gain;
    tap(BTN_SEL);
    CHECK(oscillator_mutes == 1u && edit.osc[0].gain == gain &&
          !published[patch_index].patch.osc[0].gain, "mute is outside the editable patch");
    input(1, 1u << BTN_SEL, 0); input(400, 0, 0);
    CHECK(oscillator_mutes == 2u, "SEL hold solos the selected oscillator");
    input(1, 1u << BTN_SEL, 0); input(400, 0, 0);
    CHECK(!oscillator_mutes, "repeated solo restores both oscillators");
    input(1, 1u << BTN_SEL, 0); turn(ENC_KNOB2, 1); input(500, 0, 0);
    CHECK(!oscillator_mutes && edit.osc[0].position == 328u, "fine movement consumes SEL and uses its fine step");
    input(1, 1u << BTN_SEL, 0); host_enc[ENC_ALGORITHM] = 1; input(50, 0, 0);
    CHECK(!oscillator_mutes, "encoder activity on SEL release cannot accidentally mute");
    input(1, 1u << BTN_SEL, 0); tap(BTN_HOME); input(500, 0, 0);
    CHECK(!oscillator_mutes, "navigation cancels a pending mute/solo gesture");

    tap(BTN_ENV); CHECK(focus == FOCUS_ENV && ui_field(0) == U_AMP_A, "ENV shortcut exposes amplifier ADSR");
    turn(ENC_SELECT, 1); CHECK(ui_field(0) == U_ENV_A, "ENV shortcut selects the filter/motion envelope");
    input(1, 1u << BTN_ENV, 0); input(400, 0, 0);
    CHECK(page == PAGE_ENV && bank == 1u && ui_field(0) == U_ENV_A, "held ENV preserves envelope selection");
    turn(ENC_SELECT, 1);
    CHECK(bank == 2u && ui_field(0) == U_POSENV1 && ui_field(1) == U_POSENV2, "position envelope routes have explicit oscillator targets");
    turn(ENC_KNOB2, -1);
    CHECK(edit.osc[1].position_env < 0 && !edit.osc[0].position_env, "routing does not accidentally use oscillator selection");
    tap(BTN_LFO); turn(ENC_SELECT, 1);
    CHECK(lfo_selected == 1u && ui_field(3) == U_DEST, "LFO shortcut selects the second real LFO");
    turn(ENC_KNOB4, 1);
    input(1, 1u << BTN_LFO, 0); input(400, 0, 0);
    CHECK(page == PAGE_LFO && bank == 2u && edit.lfo[1].destination == WT_MOD_POS1 &&
          edit.lfo[0].destination == WT_MOD_NONE, "full LFO and shortcut share the selected routing");
    turn(ENC_SELECT, 1); CHECK(ui_field(0) == U_SYNC, "key sync remains available in the full editor");

    boot(); tap(BTN_EDIT);
    edit.osc[0].table = 2; patch_publish();
    preferences.transpose = 12; preferences.fine = 17; preferences.bits = 2;
    preferences.output_mono = 1; midi_channel = 6; mono = 1; oscillator_mutes = 2; patch_publish();
    CHECK(ev_push(EV_ON, 60, 100) == 0, "A/B held note");
    for (i = 0; i < 50u; i++) audio();
    age = synth.age; increment = (uint32_t)synth.voice[0].osc[0].increment.current;
    input(1, 0, 1u << 25); input(50, 0, 0); audio();
    CHECK(ab == 1u && edit.osc[0].table == 0u && other.osc[0].table == 2u &&
          synth.age == age && synth.voice[0].held, "A/B swaps complete patches without retriggering a held note");
    CHECK(preferences.transpose == 12 && preferences.fine == 17 && preferences.bits == 2u &&
          preferences.output_mono == 1u && midi_channel == 6u && mono == 1u && oscillator_mutes == 2u,
          "A/B excludes global preferences and oscillator masks");
    CHECK((uint32_t)synth.voice[0].osc[0].increment.current == increment, "A/B preserves separate master tuning");
    edit.osc[0].table = 5; patch_publish();
    input(1, 0, 1u << 25); input(400, 0, 1u << 25);
    CHECK(other.osc[0].table == 5u, "held A/B copies before release");
    input(1, 0, 0);
    CHECK(ab == 1u && edit.osc[0].table == 5u, "copy release does not also swap");
    patch = edit;
    turn(ENC_ALGORITHM, 1); input(1, 0, 1u << 25); input(50, 0, 0);
    CHECK(!memcmp(&edit, &patch, sizeof patch), "copied comparison restores the complete patch");
    CHECK(wt_synth_tune(&synth, 25, 0) == -1 && wt_synth_tune(&synth, 0, 257) == -1 &&
          synth.tuning == 12 * 256 + 17, "invalid master tuning is rejected atomically");

    boot();
    input(1, 0, (1u << 25) | (1u << 1));
    CHECK(sounding == ((1u << 25) | (1u << 1)), "A/B and operator keys are notes on plain HOME");
    tap(BTN_EDIT); tap(BTN_OCT_UP);
    i = event_write; input(1, 0, 0);
    CHECK(event_write == i + 2u && events[i % WT_EVENT_COUNT].kind == EV_OFF &&
          events[i % WT_EVENT_COUNT].note == 54u && events[(i + 1u) % WT_EVENT_COUNT].note == 78u &&
          !ab && !sounding, "held shortcut keys keep original note-offs after page/octave changes");

    boot(); edit.osc[0].position = 32768; edit.lfo[0].rate = 1000;
    edit.lfo[0].depth = WT_GAIN_MAX; edit.lfo[0].destination = WT_MOD_POS1; patch_publish();
    CHECK(ev_push(EV_ON, 60, 100) == 0, "live display note"); audio();
    CHECK(view_mailbox == 2u && captured.voices == 1u &&
          captured.osc[0].position.current != (int32_t)edit.osc[0].position &&
          !memcmp(&captured.osc[0], &synth.voice[0].osc[0], sizeof(wt_osc_t)),
          "display captures the actual smoothed, modulated oscillator");
    frozen = captured; memcpy(samples, captured_audio, sizeof samples);
    audio();
    CHECK(!memcmp(&captured, &frozen, sizeof frozen) && !memcmp(captured_audio, samples, sizeof samples),
          "audio never overwrites a UI-owned capture");
    voice = synth.voice[0]; ui_draw();
    CHECK(view_mailbox == 1u && !memcmp(&voice, &synth.voice[0], sizeof voice),
          "drawing consumes the snapshot without touching audio-owned voices");
    audio(); CHECK(view_mailbox == 2u && memcmp(&captured, &frozen, sizeof frozen),
                   "releasing the mailbox permits a fresh capture");
    tap(BTN_HOME);
    CHECK(home_view == 1u && ui_field(0) == U_CUTOFF && ui_field(3) == U_SPAN, "HOME spectrum has real filter/span controls");
    ui_draw(); audio(); tap(BTN_HOME); ui_draw();
    CHECK(home_view == 2u, "HOME cycles to captured output waveform");
    tap(BTN_ENV); ui_draw(); CHECK(focus == FOCUS_ENV, "focused envelope works over a previous scope view");
    tap(BTN_HOME); CHECK(!focus && home_view == 2u, "leaving focus restores the HOME view");
    for (i = 0; i < SP_N; i++) samples[i] = (int16_t)(wt_read(WT_LFO_SIN, i * (1u << 22) * 10u) / 2);
    sp_analyze(samples);
    CHECK(sp_bin[10] > 100u && sp_bin[10] > sp_bin[30] + 30u, "native fixed-point analyser finds the captured fundamental");
    for (i = 0; i < 4u; i++) {
        int32_t value = wt_output_sample(1234567, i), step = 1 << (i == 0 ? 0 : i == 1 ? 8 : i == 2 ? 12 : 16);
        CHECK(value % step == 0 && wt_output_sample(-1234567, i) % step == 0,
              "output bit-depth preference quantizes both polarities");
    }
    CHECK(!audio_fault, "new UI paths leave the audio owner fault-free");

    boot(); host_tick_step = FM1_TICKS_PER_US*6000u; audio();
#ifdef WT_PROFILE_TEST
    CHECK(cpu_peak > 100u && cpu_late == 1u, "instrumented audio records synthetic deadline excess");
#else
    CHECK(cpu_peak == 103u && cpu_late == 1u, "app audio timing records a genuine deadline excess");
#endif
    tap(BTN_GLO); turn(ENC_SELECT,3);
    patch = edit; gain = patch_generation;
    turn(ENC_KNOB2,1); turn(ENC_KNOB3,-1); turn(ENC_KNOB4,1);
    CHECK(!memcmp(&edit,&patch,sizeof patch) && gain == patch_generation, "diagnostic rows are read-only");
    tap(BTN_SEL);
#ifdef WT_PROFILE_TEST
    CHECK(cpu_peak > 100u && cpu_late == 1u && !oscillator_mutes, "main leaves instrumented audio counters owned by audio");
#else
    CHECK(cpu_peak == 103u && cpu_late == 1u && !oscillator_mutes, "main requests meter reset without editing audio-owned counters");
#endif
#ifdef WT_PROFILE_TEST
    host_tick_step = FM1_TICKS_PER_US; audio();
    CHECK(cpu_peak < 100u && !cpu_late && cpu_reset_seen == cpu_reset_generation,
          "instrumented audio acknowledges meter reset");
#else
    host_tick_step = FM1_TICKS_PER_US*1000u; audio();
    CHECK(cpu_peak == 17u && !cpu_late && cpu_reset_seen == cpu_reset_generation, "audio acknowledges peak/deadline reset on its next block");
#endif
    {
        char value[48];
        CHECK(!strcmp(ui_value(U_CPU,value),"APP CPU") && !strcmp(value,"NOT DEVICE"),
              "host diagnostics never pretend to measure FM-1 performance");
        CHECK(!strcmp(ui_value(U_BUILD,value),"BUILD") && strlen(value) == 20u &&
              !strncmp(value,APP_VER,17) && !strcmp(value+17,"..."),
              "long release versions fit the diagnostic row without overflowing its buffer");
    }
    host_tick_step = host_tick = 0;
}

#ifdef WT_PROFILE_TEST
static void profile_checks(void)
{
    wt_profile_t frozen, empty = {0};
    uint32_t i, sum;
    boot(); host_tick_step = FM1_TICKS_PER_US*100u; host_tick = 0xfffffff0u;
    wt_midi_fx_ticks = UINT32_MAX;
    for (i = 0; i < WT_VOICES; i++) CHECK(!ev_push(EV_ON,48u+i,100), "profile voice setup");
    audio();
    CHECK(cpu_profile_peak.render > 0 && cpu_profile_peak.fx > 0, "actual callback records render and effects");
    sum = cpu_profile_peak.render+cpu_profile_peak.wait+cpu_profile_peak.fx+cpu_profile_peak.other;
    CHECK(sum <= cpu_peak && cpu_peak-sum <= 4u, "nonoverlapping callback stages account for peak within rounding");
    sum = cpu_profile_peak.events+cpu_profile_peak.prep+cpu_profile_peak.job+
          cpu_profile_peak.output+cpu_profile_peak.view+cpu_profile_peak.rest;
    CHECK(sum <= cpu_profile_peak.other && cpu_profile_peak.other-sum <= 6u,
          "OTHER subcomponents account for their parent across timer wrap");
    CHECK(cpu_profile_peak.fx_config > 0 && cpu_profile_peak.fx_config <= cpu_profile_peak.prep &&
          cpu_profile_peak.events > 0 && cpu_profile_peak.output > 0 && cpu_profile_peak.view > 0,
          "configuration is a PREP subset, and event/output/capture stages are recorded");
    CHECK(cpu_profile_peak.total == cpu_peak, "profile retains its originating peak");
#ifdef WT_CPU1
    CHECK(cpu_profile_peak.job_setup > 0 && cpu_profile_peak.job_stage > 0 &&
          cpu_profile_peak.job_send > 0 && cpu_profile_peak.job_accept > 0 &&
          cpu_profile_peak.job_mix > 0 && cpu_profile_peak.job_state > 0,
          "job profiler measures actual setup/staging/dispatch/acceptance/mix/state work");
    sum = cpu_profile_peak.job_setup+cpu_profile_peak.job_stage+cpu_profile_peak.job_send+
          cpu_profile_peak.job_accept+cpu_profile_peak.job_mix+cpu_profile_peak.job_state+
          cpu_profile_peak.job_rest;
    CHECK(sum <= cpu_profile_peak.job && cpu_profile_peak.job-sum <= 7u,
          "nonoverlapping JOB components account for their parent across timer wrap");
#endif
    view_mailbox = 1; audio(); frozen = captured.profile;
    CHECK(frozen.render > 0, "profile travels through the audio-owned capture");
    cpu_profile_peak.render = 99; audio();
    CHECK(!memcmp(&captured.profile,&frozen,sizeof frozen), "later profiles cannot tear a UI-owned capture");
    ui_draw();
    CHECK(!memcmp(&display.profile,&frozen,sizeof frozen), "display consumes a coherent peak breakdown");
    tap(BTN_GLO); turn(ENC_SELECT,3); tap(BTN_SEL);
    host_tick_step = FM1_TICKS_PER_US; view_mailbox = 1; audio();
    CHECK(cpu_reset_seen == cpu_reset_generation && cpu_profile_peak.render < 99u &&
          !memcmp(&captured.profile,&empty,sizeof empty), "SEL reset clears the old profile before measuring anew");
    host_tick_step = host_tick = 0;
}
#endif

static void control_cache_equivalence(void)
{
    wt_patch_t patch;
    fx_t effects;
    wt_prepare_controls();
    wt_midi_patch(&patch,&audio_base_patch,&effects);
    if (oscillator_mutes & 1u) patch.osc[0].gain = 0;
    if (oscillator_mutes & 2u) patch.osc[1].gain = 0;
    CHECK(!memcmp(&prepared_patch,&patch,sizeof patch) &&
          !memcmp(&prepared_effects,&effects,sizeof effects) &&
          prepared_vibrato == mi_value(MI_VIBRATO,0,-256,256),
          "cached preparation matches fresh patch, effects and vibrato at each control boundary");
}

static void control_cache_checks(void)
{
    uint32_t i, misses;
    boot(); audio();
    CHECK(control_cache_misses == 1u && control_cache_hits == 7u,
          "settled callback prepares once rather than eight times");
    for (i = 0; i < WT_VOICES; i++) ev_push(EV_ON,48u+i,100);
    audio();
    CHECK(control_cache_misses == 0u && control_cache_hits == 8u && lit == 255u,
          "cached controls survive callbacks and independent note allocation");
    edit.osc[0].position = 45000; edit.fx[1] = 75; patch_publish(); audio();
    CHECK(control_cache_misses == 1u && prepared_patch.osc[0].position == 45000u,
          "base publication invalidates the cache before rendering");
    control_cache_equivalence();
    oscillator_mutes = 1; control_cache_equivalence();
    CHECK(!prepared_patch.osc[0].gain, "mute is part of the cache key");
    oscillator_mutes = 0; control_cache_equivalence();
    CHECK(prepared_patch.osc[0].gain == audio_base_patch.osc[0].gain,
          "unmute restores the original gain");

    mi_receive(0xB0,74,127);
    control_cache_equivalence();
    CHECK(!prepared_valid && prepared_midi_generation == mi_seen,
          "pending publication cannot make the previous audio inputs cacheable");
    for (i = 0; i < 28u; i++) {
        mi_tick(32); control_cache_equivalence();
        if (i < 27u) CHECK(!prepared_valid, "moving route mix never leaves an eligible cached result");
    }
    CHECK(prepared_valid && prepared_patch.cutoff == WT_CUTOFF_MAX,
          "final partial ramp block recomputes before becoming cacheable");
    misses = control_cache_misses; mi_tick(32); control_cache_equivalence();
    CHECK(control_cache_misses == misses, "final ramp result is reused once settled");
    mi_receive(0xB0,74,0);
    for (i = 0; i < 28u; i++) { mi_tick(32); control_cache_equivalence(); }
    CHECK(!prepared_patch.cutoff, "value-ramp endpoint is not replaced by stale cached controls");

    mi_config.route[10].source = 21; mi_config.route[10].target = 36; mi_publish();
    mi_receive(0xB0,21,127); mi_receive(0xB0,1,127);
    for (i = 0; i < 28u; i++) { mi_tick(32); control_cache_equivalence(); }
    CHECK(prepared_patch.fx[1] == 99u && prepared_vibrato > 0,
          "mapping/FX publication and vibrato update together");
    mi_receive(0xB0,1,0);
    for (i = 0; i < 28u; i++) { mi_tick(32); control_cache_equivalence(); }
    CHECK(!prepared_vibrato, "final vibrato ramp returns exactly to zero");
    mi_receive(0xE0,127,127);
    for (i = 0; i < 28u; i++) {
        mi_tick(32); control_cache_equivalence();
        if (i < 27u) CHECK(!prepared_valid, "bend motion retains the conservative uncached path");
    }
    CHECK(prepared_valid && mi_pitch() == 512, "bend endpoint retains its original timing and tuning");
    mi_input.suppressed |= 1u<<4; mi_publish();
    for (i = 0; i < 28u; i++) { mi_tick(32); control_cache_equivalence(); }
    CHECK(prepared_patch.cutoff == audio_base_patch.cutoff,
          "route suppression fades back to the base rather than retaining cached MIDI");

    mi_generation = UINT32_MAX; mi_publish(); mi_tick(32); control_cache_equivalence();
    CHECK(!mi_seen && prepared_midi_generation == 0u, "MIDI generation wrap invalidates normally");
    patch_generation = UINT32_MAX; edit.osc[0].position = 12345; patch_publish(); audio();
    CHECK(!seen_generation && prepared_patch.osc[0].position == 12345u,
          "base generation wrap does not restore a previous cache entry");
    boot(); audio();
    CHECK(control_cache_misses == 1u, "warm UI initialization invalidates all prior prepared controls");
}

static void octave_led_checks(void)
{
    static const uint32_t times[] = {899,900,1499,1500,1799,1800};
    static const uint8_t bright[] = {0,1,1,0,0,1};
    uint32_t i, j, button, opposite;
    int32_t direction;
    for (i = 0; i < 2u; i++) {
        boot(); input(1,0,0);
        button = i ? BTN_OCT_UP : BTN_OCT_DOWN;
        opposite = i ? BTN_OCT_DOWN : BTN_OCT_UP;
        direction = i ? 1 : -1;
        CHECK(!host_led_keys[button] && !host_led_keys[opposite] &&
              !host_led_glow[button] && !host_led_glow[opposite],
              "unshifted octave leaves both button light layers off");
        tap(button);
        CHECK(octave == direction && host_led_keys[button] && !host_led_keys[opposite] &&
              !host_led_glow[button] && !host_led_glow[opposite],
              "first octave press lights only that direction steadily");
        fm1_ms = 600; input(0,0,0);
        CHECK(host_led_keys[button], "one-octave light remains bright during the two-octave pulse's dim phase");
        tap(button);
        CHECK(octave == direction*2 && !host_led_keys[button] && host_led_glow[button],
              "second octave press reaches the limit and enables its dim glow under the pulse");
        for (j = 0; j < sizeof times/sizeof times[0]; j++) {
            fm1_ms = times[j]; input(0,0,0);
            CHECK(host_led_keys[button] == bright[j] && host_led_glow[button] &&
                  !host_led_keys[opposite] && !host_led_glow[opposite],
                  "two-octave light uses FM's 600ms bright/300ms dim cadence without going fully dark");
        }
        tap(button); tap(BTN_ENV);
        CHECK(octave == direction*2 && host_led_glow[button] && !host_led_glow[opposite],
              "extra octave presses clamp at the limit and shortcut navigation keeps its pulse");
        tap(opposite);
        CHECK(octave == direction && host_led_keys[button] && !host_led_glow[button] &&
              !host_led_keys[opposite] && !host_led_glow[opposite],
              "backing away from the limit restores steady light and clears its glow");
        tap(opposite);
        CHECK(!octave && !host_led_keys[button] && !host_led_glow[button] &&
              !host_led_keys[opposite] && !host_led_glow[opposite],
              "returning to zero clears both octave light layers");
        tap(opposite);
        CHECK(octave == -direction && host_led_keys[opposite] && !host_led_glow[opposite] &&
              !host_led_keys[button] && !host_led_glow[button],
              "crossing zero transfers the steady indicator to the other direction");
    }
}

static uint32_t pixels_with(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t color)
{
    uint32_t i, j, count = 0;
    for (j = y; j < y+h; j++)
        for (i = x; i < x+w; i++) count += fb[j*240u+i] == color;
    return count;
}

static int text_at(uint32_t y, const char *text, uint16_t color)
{
    uint32_t i;
    cv_begin(240, 16, C_BLACK);
    cv_text((240-text_w(&FONT_S,text))/2, 0, &FONT_S, text, color);
    for (i = 0; i < 240u*16u; i++)
        if (fb[y*240u+i] != swap16(cv_px[i])) return 0;
    return 1;
}

static int panel_text_at(uint32_t oscillator, uint32_t y, const char *text, uint16_t color)
{
    uint32_t i, j;
    cv_begin(120, 16, C_BLACK);
    cv_text((120-text_w(&FONT_S,text))/2, 0, &FONT_S, text, color);
    for (j = 0; j < 16u; j++)
        for (i = 0; i < 119u; i++)
            if (fb[(y+j)*240u+oscillator*120u+i] != swap16(cv_px[j*120u+i])) return 0;
    return 1;
}

static int title_at(const char *text)
{
    uint32_t i;
    cv_begin(240,26,C_BLACK);
    cv_text((240-text_w(&FONT_L,text))/2,0,&FONT_L,text,WT_ACC);
    for (i = 0; i < 240u*26u; i++)
        if (fb[18u*240u+i] != swap16(cv_px[i])) return 0;
    return 1;
}

static void layout_checks(void)
{
    uint32_t i, writes;
    boot(); fm1_ms = 2500; ui_draw();
    CHECK(pixels_with(0, 18, 240, 26, WT_ACC) > 100 &&
          pixels_with(0, 44, 240, 16, C_WHITE) > 50,
          "HOME uses the large centered lilac banner and a separate white patch line");
    CHECK(pixels_with(32, 192, 84, 4, C_WHITE) == 336u &&
          pixels_with(152, 192, 84, 4, RGB(40,40,40)) == 336u &&
          pixels_with(0, 224, 240, 16, C_BLACK) == 240u*16u,
          "both HOME panels have their own level bars without a permanent value strip");
    writes = host_lcd_rows[44]; ui_draw();
    CHECK(host_lcd_rows[44] == writes+1u,
          "steady HOME redraws never paint the title over the top of the patch line");
    turn(ENC_KNOB2, 1); ui_draw();
    CHECK(text_at(44, "POSITION 1 5%", WT_ACC) &&
          pixels_with(0, 224, 240, 16, C_BLACK) == 240u*16u,
          "HOME knob feedback replaces the patch name, not the area below the hints");
    fm1_ms += 1499; ui_draw();
    CHECK(text_at(44, "POSITION 1 5%", WT_ACC), "knob feedback lasts the FM Lab 1.5-second interval");
    fm1_ms++; ui_draw();
    CHECK(!text_at(44, "POSITION 1 5%", WT_ACC) && pixels_with(0,44,240,16,WT_ACC) > 50,
          "the modified patch line returns after knob feedback expires");
    turn(ENC_KNOB2, 1); say("TEST NOTICE"); ui_draw();
    CHECK(text_at(44, "TEST NOTICE", WT_ACC), "a new notification supersedes transient knob feedback");

    input(1, 1u << BTN_EDIT, 0); input(400, 0, 0);
    edit.osc[0].gain = WT_GAIN_MAX/2u; ui_draw();
    CHECK(pixels_with(6, 144, 114, 4, WT_ACC) == 114u*4u &&
          pixels_with(120, 144, 114, 4, RGB(40,40,40)) == 114u*4u,
          "full editor rows show the actual half-scale level in a full-width amount bar");
    for (i = 0; i < 4u; i++)
        CHECK(pixels_with(0, 86u+i*32u, 240, 2, C_BLACK) == 480u,
              "full editor rows have clean 32-pixel spacing after leaving HOME");
    CHECK(pixels_with(0, 188, 240, 16, WT_ACC) > 50 &&
          pixels_with(0, 208, 240, 16, WT_OFF) > 20,
          "editor patch line and subdued knob legend match the FM Lab footer positions");

    input(1, 1u << BTN_ENV, 0); input(400, 0, 0); ui_draw();
    CHECK(pixels_with(0, 44, 240, 104, WT_ACC) > 100 &&
          pixels_with(0, 148, 240, 4, C_BLACK) == 960u,
          "envelope uses the tall graph above a distinct four-cell strip");
    for (i = 0; i < 4u; i++)
        CHECK(pixels_with(i*60u+3u, 178, 54, 4, RGB(40,40,40)) > 0,
              "each envelope knob has its own compact amount bar");
    input(1, 1u << BTN_LFO, 0); input(400, 0, 0); turn(ENC_SELECT, 1); ui_draw();
    CHECK(pixels_with(60, 152, 180, 32, C_BLACK) == 180u*32u &&
          pixels_with(0, 44, 240, 104, WT_ACC) > 100,
          "LFO sync bank keeps its plot and clears all unused parameter cells");

    tap(BTN_GLO); turn(ENC_SELECT, 3); ui_draw();
    CHECK(pixels_with(0, 24, 240, 16, WT_ACC) > 20 &&
          pixels_with(0, 44, 240, 72, C_WHITE) > 20 &&
          pixels_with(0, 224, 240, 16, WT_OFF) > 20,
          "diagnostics retain their readout with a centered header and bottom status line");
    CHECK(pixels_with(6, 3, 17, 1, WT_OFF) == 17 &&
          pixels_with(26, 0, 10, 16, C_BLACK) == 160u,
          "a long build version stays in the top-right corner without overwriting the battery");
    tap(BTN_HOME); tap(BTN_SAVE); ui_draw();
    CHECK(pixels_with(0, 58, 240, 2, WT_ACC) > 0,
          "SAVE underlines the name cursor in the patch line");
    CHECK(pixels_with(32, 193, 84, 1, C_BLACK) == 0 &&
          pixels_with(152, 193, 84, 1, C_BLACK) == 0,
          "SAVE keeps both full-height HOME oscillator panels visible");
}

static void output_quality_checks(void)
{
    uint32_t i, rate, updates, writes;
    int32_t full[512] = {0}, held[512] = {0};
    char value[48];
    boot(); tap(BTN_GLO); turn(ENC_SELECT,2); ui_draw();
    CHECK(ui_field(0) == U_VELOCITY && ui_field(1) == U_SRATE &&
          ui_field(2) == U_BITS && ui_field(3) == U_OUTPUT,
          "GLO places sample rate directly above bit depth without losing velocity/output controls");
    CHECK(ui_amount(U_SRATE,228) == 228u && ui_amount(U_BITS,228) == 228u &&
          pixels_with(6,112,228,4,WT_ACC) == 912u && pixels_with(6,144,228,4,WT_ACC) == 912u,
          "default 44.1 kHz and 24-bit settings show completely full bars");
    turn(ENC_KNOB2,-5); turn(ENC_KNOB3,-3); ui_draw();
    CHECK(preferences.sample_rate == 5u && preferences.bits == 3u &&
          !ui_amount(U_SRATE,228) && !ui_amount(U_BITS,228) &&
          pixels_with(6,112,228,4,RGB(40,40,40)) == 912u &&
          pixels_with(6,144,228,4,RGB(40,40,40)) == 912u,
          "counterclockwise turns reach 8 kHz/8-bit with empty bars");
    turn(ENC_KNOB2,1); turn(ENC_KNOB3,1);
    CHECK(preferences.sample_rate == 4u && preferences.bits == 2u &&
          ui_amount(U_SRATE,228) > 0u && ui_amount(U_BITS,228) == 76u &&
          !strcmp(ui_value(U_BITS,value),"BIT DEPTH") && !strcmp(value,"12 BIT"),
          "clockwise turns increase rate, bit depth and bar fill while retaining legacy bit encodings");
    ui_edit(U_SRATE,100); ui_edit(U_BITS,100);
    CHECK(!preferences.sample_rate && !preferences.bits &&
          !strcmp(ui_value(U_SRATE,value),"SAMPLE RATE") && !strcmp(value,"44.1K"),
          "large quality adjustments clamp at full native rate and resolution");

    for (i = 0; i < 4u; i++) {
        writes = host_lcd_rows[56u+i*32u]; ui_row_draw(i,56u+i*32u,0);
        CHECK(host_lcd_rows[56u+i*32u] == writes+1u &&
              pixels_with(150,56u+i*32u,84,16,C_WHITE) > 0u,
              "each long-menu row transfers its label, value and bar together without a blank-value pass");
    }
    writes = host_lcd_rows[120]; ui_row_draw(1,120,1);
    CHECK(host_lcd_rows[120] == writes+1u, "compact diagnostic rows also use one coherent text transfer");

    wt_sample_rate_reset(2);
    for (i = 0; i < 8u; i++)
        CHECK(wt_sample_rate_sample((int32_t)i,22050) == (int32_t)(i&~1u),
              "22.05 kHz holds each sampled value for exactly two native output frames");
    for (rate = 1; rate < WT_SAMPLE_RATE_COUNT; rate++) {
        int32_t previous = -1;
        wt_sample_rate_reset(rate); updates = 0;
        for (i = 0; i < FS; i++) {
            int32_t sample = wt_sample_rate_sample((int32_t)i,WT_SAMPLE_RATES[rate]);
            if (sample != previous) { updates++; previous = sample; }
        }
        CHECK(updates == WT_SAMPLE_RATES[rate], "each fractional sample-and-hold clock produces its exact update rate");
    }
    wt_sample_rate_reset(5); wt_sample_rate_sample(123,8000);
    wt_sample_rate_reset(1);
    CHECK(wt_sample_rate_sample(-456,32000) == -456, "a changed rate captures fresh audio immediately, not a stale held sample");

    boot(); ev_push(EV_ON,60,100); app_audio(full,256);
    boot(); preferences.sample_rate = 2; patch_publish(); ev_push(EV_ON,60,100); app_audio(held,256);
    for (i = 0; i < 256u; i++)
        CHECK(held[2u*i] == full[2u*(i&~1u)] && held[2u*i+1u] == full[2u*(i&~1u)+1u],
              "published sample rate holds actual native voice output before the stereo effects");
}

static void oscillator_home_checks(void)
{
    uint32_t i, j;
    uint16_t plot[240u*SP_H];
    wt_patch_t expected;
    static const char *const primary[] = {"LVL 1","POS 1","LVL 2","POS 2"};
    static const char *const pitch[] = {"DET 1","PENV 1","DET 2","PENV 2"};
    char value[48];
    boot(); fm1_ms = 2500; edit.osc[0].gain = WT_GAIN_MAX/2u; expected = edit;
    for (i = 0; i < 4u; i++)
        CHECK(!strcmp(ui_legend(ui_field(i)),primary[i]), "plain HOME identifies both oscillators in its mix/position shortcuts");
    turn(ENC_KNOB1,1); expected.osc[0].gain += 1638;
    turn(ENC_KNOB2,1); expected.osc[0].position += 3277;
    turn(ENC_KNOB3,2); expected.osc[1].gain += 3276;
    turn(ENC_KNOB4,3); expected.osc[1].position += 9831;
    CHECK(!memcmp(&edit,&expected,sizeof edit) && selected == 1u,
          "HOME edits level/position of each explicit oscillator and selects the last edited side");
    ui_draw();
    CHECK(text_at(44,"POSITION 2 15%",WT_ACC), "HOME feedback identifies the actual oscillator and edited value");
    turn(ENC_SELECT,1);
    for (i = 0; i < 4u; i++)
        CHECK(!strcmp(ui_legend(ui_field(i)),pitch[i]), "SELECT exposes detune/position-envelope controls for both oscillators");
    turn(ENC_KNOB1,3); expected.osc[0].semitone += 3;
    turn(ENC_KNOB2,-2); expected.osc[0].position_env -= 6554;
    turn(ENC_KNOB3,-4); expected.osc[1].semitone = -3; expected.osc[1].fine = -249;
    turn(ENC_KNOB4,2); expected.osc[1].position_env += 6554;
    CHECK(!memcmp(&edit,&expected,sizeof edit), "HOME detune bank preserves fractional tuning and changes only its named parameters");
    input(1,1u<<BTN_SEL,0); turn(ENC_KNOB1,13); input(50,0,0);
    expected.osc[0].fine = 13;
    CHECK(!memcmp(&edit,&expected,sizeof edit) && !oscillator_mutes &&
          !strcmp(ui_value(U_DETUNE1,value),"DETUNE 1") && !strcmp(value,"+3.051ST"),
          "SEL-held detune uses fine pitch steps with readable precision and never triggers mute");
    ui_edit(U_DETUNE1,1000);
    CHECK(edit.osc[0].semitone == 24 && edit.osc[0].fine == 256 && wt_patch_valid(&edit),
          "combined detune reaches the existing upper pitch limit without invalidating the patch");
    ui_edit(U_DETUNE1,-2000);
    CHECK(edit.osc[0].semitone == -24 && edit.osc[0].fine == -256 && wt_patch_valid(&edit),
          "combined detune safely crosses zero and reaches the existing lower pitch limit");
    turn(ENC_SELECT,-1);
    CHECK(ui_field(0) == U_LEVEL1 && ui_field(3) == U_POSITION2, "SELECT returns to the mix/position bank in either direction");
    turn(ENC_KNOB3,1);
    edit.osc[1].table = 7u; turn(ENC_ALGORITHM,1);
    CHECK(edit.osc[1].table == 8u, "selection advances from existing vocal table into appended strings");
    edit.osc[1].table = WT_TABLE_COUNT-1u;
    turn(ENC_ALGORITHM,1);
    CHECK(!edit.osc[1].table && edit.osc[0].table == expected.osc[0].table,
          "algorithm knob wraps the last table to SINE-SAW on the last edited oscillator only");
    turn(ENC_ALGORITHM,-1);
    CHECK(edit.osc[1].table == WT_TABLE_COUNT-1u, "table selection wraps backwards to the last table");
    turn(ENC_ALGORITHM,9); turn(ENC_ALGORITHM,-9);
    CHECK(edit.osc[1].table == WT_TABLE_COUNT-1u, "table selection handles multi-revolution encoder deltas");
    home_view = 2; turn(ENC_SELECT,1);
    CHECK(ui_field(0) == U_DETUNE1 && ui_field(3) == U_PENV2, "scope HOME has the same two oscillator-control banks");
    home_view = 1;
    CHECK(ui_field(0) == U_CUTOFF && ui_field(3) == U_SPAN, "filter HOME retains filter/span controls regardless of the oscillator bank");

    for (i = 0; i < 3u; i++) {
        uint32_t fields[4], events_before, unchanged = 1;
        boot(); fm1_ms = 2500; home_view = i; bank = 1; view_mailbox = 2; ui_draw();
        memcpy(plot,fb+64u*240u,sizeof plot);
        for (j = 0; j < 4u; j++) fields[j] = ui_field(j);
        tap(BTN_EDIT); view_mailbox = 2; ui_draw();
        for (j = 0; j < 4u; j++) if (fields[j] != ui_field(j)) unchanged = 0;
        CHECK(page == PAGE_HOME && focus == FOCUS_EDIT && unchanged &&
              !memcmp(plot,fb+64u*240u,sizeof plot),
              "short EDIT preserves each HOME visualization and its current knob bank");
        CHECK(title_at("EDIT WT 1") && host_led_keys[KEY_LED(1)] &&
              !host_led_keys[KEY_LED(3)] && !host_led_keys[KEY_LED(20)] && host_led_keys[KEY_LED(22)],
              "HOME EDIT identifies oscillator 1 and lights OP1/POLY rather than unselected shortcuts");
        events_before = event_write;
        input(1,0,1u<<3); input(50,0,0); CHECK(selected == 1u, "OP2 selects oscillator 2 in every HOME edit view");
        ui_draw();
        CHECK(title_at("EDIT WT 2") && !host_led_keys[KEY_LED(1)] && host_led_keys[KEY_LED(3)],
              "selecting OP2 updates the EDIT title and moves its selected-key light");
        input(1,0,1u<<1); input(50,0,0); CHECK(!selected, "OP1 selects oscillator 1 in every HOME edit view");
        input(1,0,1u<<20); input(50,0,0); CHECK(mono, "MONO selects mono mode without playing a note");
        CHECK(host_led_keys[KEY_LED(20)] && !host_led_keys[KEY_LED(22)], "MONO/POLY lights follow the selected mode");
        input(1,0,1u<<22); input(50,0,0); CHECK(!mono, "POLY restores poly mode without playing a note");
        CHECK(!host_led_keys[KEY_LED(20)] && host_led_keys[KEY_LED(22)], "POLY restores its selected-mode light");
        input(1,0,1u<<25); input(50,0,0);
        CHECK(ab == 1u && !sounding && event_write == events_before+2u &&
              events[events_before%WT_EVENT_COUNT].kind == EV_MONO &&
              events[(events_before+1u)%WT_EVENT_COUNT].kind == EV_MONO,
              "high F-sharp toggles A/B and all edit-state shortcut keys generate no notes");
        tap(BTN_HOME);
        CHECK(!focus && home_view == i, "HOME exits edit state without changing the underlying HOME view");
        CHECK(!host_led_keys[KEY_LED(1)] && !host_led_keys[KEY_LED(3)] &&
              !host_led_keys[KEY_LED(20)] && !host_led_keys[KEY_LED(22)],
              "leaving EDIT removes shortcut lights when those keys are not physically held");
    }

    boot();
    edit.osc[0].gain = edit.osc[1].gain = WT_GAIN_MAX;
    for (i = 0; i < WT_TABLE_COUNT; i++) {
        display.osc[0].table = i; display.osc[1].table = (i+1u)%WT_TABLE_COUNT;
        selected = i&1u; ui_wave_draw(SP_H);
        CHECK(panel_text_at(0,WT_PLOT_Y,wt_table_info[i].name,selected == 0u ? WT_ACC : WT_LABEL) &&
              panel_text_at(1,WT_PLOT_Y,wt_table_info[(i+1u)%WT_TABLE_COUNT].name,selected == 1u ? WT_ACC : WT_LABEL),
              "each HOME heading shows its wavetable name and accents only the selected oscillator");
        CHECK(panel_text_at(0,WT_PLOT_Y+18u,"",C_BLACK) && panel_text_at(1,WT_PLOT_Y+18u,"",C_BLACK),
              "active HOME panels leave the former duplicate-name row clear");
    }
    edit.osc[0].gain = edit.osc[1].gain = 0; oscillator_mutes = 1; ui_wave_draw(SP_H);
    CHECK(panel_text_at(0,WT_PLOT_Y,wt_table_info[display.osc[0].table].name,WT_LABEL) &&
          panel_text_at(1,WT_PLOT_Y,wt_table_info[display.osc[1].table].name,WT_ACC) &&
          panel_text_at(0,WT_PLOT_Y+18u,"OFF",WT_OFF) && panel_text_at(1,WT_PLOT_Y+18u,"SILENT",WT_OFF),
          "muted and zero-level panels retain their table headings and distinct status indications");

    boot(); edit.osc[0].gain = WT_GAIN_MAX/2u;
    display.osc[0].position.current = WT_POSITION_MAX; ui_wave_draw(SP_H);
    CHECK(pixels_with(32,192,42,4,C_WHITE) == 168u &&
          pixels_with(74,192,42,4,RGB(40,40,40)) == 168u &&
          pixels_with(152,192,84,4,RGB(40,40,40)) == 336u && !display.voices,
          "level bars show half/zero configured levels even without a sounding note");
    CHECK(pixels_with(32,172,1,9,WT_LABEL) == 9u &&
          pixels_with(114,171,2,11,WT_ACC) == 22u,
          "the position bar separates its base marker from the live endpoint without overflowing the panel");
    for (i = 1; i < 7u; i++)
        CHECK(pixels_with(32u+i*82u/7u,173,1,7,RGB(70,70,70)) == 7u,
              "position bars retain the six interior frame ticks between their marked endpoints");
    oscillator_mutes = 1; ui_wave_draw(SP_H);
    CHECK(pixels_with(32,192,42,4,WT_OFF) == 168u &&
          pixels_with(32,192,84,4,C_WHITE) == 0u && edit.osc[0].gain == WT_GAIN_MAX/2u,
          "muting dims the level bar without losing its configured level");
    edit.osc[1].gain = WT_GAIN_MAX; display.osc[1].position.current = WT_POSITION_MAX; ui_wave_draw(SP_H);
    CHECK(pixels_with(152,192,84,4,C_WHITE) == 336u &&
          pixels_with(234,171,2,11,WT_ACC) == 22u &&
          pixels_with(119,64,1,SP_H,RGB(44,44,44)) == SP_H,
          "full level and position stay inside oscillator 2 without overwriting the panel divider");

    boot(); input(1,1u<<BTN_EDIT,0); input(400,0,0); ui_draw();
    CHECK(text_at(40,"F1>2  MIX 0%",WT_LABEL) && ui_field(0) == U_TABLE,
          "full EDIT retains exact frame/blend details above its selected-oscillator controls");
    edit.osc[0].table = 0; turn(ENC_KNOB1,-1);
    CHECK(edit.osc[0].table == WT_TABLE_COUNT-1u, "table selection also wraps through EDIT's table knob");
}

static void envelope_home_checks(void)
{
    uint32_t i, j;
    uint16_t plot[240u*SP_H];
    wt_patch_t expected;
    for (i = 0; i < 3u; i++) {
        boot(); fm1_ms = 2500; home_view = i; bank = 1; env_selected = 1;
        edit.amp.sustain = edit.envelope.sustain = WT_GAIN_MAX/2u; expected = edit;
        view_mailbox = 2; ui_draw(); memcpy(plot,fb+WT_PLOT_Y*240u,sizeof plot);
        ui_spectrum_peak[5] = 123;
        tap(BTN_ENV); view_mailbox = 0; ui_draw();
        CHECK(page == PAGE_HOME && home_view == i && focus == FOCUS_ENV && !env_selected &&
              !memcmp(plot,fb+WT_PLOT_Y*240u,sizeof plot) && ui_spectrum_peak[5] == 123,
              "short ENV starts with AMP controls and preserves every HOME plot and spectrum peak");
        for (j = 0; j < 4u; j++) {
            CHECK(ui_field(j) == U_AMP_A+j, "all four ENV shortcut knobs initially target AMP ADSR");
            turn(ENC_KNOB1+j,1);
        }
        expected.amp.attack += 25; expected.amp.decay += 25;
        expected.amp.sustain += 1638; expected.amp.release += 25;
        CHECK(!memcmp(&edit,&expected,sizeof edit), "AMP shortcut knobs change only their four amplifier parameters");
        turn(ENC_SELECT,1); view_mailbox = 0; ui_draw();
        CHECK(env_selected == 1u && !memcmp(plot,fb+WT_PLOT_Y*240u,sizeof plot) && ui_spectrum_peak[5] == 123,
              "SELECT switches to the motion/filter envelope without clearing or replacing HOME");
        for (j = 0; j < 4u; j++) {
            CHECK(ui_field(j) == U_ENV_A+j, "the alternate ENV shortcut bank targets motion/filter ADSR");
            turn(ENC_KNOB1+j,1);
        }
        expected.envelope.attack += 25; expected.envelope.decay += 25;
        expected.envelope.sustain += 1638; expected.envelope.release += 25;
        CHECK(!memcmp(&edit,&expected,sizeof edit), "motion shortcut knobs leave AMP and oscillator parameters unchanged");
        turn(ENC_SELECT,-1); CHECK(!env_selected, "SELECT cycles back to AMP in either direction");
        tap(BTN_ENV); CHECK(env_selected == 1u, "repeated short ENV retains its existing bank-cycle gesture");
        input(1,1u<<BTN_ENV,0); input(400,0,0); ui_draw();
        CHECK(page == PAGE_ENV && bank == 1u && ui_field(0) == U_ENV_A &&
              pixels_with(0,WT_ENV_Y,240,WT_ENV_H,WT_ACC) > 100,
              "long ENV still opens the selected full envelope editor with its graph");
        turn(ENC_SELECT,1);
        CHECK(bank == 2u && ui_field(0) == U_POSENV1 && ui_field(3) == U_TRACK,
              "the full ENV routing bank remains available");
        tap(BTN_HOME); tap(BTN_ENV);
        CHECK(page == PAGE_HOME && home_view == i && !env_selected && ui_field(3) == U_AMP_R,
              "a new short ENV session starts at AMP even after the full routing editor");
        tap(BTN_HOME); view_mailbox = 0; ui_draw();
        CHECK(!focus && home_view == i, "HOME exits ENV controls without cycling the underlying view");
    }
}

static void lfo_home_checks(void)
{
    uint32_t i, j, first;
    uint16_t plot[240u*SP_H];
    wt_patch_t expected;
    for (i = 0; i < 3u; i++) {
        boot(); fm1_ms = 2500; home_view = i; bank = 1; first = lfo_selected = i%2u;
        for (j = 0; j < 2u; j++) {
            edit.lfo[j].rate = 1000; edit.lfo[j].depth = WT_GAIN_MAX/2u;
        }
        expected = edit;
        view_mailbox = 2; ui_draw(); memcpy(plot,fb+WT_PLOT_Y*240u,sizeof plot);
        ui_spectrum_peak[5] = 123;
        tap(BTN_LFO); view_mailbox = 0; ui_draw();
        CHECK(page == PAGE_HOME && home_view == i && focus == FOCUS_LFO &&
              lfo_selected == first && !memcmp(plot,fb+WT_PLOT_Y*240u,sizeof plot) &&
              ui_spectrum_peak[5] == 123 && title_at(first ? "LFO 2" : "LFO 1"),
              "short LFO retains its selected target, every HOME plot and spectrum peak");
        for (j = 0; j < 4u; j++) {
            CHECK(ui_field(j) == U_RATE+j, "all four LFO shortcut knobs expose rate/shape/depth/destination");
            turn(ENC_KNOB1+j,1);
        }
        expected.lfo[first].rate += 250; expected.lfo[first].shape++;
        expected.lfo[first].depth += 1638; expected.lfo[first].destination++;
        CHECK(!memcmp(&edit,&expected,sizeof edit), "LFO shortcut knobs change only the selected LFO's four parameters");
        input(1,1u<<BTN_SEL,0); turn(ENC_KNOB1,1); turn(ENC_KNOB3,1); input(500,0,0);
        expected.lfo[first].rate += 10; expected.lfo[first].depth += 164;
        CHECK(!memcmp(&edit,&expected,sizeof edit) && !oscillator_mutes && lfo_selected == first,
              "SEL-held LFO edits retain fine rate/depth steps without muting or changing target");
        turn(ENC_SELECT,1); view_mailbox = 0; ui_draw();
        CHECK(lfo_selected == (first^1u) && title_at(first ? "LFO 1" : "LFO 2") &&
              !memcmp(plot,fb+WT_PLOT_Y*240u,sizeof plot) && ui_spectrum_peak[5] == 123,
              "SELECT switches LFO and title without clearing or replacing HOME");
        for (j = 0; j < 4u; j++) turn(ENC_KNOB1+j,1);
        expected.lfo[first^1u].rate += 250; expected.lfo[first^1u].shape++;
        expected.lfo[first^1u].depth += 1638; expected.lfo[first^1u].destination++;
        CHECK(!memcmp(&edit,&expected,sizeof edit), "the alternate LFO shortcut target leaves the first LFO and all other patch fields unchanged");
        turn(ENC_SELECT,-1); CHECK(lfo_selected == first, "SELECT cycles LFOs in either direction");
        tap(BTN_LFO); view_mailbox = 0; ui_draw();
        CHECK(lfo_selected == (first^1u) && !memcmp(plot,fb+WT_PLOT_Y*240u,sizeof plot),
              "another short LFO tap switches target while preserving HOME");
        input(1,1u<<BTN_LFO,0); input(400,0,0); ui_draw();
        CHECK(page == PAGE_LFO && bank == lfo_selected*2u && ui_field(3) == U_DEST &&
              pixels_with(0,WT_ENV_Y,240,WT_ENV_H,WT_ACC) > 100,
              "long LFO still opens the selected full editor with its waveform and primary controls");
        turn(ENC_SELECT,1); turn(ENC_KNOB1,1); ui_draw();
        expected.lfo[first^1u].key_sync = 1;
        CHECK(ui_field(0) == U_SYNC && ui_field(3) == U_NONE &&
              !memcmp(&edit,&expected,sizeof edit) &&
              pixels_with(60,WT_CELLS_Y,180,32,C_BLACK) == 180u*32u,
              "the full LFO key-sync bank remains editable with its unused cells clear");
        tap(BTN_HOME); tap(BTN_LFO); view_mailbox = 0; ui_draw();
        CHECK(page == PAGE_HOME && home_view == i && lfo_selected == (first^1u) &&
              ui_field(0) == U_RATE && ui_field(3) == U_DEST,
              "short LFO returns to HOME primary controls while retaining the last selected LFO");
        memcpy(plot,fb+WT_PLOT_Y*240u,sizeof plot); ui_spectrum_peak[5] = 123;
        tap(BTN_HOME); view_mailbox = 0; ui_draw();
        CHECK(!focus && home_view == i && !memcmp(plot,fb+WT_PLOT_Y*240u,sizeof plot) &&
              ui_spectrum_peak[5] == 123,
              "HOME exits LFO controls without cycling or clearing the underlying view");
    }
}

static void ui_parity_checks(void)
{
    uint32_t i, x, y, unchanged = 1;
    uint16_t left[120u*136u], plot[240u*SP_H];
    wt_controls_t controls = {2,32768,10000000,WT_GAIN_MAX};
    wt_patch_t expected;
    char value[48];
    boot(); fm1_ms = 2500; ui_draw();
    for (y = 0; y < 136u; y++)
        for (x = 0; x < 120u; x++) left[y*120u+x] = fb[(64u+y)*240u+x];
    edit.osc[1].table = controls.table; edit.osc[1].gain = WT_GAIN_MAX;
    CHECK(wt_osc_start(&display.osc[1], &controls) == 0, "second oscillator preview initialized");
    ui_draw();
    for (y = 0; y < 136u; y++)
        for (x = 0; x < 120u; x++)
            if (left[y*120u+x] != fb[(64u+y)*240u+x]) unchanged = 0;
    CHECK(unchanged, "editing oscillator 2 cannot draw into oscillator 1's panel");
    CHECK(pixels_with(124,104,112,64,C_WHITE) > 50 &&
          pixels_with(119,64,1,136,RGB(44,44,44)) == 136,
          "the second oscillator has a live waveform within its separate half-screen panel");

    host_usb_connected = 1;
    for (i = 0; i < 4u; i++) {
        uint32_t k, bars = 1u+i%3u;
        fm1_ms = i*600u; ui_corners_draw();
        for (k = 0; k < 3u; k++)
            CHECK(pixels_with(8u+k*5u,5,3,5,k < bars ? WT_OFF : C_BLACK) == 15u,
                  "gray charging bars fill cumulatively left-to-right and wrap after three");
        CHECK(pixels_with(6,3,19,9,C_WHITE) == 0 && pixels_with(6,3,19,9,WT_ACC) == 0,
              "charging does not introduce white or lilac battery bars");
    }
    host_usb_connected = 0; batt_raw = 560; ui_corners_draw();
    CHECK(pixels_with(8,5,3,5,RGB(235,170,60)) == 15u &&
          pixels_with(13,5,3,5,C_BLACK) == 15u,
          "unplugged battery levels use FM Lab ADC thresholds and low-charge color");

    boot(); tap(BTN_FX); ui_draw();
    CHECK(page == PAGE_HOME && focus == FOCUS_FX, "short FX opens the HOME mixer");
    expected = edit;
    for (i = 0; i < 4u; i++) {
        CHECK(ui_field(i) == U_FX+i*4u, "FX MIX assigns one effect's mix to each knob");
        turn(ENC_KNOB1+i,1); expected.fx[i*4u] += 5;
    }
    CHECK(!memcmp(&edit,&expected,sizeof edit), "FX MIX edits all four wet amounts without touching detailed parameters");
    tap(BTN_FX); turn(ENC_SELECT,1);
    CHECK(!fx_selected && ui_field(1) == U_FX+4u, "repeated FX and SELECT do not change the mixer mappings");
    ui_draw();
    CHECK(text_at(44,"REVERB MIX 5",WT_ACC), "mixer value feedback names the effect in the patch line");
    input(1,1u<<BTN_FX,0); input(400,0,0);
    CHECK(page == PAGE_FX && ui_field(1) == U_FX+1u, "holding FX opens the detailed effect editor");
    edit.fx[3] = 99;
    CHECK(!strcmp(ui_value(U_FX+3u,value),"WIDTH") && !strcmp(value,"100"),
          "chorus width is displayed on a unitless 0-100 scale");
    edit.fx[0] = 0; ui_value(U_FX,value);
    CHECK(!strcmp(value,"0"), "effect mix zero has no percent suffix");
    ui_value(U_FX+1u,value);
    CHECK(strstr(value,"HZ") && !strchr(value,'%') && !strchr(value,' '), "effect rate has no space before Hz");
    ui_value(U_FX+9u,value); CHECK(strstr(value,"MS") && !strchr(value,' '), "delay time joins its ms suffix");
    ui_value(U_FX+13u,value); CHECK(strstr(value,"S") && !strchr(value,' '), "reverb decay joins its seconds suffix");
    ui_value(U_FX+15u,value); CHECK(strstr(value,"MS") && !strchr(value,' '), "reverb pre-delay joins its ms suffix");
    CHECK(!strcmp(ui_value(U_FX+6u,value),"DEPTH") && !strcmp(ui_legend(U_FX+7u),"FEEDBK") &&
          !strcmp(ui_legend(U_FX+10u),"FEEDBK") && !strcmp(ui_value(U_FX+15u,value),"PRE-DELAY"),
          "phaser, feedback and reverb labels use the FM Lab spellings");
    for (i = 0; i < 3u; i++) {
        boot(); fm1_ms = 2500; home_view = i; view_mailbox = 2; ui_draw();
        memcpy(plot,fb+64u*240u,sizeof plot);
        turn(ENC_SELECT,1); tap(BTN_FX); view_mailbox = 2; ui_draw();
        CHECK(title_at("FX MIX"), "short FX uses the FM Lab FX MIX title on every HOME view");
        CHECK(!memcmp(plot,fb+64u*240u,sizeof plot) && ui_field(0) == U_FX && ui_field(3) == U_FX+12u,
              "short FX changes only knob assignments while preserving each underlying HOME plot");
    }

    boot(); tap(BTN_HOME);
    CHECK(spectrum_span == 2u && SPANS[spectrum_span] == 5000u, "fresh spectrum defaults to 0-5 kHz");
    edit.filter_type = WT_FILTER_OFF; turn(ENC_KNOB3,-1);
    CHECK(edit.filter_type == WT_FILTER_HP, "filter TYPE wraps backwards from OFF");
    turn(ENC_KNOB3,9);
    CHECK(edit.filter_type == WT_FILTER_OFF, "filter TYPE wraps forwards with a multi-detent turn");
    turn(ENC_KNOB3,-9);
    CHECK(edit.filter_type == WT_FILTER_HP, "filter TYPE wraps with large negative turns");
    edit.cutoff = 0; ui_value(U_CUTOFF,value); CHECK(!strcmp(value,"20HZ"), "low cutoff uses compact Hz");
    edit.cutoff = WT_POSITION_MAX; ui_value(U_CUTOFF,value);
    CHECK(!strcmp(value,"16.0KHZ"), "legacy maximum retains its original 16 kHz frequency");
    turn(ENC_KNOB1,8); ui_value(U_CUTOFF,value);
    CHECK(edit.cutoff == WT_CUTOFF_MAX && !strcmp(value,"20.0KHZ"), "cutoff dial reaches 20 kHz without retuning legacy values");
    edit.resonance = WT_GAIN_MAX/4u;
    CHECK(!strcmp(ui_value(U_RESONANCE,value),"RESO") && !strcmp(value,"25"), "resonance feedback is RESO 25");
    edit.cutoff = 64u*512u;
    CHECK(abs(ui_filter_db16((uint32_t)WT_FILTER_HZ[64]*16u,edit.cutoff,0,WT_FILTER_LP)+48) <= 2 &&
          abs(ui_filter_db16((uint32_t)WT_FILTER_HZ[64]*16u,edit.cutoff,WT_GAIN_MAX,WT_FILTER_LP)-289) <= 2 &&
          abs(ui_filter_db16((uint32_t)WT_FILTER_HZ[64]*16u,edit.cutoff,WT_GAIN_MAX,WT_FILTER_BP)) <= 2,
          "filter curve uses WT's native Q range and unity-gain bandpass, not FM's resonance table");
    CHECK(ui_filter_db16(2000u*16u,edit.cutoff,0,WT_FILTER_LP) < -350 &&
          ui_filter_db16(100u*16u,edit.cutoff,0,WT_FILTER_HP) < -450 &&
          ui_filter_db16(2000u*16u,edit.cutoff,0,WT_FILTER_OFF) == 0,
          "filter visualization has the proper low/high-pass slopes and flat bypass response");
    display.voices = 1; display.note = 60; memset(captured_audio,0,sizeof captured_audio);
    edit.filter_type = WT_FILTER_LP; ui_spectrum_draw();
    CHECK(pixels_with(0,64,240,SP_H,WT_FILTER_CURVE) > 200 &&
          pixels_with(12,64,1,9,RGB(150,118,210)) == 9 &&
          pixels_with(3,78,32,16,RGB(120,100,160)) > 10,
          "spectrum draws the filter response, root/harmonic ruler and played note");
    CHECK(pixels_with(25,64,1,5,RGB(96,76,136)) == 5,
          "spectrum includes the second harmonic interval marker");
    ui_scope_draw(SP_H);
    CHECK(pixels_with(3,68,32,16,RGB(120,100,160)) > 10 &&
          pixels_with(180,68,57,16,RGB(80,80,80)) > 10,
          "scope has a played-note label on the left and actual window duration on the right");
    cv_begin(240,16,C_BLACK);
    cv_text(237-text_w(&FONT_S,"7.6MS"),0,&FONT_S,"7.6MS",RGB(80,80,80));
    unchanged = 1;
    for (y = 0; y < 16u; y++)
        for (x = 180; x < 240u; x++)
            if ((fb[(68u+y)*240u+x] == RGB(80,80,80)) !=
                (swap16(cv_px[y*240u+x]) == RGB(80,80,80))) unchanged = 0;
    CHECK(unchanged, "C4 scope duration is the actual two-period 7.6 ms window");
    {
        uint16_t scope[240u*SP_H];
        memcpy(scope,fb+64u*240u,sizeof scope);
        display.osc[0].increment.current = 1; selected = 1;
        display.osc[1].increment.current = 2000000000; ui_scope_draw(SP_H);
        CHECK(!memcmp(scope,fb+64u*240u,sizeof scope), "scope window tracks the note rather than the selected/detuned oscillator");
    }
}
static uint32_t owed;
static void pump(uint32_t ms)
{
    uint32_t i;
    for (i = 0; i < ms; i++) {
        input(1, fm1_in.buttons, fm1_in.notes);
        owed += FS;
        while (owed >= HALF_FRAMES*1000u) { owed -= HALF_FRAMES*1000u; audio(); }
    }
}

static void midi(uint32_t status, uint32_t note, uint32_t velocity)
{
    mq[mq_w%64u][0] = (uint8_t)status; mq[mq_w%64u][1] = (uint8_t)note;
    mq[mq_w%64u][2] = (uint8_t)velocity; mq_w++; input(0, fm1_in.buttons, fm1_in.notes);
}

static void named_patch(wt_patch_t *p, uint32_t index)
{
    CHECK(wt_factory(p, index%WT_FACTORY_COUNT) == 0, "native factory patch");
    memset(p->name, 0, sizeof p->name); ui_number(ui_text(p->name,"SLOT"), (int32_t)index);
    p->osc[0].semitone = (int32_t)(index%49u)-24;
}

static void midi_controller_checks(void)
{
    uint32_t i, held, sustained, generation, writes;
    wt_patch_t base;
    mi_config_t mapping;
    wt_settings_t stored, restored;
    int32_t samples[512];
    boot(); owed = 0;
    midi(0xB0,64,63); midi(0x90,60,100); audio(); midi(0x80,60,0); audio();
    CHECK(!synth.sustain && synth.voice[0].amp.stage == WT_ENV_RELEASE,
          "CC64 value 63 releases normally");
    boot(); owed = 0;
    midi(0xB0,64,64);
    for (i = 0; i < WT_VOICES; i++) midi(0x90,60u+i,100);
    audio();
    for (i = 0; i < WT_VOICES; i++) midi(0x80,60u+i,0);
    audio();
    sustained = 0; for (i = 0; i < WT_VOICES; i++) sustained += synth.voice[i].sustained;
    CHECK(sustained == WT_VOICES && synth.sustain && lit == 255u,
          "eight released voices remain sounding at the sustain threshold");
    midi(0x90,60,110); audio();
    held = sustained = 0;
    for (i = 0; i < WT_VOICES; i++) { held += synth.voice[i].held; sustained += synth.voice[i].sustained; }
    CHECK(held == 1u && sustained == 7u && synth.voice[0].note == 60u,
          "repeated sustained pitch retriggers one voice without accumulating stuck ownership");
    midi(0x90,90,100); audio();
    CHECK(synth.voice[0].held && synth.voice[0].note == 60u,
          "voice stealing prefers a pedal-held note over a physically held note");
    mapping = mi_config; midi(0xC0,1,0); audio();
    CHECK(!memcmp(&mapping,&mi_config,sizeof mapping) && synth.sustain,
          "program change retains global mappings and pedal ownership");
    midi(0xB0,64,0); audio();
    for (i = 0; i < WT_VOICES; i++) CHECK(!synth.voice[i].sustained,"pedal-up clears every deferred release");
    midi(0x80,60,0); midi(0x90,90,0); pump(600);
    CHECK(!lit,"stolen/repeated notes and program change eventually release all voices");

    boot(); owed = 0; ev_push(EV_MONO,1,0); audio();
    midi(0xB0,64,127); midi(0x90,60,100); midi(0x90,64,100); audio();
    midi(0x80,64,0); audio();
    CHECK(synth.voice[0].note == 60u && synth.voice[0].held,"mono sustain keeps physical last-note fallback");
    midi(0x80,60,0); audio();
    CHECK(synth.voice[0].sustained && !synth.held,"mono last key is deferred by the pedal");
    midi(0xB0,121,0); pump(600);
    CHECK(!lit && !synth.sustain,"controller reset releases mono pedal-held notes");

    boot(); owed = 0; base = edit; generation = patch_generation;
    midi(0xE0,0,0); pump(30); CHECK(mi_pitch() == -512,"14-bit bend low endpoint is minus two semitones");
    midi(0xE0,127,127); pump(30); CHECK(mi_pitch() == 512,"14-bit bend high endpoint is plus two semitones");
    midi(0xE0,0,64); pump(30); CHECK(!mi_pitch(),"pitch-wheel center restores exact tuning");
    mi_range_turn(10); midi(0xE0,127,127); pump(30);
    CHECK(mi_pitch() == 12*256,"adjustable bend range affects the audio-owned pitch");
    midi(0xB0,1,127); pump(30);
    CHECK(synth.controller_vibrato > 0,"wheel adds smoothly controlled patch-LFO vibrato");
    midi(0xB0,1,0); pump(30);
    CHECK(!synth.controller_vibrato && !memcmp(&edit,&base,sizeof base) && patch_generation == generation,
          "wheel zero adds nothing and live controllers never publish or edit the saved patch");
    mi_config.route[9].negative = 1; mi_publish();
    midi(0xD0,127,255); pump(30);
    CHECK(synth.patch.osc[0].position == 0u && synth.controller_vibrato > 0,
          "channel pressure is one-byte data, with simultaneous vibrato and negative brightness routing");
    midi(0xD0,0,255); pump(30);
    CHECK(!synth.controller_vibrato && !memcmp(&synth.patch,&edit,sizeof edit),
          "pressure zero restores base controls without changing the editor");

    boot(); owed = 0; base = edit; audio();
    midi(0xB0,74,127); audio();
    CHECK(synth.patch.cutoff > base.cutoff && synth.patch.cutoff < WT_CUTOFF_MAX,
          "first absolute CC activation crossfades from the patch rather than jumping to zero");
    pump(30); midi(0xB0,71,127); pump(30);
    CHECK(synth.patch.cutoff == WT_CUTOFF_MAX && synth.patch.resonance == WT_GAIN_MAX,
          "standard filter controllers reach their exposed ranges");
    ui_edit(U_CUTOFF,-1); pump(30);
    CHECK(synth.patch.cutoff == edit.cutoff,"a physical parameter edit takes ownership back from MIDI");
    midi(0xB0,74,127); pump(30);
    CHECK(synth.patch.cutoff == WT_CUTOFF_MAX,"the next controller movement reactivates its assignment");
    base = edit;
    midi(0xB0,91,100); midi(0xB0,93,80); pump(30);
    mapping = mi_config;
    midi(0xB0,7,63); midi(0xB0,10,0); midi(0xB0,11,0); midi(0xD0,100,0); pump(30);
    midi(0xB0,121,0); pump(30);
    CHECK(mi_value(MI_VOLUME,32768,0,32768) < 17000 &&
          mi_value(MI_EXPRESSION,32768,0,32768) == 32768 &&
          mi_value(MI_PAN,32768,0,65536) == 0 && !synth.controller_vibrato &&
          synth.patch.cutoff == WT_CUTOFF_MAX && !memcmp(&mapping,&mi_config,sizeof mapping),
          "CC121 resets expression/wheel/pressure/bend, preserving volume/pan/timbre/effects and mappings");
    midi(0xB0,11,0); pump(30);
    CHECK(mi_value(MI_EXPRESSION,32768,0,32768) == 0 && !memcmp(&base,&edit,sizeof base),
          "expression can silence output without altering the patch");

    boot(); owed = 0; preferences.output_mono = 1; patch_publish();
    midi(0xB0,10,127); midi(0x90,60,100); pump(30);
    memset(samples,0,sizeof samples); app_audio(samples,256);
    { uint32_t signal = 0;
      for (i = 0; i < 256u; i++) {
          CHECK(samples[2u*i] == samples[2u*i+1u],"OUTPUT MONO folds controller pan onto both channels");
          signal |= (uint32_t)samples[2u*i];
      }
      CHECK(signal,"fully right-panned notes remain audible through a mono plug"); }
      boot(); owed = 0; audio();
      mi_config.route[10] = (mi_route_t){MI_PRESSURE,MI_AMPLITUDE,25,1};
      mi_publish(); midi(0xD0,127,0); pump(30);
      CHECK(mi_left.current == 16384 && mi_right.current == 16384,
            "pressure amplitude depth reaches both output gains");
      midi(0xD0,0,0); pump(30);
      CHECK(mi_left.current == 32768 && mi_right.current == 32768,"zero pressure adds no amplitude modulation");
      mi_config.route[10] = (mi_route_t){MI_PRESSURE,MI_CUTOFF,50,1};
      mi_publish(); midi(0xD0,127,0); pump(30);
      CHECK(synth.patch.cutoff == edit.cutoff-WT_CUTOFF_MAX/2u,
            "pressure drives temporary cutoff with adjustable depth and polarity");
    boot(); owed = 0; midi(0xB0,64,127); midi(0x90,60,100); audio();
    midi(0xB0,123,0); audio();
    CHECK(synth.voice[0].sustained && lit && !direct_count[60],"CC123 releases ownership but honors sustain");
    midi(0xB0,64,0); audio();
    CHECK(synth.voice[0].amp.stage == WT_ENV_RELEASE,"All Notes Off enters normal release at pedal-up");
    edit.fx[0] = edit.fx[8] = edit.fx[12] = 70; patch_publish();
    midi(0x90,64,100); pump(400); midi(0xB0,64,127); midi(0xB0,120,0);
    memset(samples,0,sizeof samples); app_audio(samples,256);
    for (i = 0; i < 512u; i++) CHECK(!samples[i],"CC120 immediately clears voices and effect history");
    CHECK(!lit && synth.sustain,"All Sound Off clears notes but preserves controller state");
    midi(0x90,60,100); midi(0x80,60,0); audio();
    CHECK(synth.voice[0].sustained,"new notes still obey the pedal after All Sound Off");
    midi(0xB0,121,0); pump(600); CHECK(!lit,"controller reset subsequently releases the preserved pedal");

    boot(); owed = 0; midi_channel = 3;
    midi(0xB0,64,127); midi(0x90,60,100); audio();
    CHECK(!lit && !synth.sustain,"channel filtering covers controllers as well as notes");
    midi(0xB2,64,127); midi(0x92,60,100); audio();
    CHECK(lit && synth.sustain,"selected-channel notes and sustain are accepted");
    ui_edit(U_MIDI,-2); audio();
    CHECK(!lit && !synth.sustain,"receive-channel changes clear old notes and pedal ownership");

    boot(); owed = 0; base = edit; generation = patch_generation;
    input(1,1u<<BTN_GLO,0); input(399,0,0);
    CHECK(!mi_editor && page == PAGE_GLO,"short GLO retains its existing menu");
    tap(BTN_HOME); input(1,1u<<BTN_GLO,0); input(400,0,0);
    CHECK(mi_editor,"400 ms GLO opens the matching controller editor");
    turn(ENC_SELECT,10); tap(BTN_SEL);
    CHECK(!mi_learn && mi_feedback == 4u,"Learn requires a selected destination");
    turn(ENC_PRESETS,1); tap(BTN_SEL); midi(0xB0,64,127);
    CHECK(mi_learn && mi_feedback == 2u && mi_config.route[10].source == MI_OFF,
          "reserved sustain remains functional and cannot consume a Learn assignment");
    midi(0xB0,21,100); audio();
    CHECK(!mi_learn && mi_config.route[10].source == 21u && mi_config.route[10].target == MI_CUTOFF &&
          !memcmp(&edit,&base,sizeof base) && patch_generation == generation,
          "on-device Learn stores a route without editing or publishing the patch");
    turn(ENC_KNOB2,-35); turn(ENC_KNOB3,-1); turn(ENC_KNOB4,5);
    mapping = mi_config; midi(0xB0,64,0); midi(0xB0,120,0); pump(2500);
    CHECK(!settings_dirty && !mi_dirty && wt_settings_read(&stored) == 0 &&
          !memcmp(&stored.midi_config,&mapping,sizeof mapping),"global route/depth/polarity/range save after quiet");
    writes = host_flash_erases; midi(0xB0,21,20); pump(2500);
    CHECK(host_flash_erases == writes && !settings_dirty,"temporary live CC movement never autosaves settings");
    ui_init();
    CHECK(!memcmp(&mi_config,&mapping,sizeof mapping) && !mi_input.bend && mi_input.value[21] == 0,
          "reboot restores mappings, not live controller values");
    restored = stored; stored.midi_config.route[0].source = 64;
    CHECK(wt_settings_write(&stored) == ST_INVALID,"reserved learned sources are rejected by the settings codec");
    CHECK(wt_settings_read(&stored) == 0 && !memcmp(&stored,&restored,sizeof stored),
          "invalid settings do not overwrite the last valid mapping");
    mi_open_editor(); mi_slot = 10; mi_clear();
    CHECK(mi_config.route[10].source == MI_OFF && mi_dirty,"SAVE clears only the selected global assignment");
    tap(BTN_HOME); CHECK(!mi_editor && !mi_learn,"HOME cancels Learn and returns to the normal instrument");
    mi_toggle_learn(); midi(0xD0,100,0);
    CHECK(mi_config.route[10].source == MI_PRESSURE && !mi_learn,"channel pressure is a separate learnable source");
    {
        uint8_t seen[MI_TARGET_COUNT] = {0};
        uint8_t saved_target = mi_config.route[10].target;
        uint32_t g, n;
        for (g = 0; g < MI_GROUP_COUNT; g++) {
            CHECK(mi_groups[g][0] >= 3u,"destination groups contain at least three meaningful choices");
            for (n = 1; n <= mi_groups[g][0]; n++) {
                uint32_t target = mi_groups[g][n];
                CHECK(target < MI_TARGET_COUNT && !seen[target],"destination hierarchy preserves unique stored IDs");
                if (target < MI_TARGET_COUNT) {
                    seen[target] = 1; mi_config.route[10].target = (uint8_t)target;
                    mi_drawn = ~0u; mi_draw();
                }
            }
        }
        for (n = 0; n < MI_TARGET_COUNT; n++) CHECK(seen[n],"every stored destination remains reachable");
        mi_config.route[10].target = saved_target; mi_drawn = ~0u;
    }
    mi_open_editor(); mi_slot = 10;
    mi_toggle_learn(); mi_feedback = 2; mapping = mi_config;
    tap(BTN_GLO);
    CHECK(mi_editor && mi_slot == 11u && !mi_learn && !mi_feedback &&
          !memcmp(&mi_config,&mapping,sizeof mapping),"GLO advances assignment without editing settings or retaining Learn");
    mi_slot = 15; input(1,1u<<BTN_GLO,0); input(600,0,0);
    CHECK(mi_editor && mi_slot == 0u,"a held GLO advances once and wraps without reopening GLOBAL");
    input(1,0,0); turn(ENC_SELECT,10);
    CHECK(mi_config.route[10].target == MI_CUTOFF,"slot return derives the destination group from stored target");
    turn(ENC_ALGORITHM,-1);
    CHECK(mi_config.route[10].target == 34u && mi_config.bend_range == mapping.bend_range,
          "ALGORITHM wraps within TONE without editing bend or loading a patch");
    turn(ENC_PRESETS,1);
    CHECK(mi_config.route[10].target == 13u,"PRESETS selects the next group's first target");
    CHECK(!memcmp(&edit,&base,sizeof base) && patch_generation == generation,
          "grouped navigation leaves the native patch unchanged");
    mi_knob(3,100); CHECK(mi_config.bend_range == 24u,"BEND knob clamps to the global maximum");
    mi_knob(3,-100); CHECK(!mi_config.bend_range,"BEND knob clamps to the global minimum");
}

static void patch_audio(const wt_patch_t *p, int32_t *out, uint32_t count)
{
    wt_synth_t s;
    fx_t x;
    int32_t l[32], r[32];
    uint32_t b, i;
    wt_synth_init(&s); CHECK(wt_synth_patch(&s,p) == 0, "loaded voice applies");
    CHECK(wt_synth_note_on(&s,69,100) == 0, "round-trip audio note");
    wts_clear(&xs,sizeof xs); xs.ph_a = PH_A[PH_LO>>8]; fx_config(&x,p->fx);
    for (b = 0; b < count; b += 32u) {
        CHECK(wt_synth_render(&s,l,32) == 0, "round-trip voice rendering");
        fx_run(&x,l,r,32);
        for (i = 0; i < 32u; i++) { out[2u*(b+i)] = l[i]; out[2u*(b+i)+1u] = r[i]; }
    }
}

static void storage_checks(void)
{
    wt_patch_t p, decoded, unchanged;
    wt_settings_t s, loaded, old;
    uint8_t bytes[WT_PATCH_BYTES], config[WT_SETTINGS_BYTES], prior_bank[WT_BANK_BYTES-16u];
    static int32_t reference[2u*16384u], restored[2u*16384u];
    uint32_t i, writes;
    boot(); owed = 0;
    named_patch(&p,6); p.osc[0].fine = -256; p.osc[1].position_env = -65536;
    p.filter_env = 65536; p.lfo[1].rate = 17770; p.lfo[1].shape = WT_LFO_RANDOM;
    p.lfo[1].depth = 21000; p.lfo[1].destination = WT_MOD_POS2;
    p.fx[0] = 45; p.fx[4] = 30; p.fx[8] = 35; p.fx[9] = 20; p.fx[12] = 30;
    CHECK(wt_patch_pack(bytes,&p) == 0 && wt_patch_unpack(&decoded,bytes) == 0 &&
          !memcmp(&p,&decoded,sizeof p), "all native patch fields, effects and name round-trip");
    p.cutoff = WT_CUTOFF_MAX;
    CHECK(wt_patch_pack(bytes,&p) == 0 && wt_patch_unpack(&decoded,bytes) == 0 &&
          decoded.cutoff == WT_CUTOFF_MAX && wt_filter_hz(decoded.cutoff) == 20000u &&
          wt_filter_hz(WT_CUTOFF_MAX+1u) == 20000u,
          "extended cutoff survives the existing patch codec and its Hz conversion clamps safely");
    for (i = 8u; i < WT_TABLE_COUNT; i++) {
        p.osc[0].table = i; p.osc[1].table = WT_TABLE_COUNT-1u;
        CHECK(wt_patch_pack(bytes,&p) == 0 && wt_patch_unpack(&decoded,bytes) == 0 &&
              !memcmp(&p,&decoded,sizeof p), "appended table IDs round-trip in unchanged native patch format");
    }
    unchanged = decoded; wt_put32(bytes,WT_TABLE_COUNT);
    CHECK(wt_patch_unpack(&decoded,bytes) == ST_INVALID && !memcmp(&decoded,&unchanged,sizeof decoded),
          "invalid native patch is rejected without changing destination");
    wt_patch_pack(bytes,&p); memset(bytes+160,'X',WT_NAME_LEN);
    CHECK(wt_patch_unpack(&decoded,bytes) == ST_INVALID, "non-terminated native patch names are rejected");
    CHECK(wt_voice_store(255,&p) == 0, "last user slot persists a complete native patch");
    patch_audio(&p,reference,16384);
    CHECK(wt_banks_load() == 0 && wt_voice_get(&decoded,WT_FACTORY_COUNT+255u) == 0, "readback after bank RAM reload");
    patch_audio(&decoded,restored,16384);
    CHECK(!memcmp(reference,restored,sizeof reference), "saved/reloaded sound including all four effects is bit-identical");
    writes = host_flash_erases;
    CHECK(wt_voice_store(256,&p) == ST_INVALID && host_flash_erases == writes, "out-of-range slot never erases flash");

    for (i = 0; i < WT_USER_COUNT; i++) {
        named_patch(&p,i); CHECK(wt_voice_store(i,&p) == 0, "write every user slot within the existing bank allocation");
    }
    CHECK(wt_banks_load() == 0, "all eight complete native banks survive reload");
    for (i = 0; i < WT_USER_COUNT; i++) {
        named_patch(&p,i);
        CHECK(wt_voice_get(&decoded,WT_FACTORY_COUNT+i) == 0 && !memcmp(&p,&decoded,sizeof p),
              "all 256 slots preserve their distinct full patches");
    }
    memcpy(prior_bank,user_patches,sizeof prior_bank); named_patch(&p,999);
    host_flash_drop_writes = 1;
    CHECK(wt_voice_store(0,&p) == ST_INVALID && !memcmp(prior_bank,user_patches,sizeof prior_bank),
          "readback must match the intended bank, not merely an old CRC-valid record");
    host_flash_drop_writes = 0; host_flash_fail_after = 0;
    CHECK(wt_voice_store(0,&p) != 0 && !memcmp(prior_bank,user_patches,sizeof prior_bank),
          "failed programming does not commit the staged bank into RAM");
    host_flash_fail_after = 1;
    CHECK(wt_voice_store(0,&p) != 0 && !memcmp(prior_bank,user_patches,sizeof prior_bank),
          "interrupted two-sector erase cannot claim a saved patch");
    host_flash_fail_after = -1;
    wt_banks_load(); named_patch(&p,255);
    CHECK(wt_voice_get(&decoded,WT_FACTORY_COUNT+255u) == 0 && !memcmp(&p,&decoded,sizeof p),
          "a cut write leaves other banks untouched");
    CHECK(wt_voice_get(&decoded,WT_FACTORY_COUNT) == 0 && !strcmp(decoded.name,"INIT WAVE"),
          "a bank without its final header never loads partially written voices");

    host_flash[BANK_ADDR(7)-BANK_LO_AT+32u] ^= 1u;
    CHECK(wt_banks_load() & (1u<<7), "bank checksum corruption is rejected explicitly");
    named_patch(&p,10); CHECK(wt_voice_store(0,&p) == 0, "restore a valid test bank");
    CHECK(bank_read(0,bank_stage,sizeof bank_stage) == 0, "native bank header");
    wt_put32(bank_stage,999);
    CHECK(bank_write(0,bank_stage,sizeof bank_stage) == 0 && (wt_banks_load()&1u),
          "a CRC-valid bank with an unknown schema is rejected");
    CHECK(wt_voice_store(0,&p) == 0 && bank_read(0,bank_stage,sizeof bank_stage) == 0, "valid bank before bad field");
    bank_stage[16u+144u] = 100;
    CHECK(bank_write(0,bank_stage,sizeof bank_stage) == 0 && (wt_banks_load()&1u),
          "a CRC-valid bank with an invalid effect field is rejected, not clamped");

    workflow_capture(&s); s.preferences.transpose = -12; s.preferences.fine = 127;
    s.preferences.bits = 3; s.preferences.output_mono = 1; s.preferences.pc_send = 0;
    s.octave = -2; s.midi = 16; s.mono = 1; s.span = 4;
    s.voice = WT_VOICE_COUNT-1u; s.slot = 255; s.count = SQ_MAX;
    for (i = 0; i < SQ_MAX; i++) {
        s.notes[i].step = (uint8_t)i; s.notes[i].note = (uint8_t)(36u+i); s.notes[i].len = (uint8_t)(i+1u);
    }
    CHECK(wt_settings_write(&s) == 0 && wt_settings_read(&loaded) == 0 && !memcmp(&s,&loaded,sizeof s),
          "device preferences, selected slot, performance controls and 64-note phrase round-trip");
    old = loaded; host_flash[SETTINGS_AT-BANK_LO_AT+12u+30u] ^= 1;
    CHECK(wt_settings_read(&loaded) == ST_INVALID && !memcmp(&loaded,&old,sizeof old),
          "settings CRC failure leaves the destination unchanged");
    CHECK(wt_settings_pack(config,&s) == 0, "settings codec");
    CHECK(wt_get32(config) == 1u && !wt_get32(config+236),
          "native-rate settings retain the original wire format for older firmware");
    config[240] = 1;
    CHECK(wt_settings_unpack(&loaded,config) == ST_INVALID,"legacy records reject nonzero reserved controller tails");
    for (i = 1; i < WT_SAMPLE_RATE_COUNT; i++) {
        wt_settings_t reduced = s;
        reduced.preferences.sample_rate = i;
        CHECK(wt_settings_write(&reduced) == 0 && wt_settings_read(&loaded) == 0 &&
              !memcmp(&reduced,&loaded,sizeof reduced),
              "all reduced rates round-trip with every existing preference and all 64 phrase notes");
    }
    CHECK(wt_get32(host_flash+SETTINGS_AT-BANK_LO_AT+12u) == 2u,
          "reduced-rate settings use an explicitly versioned extension");
    CHECK(wt_settings_pack(config,&loaded) == 0, "extended settings codec");
    wt_put32(config+236,WT_SAMPLE_RATE_COUNT);
    old = loaded;
    CHECK(settings_save(config,sizeof config) == 0 && wt_settings_read(&loaded) == ST_INVALID &&
          !memcmp(&loaded,&old,sizeof old), "invalid encoded sample rates are rejected without partial restore");
    wt_settings_pack(config,&s);
    config[43] = SQ_MAX+1u;
    CHECK(settings_save(config,sizeof config) == 0 && wt_settings_read(&loaded) == ST_INVALID,
          "CRC-valid malformed phrase length is rejected");
    CHECK(wt_settings_write(&s) == 0, "restore native preferences");
    wt_settings_pack(config,&s); config[236] = 1;
    CHECK(settings_save(config,sizeof config) == 0 && wt_settings_read(&loaded) == ST_INVALID,
          "unknown reserved settings fields are rejected");
    wt_settings_pack(config,&s); wt_put32(config,3);
    CHECK(settings_save(config,sizeof config) == 0 && wt_settings_read(&loaded) == ST_INVALID,
          "unknown native settings version is rejected");
    CHECK(wt_settings_write(&s) == 0, "restore settings before checking old magic");
    wt_put32(host_flash+SETTINGS_AT-BANK_LO_AT,0x31535457u);
    CHECK(wt_settings_read(&loaded) == ST_INVALID, "baseline WTS1 settings cannot be interpreted as native data");
    CHECK(wt_voice_store(0,&p) == 0, "restore native bank");
    wt_put32(host_flash+BANK_ADDR(0)-BANK_LO_AT,0x31425457u);
    CHECK(wt_banks_load()&1u, "baseline WTB1 banks cannot be interpreted as native patches");
    host_flash_read_fail = 1;
    CHECK(wt_settings_read(&loaded) == ST_IO, "storage I/O errors remain distinguishable from blank flash");
    host_flash_read_fail = 0;

    boot(); owed = 0; tap(BTN_EDIT);
    edit.fx[0] = 30; oscillator_mutes = 1; preferences.transpose = 12; preferences.fine = 17;
    preferences.bits = 1; preferences.output_mono = 1; preferences.pc_send = 0; midi_channel = 2; octave = -1;
    preferences.sample_rate = 4;
    patch_publish(); input(1,0,1u<<7); pump(50);
    tap(BTN_SAVE); turn(ENC_PRESETS,255); turn(ENC_ALGORITHM,1); tap(BTN_SAVE); pump(400);
    CHECK(save_pending && !host_flash_erases, "SAVE never erases under a held key, even when it is muted");
    input(1,0,0); pump(700);
    CHECK(!save_pending && preset_index == WT_VOICE_COUNT-1u && !workflow_modified() &&
          host_flash_erases == 3u && !strcmp(message,"PATCH SAVED"), "quiet acknowledged save commits patch and preferences");
    p = edit;
    fm1_in.notes = fm1_in.buttons = 0; ui_init();
    CHECK(!memcmp(&edit,&p,sizeof p) && !oscillator_mutes && preferences.transpose == 12 &&
          preferences.fine == 17 && preferences.bits == 1u && midi_channel == 2u && octave == -1 &&
          preferences.sample_rate == 4u &&
          preset_index == WT_VOICE_COUNT-1u, "power-cycle startup restores the saved native patch and separate preferences");
    CHECK(!arp_on && !seq_run && !rec && !audio_fault, "startup never resumes a recording or latched performance");
    edit.fx[12] = 35; patch_publish(); ui_ab_swap();
    CHECK(edit.fx[12] != 35u && preferences.bits == 1u && preferences.sample_rate == 4u,
          "A/B includes effects but not bit-depth/sample-rate output preferences");
    ui_ab_swap(); CHECK(edit.fx[12] == 35u, "A/B restores the edited effect settings");
    host_flash_fail_after = 0;
    tap(BTN_SAVE); tap(BTN_SAVE); pump(400);
    CHECK(!save_pending && !strcmp(message,"PATCH SAVE FAILED") && workflow_modified(),
          "write failure stays visibly unsaved and does not replace the bank's original voice");
    host_flash_fail_after = -1;
    preset_index = 0; pc_bank = 0; workflow_program(8);
    CHECK(!preset_index && !strcmp(message,"INVALID BANK / PROGRAM"), "factory bank rejects programs beyond its eight factory patches");
    pc_bank = 8; workflow_program(31);
    CHECK(preset_index == WT_VOICE_COUNT-1u, "incoming bank/program reaches the final user slot");

    boot(); owed = 0; pump(150); tap(BTN_SAVE); tap(BTN_SAVE); audio();
    CHECK(flash_hold == 2u, "audio acknowledges a truly quiet save");
    input(1,0,1u<<7);
    CHECK(save_pending && !flash_hold && !host_flash_erases, "new input revokes a quiet acknowledgement before flash is erased");
    tap(BTN_HOME); input(1,0,0); pump(500);
    CHECK(!save_pending && !host_flash_erases, "HOME cancels a deferred save without writing anything");

    boot(); owed = 0; input(1,0,1u<<7); pump(30); tap(BTN_SAVE); tap(BTN_SAVE); p = pending_patch;
    turn(ENC_ALGORITHM,1); input(1,0,0); pump(700);
    CHECK(!save_pending && preset_index == 0u && edit.osc[0].table == 1u &&
          wt_voice_get(&decoded,WT_FACTORY_COUNT) == 0 && !memcmp(&decoded,&p,sizeof p),
          "a delayed save writes its snapshot without replacing later edits or changing the selected preset");

    boot(); owed = 0; edit.fx[12] = 60; patch_publish(); ev_push(EV_ON,69,100); pump(100);
    ev_push(EV_OFF,69,0); pump(300); tap(BTN_SAVE); tap(BTN_SAVE); pump(100);
    CHECK(!lit && save_pending && !host_flash_erases, "an audible reverb tail defers flash even after every voice is inactive");
    pump(12000);
    CHECK(!save_pending && host_flash_erases == 3u, "deferred save finishes once the effect tail is genuinely quiet");

    for (i = 2; i < 4u; i++) {
        boot(); named_patch(&p,12); host_flash_fail_after = (int32_t)i;
        CHECK(wt_voice_store(7,&p) != 0, "payload/header programming failures are reported");
        host_flash_fail_after = -1; wt_banks_load();
        CHECK(wt_voice_get(&decoded,WT_FACTORY_COUNT+7u) == 0 && !strcmp(decoded.name,"INIT WAVE"),
              "failed payload or final-header programming never loads a partial saved voice");
    }
}

static void effect_checks(void)
{
    wt_patch_t p;
    fx_t x;
    int32_t l[32], r[32];
    uint32_t effect, b, i, wet, stereo, stale;
    int32_t stereo_block[2u*HALF_FRAMES], mono_block[2u*HALF_FRAMES];
    for (effect = 0; effect < FX_N; effect++) {
        wt_patch_init(&p); p.fx[effect*4u] = 80;
        fx_config(&x,p.fx); wts_clear(&xs,sizeof xs); xs.ph_a = PH_A[PH_LO>>8];
        wet = stereo = 0;
        for (b = 0; b < 22080u; b += 32u) {
            memset(l,0,sizeof l); if (!b) l[0] = 1000000;
            fx_run(&x,l,r,32);
            for (i = 0; i < 32u; i++) {
                if (b && (l[i] || r[i])) wet = 1;
                if (l[i] != r[i]) stereo = 1;
            }
        }
        CHECK(wet, "each restored effect produces its own audible impulse response");
        if (effect == FX_CHO || effect == FX_REV) CHECK(stereo, "chorus and reverb actually produce stereo");
        p.fx[effect*4u] = 0; fx_config(&x,p.fx); memset(l,0,sizeof l); fx_run(&x,l,r,32);
        p.fx[effect*4u] = 80; fx_config(&x,p.fx); stale = 0;
        for (b = 0; b < 22080u; b += 32u) {
            memset(l,0,sizeof l); fx_run(&x,l,r,32);
            for (i = 0; i < 32u; i++) if (l[i] || r[i]) stale = 1;
        }
        CHECK(!stale, "re-enabling an effect never reads stale delay/reverb history");
    }
    wt_patch_init(&p); p.fx[8] = 99; p.fx[9] = 0; fx_config(&x,p.fx);
    for (i = 0; i < DLY_LEN; i++) dly_buf[i] = 12345;
    wts_clear(&xs,sizeof xs); xs.ph_a = PH_A[PH_LO>>8]; memset(l,0,sizeof l); fx_run(&x,l,r,32);
    stale = 0; for (i = 0; i < 32u; i++) if (l[i] || r[i]) stale = 1;
    CHECK(!stale, "cold minimum-time delay cannot read unwritten pool memory");
    wt_patch_init(&p); memset(p.fx,99,sizeof p.fx); fx_config(&x,p.fx);
    wts_clear(&xs,sizeof xs); xs.ph_a = PH_A[PH_LO>>8];
    for (b = 0; b < 131072u; b += 32u) {
        for (i = 0; i < 32u; i++) l[i] = (b+i)&1u ? 8388607 : -8388607;
        fx_run(&x,l,r,32);
        for (i = 0; i < 32u; i++)
            CHECK(l[i] >= -8388607 && l[i] <= 8388607 && r[i] >= -8388607 && r[i] <= 8388607,
                  "maximum effect settings keep full-scale input bounded");
    }
    boot(); edit.fx[0] = 80; patch_publish(); ev_push(EV_ON,69,100);
    memset(stereo_block,0,sizeof stereo_block); app_audio(stereo_block,HALF_FRAMES);
    boot(); edit.fx[0] = 80; preferences.output_mono = 1; patch_publish(); ev_push(EV_ON,69,100);
    memset(mono_block,0,sizeof mono_block); app_audio(mono_block,HALF_FRAMES);
    stereo = 0;
    for (i = 0; i < HALF_FRAMES; i++) {
        if (stereo_block[2u*i] != stereo_block[2u*i+1u]) stereo = 1;
        CHECK(mono_block[2u*i] == mono_block[2u*i+1u] &&
              mono_block[2u*i] == (stereo_block[2u*i]+stereo_block[2u*i+1u])/2,
              "native mono output is the real post-effects stereo mean");
    }
    CHECK(stereo, "the native audio bridge retains the effects' stereo output");
}

static void performance_home_checks(void)
{
    static const uint32_t arp_fields[2][4] = {{UI_TEMPO,UI_APAT,UI_AOCT,UI_LATCH},
                                            {UI_AGATE,UI_ARATE,UI_SWING,UI_AVEL}};
    static const char *const arp_labels[2][4] = {{"TEMPO","PATTERN","OCTAVE","LATCH"},
                                                {"GATE","RATE","SWING","VELO"}};
    static const uint32_t seq_fields[] = {UI_TEMPO,UI_BARS,UI_DIV,UI_CLICK};
    static const char *const seq_labels[] = {"TEMPO","BARS","DIV","CLICK"};
    uint32_t i, j, k, generation;
    uint8_t expected[PL_N];
    uint16_t plot[240u*SP_H];
    wt_patch_t patch;
    for (i = 0; i < 3u; i++) {
        boot(); fm1_ms = 2500; home_view = i; bank = 1; patch = edit;
        memcpy(expected,play_controls,sizeof expected);
        view_mailbox = 2; ui_draw(); memcpy(plot,fb+WT_PLOT_Y*240u,sizeof plot);
        ui_spectrum_peak[5] = 123;
        tap(BTN_ARP); view_mailbox = 0; ui_draw();
        CHECK(page == PAGE_HOME && home_view == i && focus == FOCUS_ARP && !bank && !arp_on &&
              title_at("ARP 1/2") && !memcmp(plot,fb+WT_PLOT_Y*240u,sizeof plot) &&
              ui_spectrum_peak[5] == 123 && !host_led_keys[BTN_ARP],
              "first ARP press borrows controls with the FM title, preserving HOME without enabling the arp");
        for (k = 0; k < 2u; k++) {
            for (j = 0; j < 4u; j++) {
                CHECK(ui_field(j) == U_PLAY+arp_fields[k][j] &&
                      !strcmp(ui_legend(ui_field(j)),arp_labels[k][j]),
                      "ARP's two four-knob banks match FM Lab's mappings and legends");
                turn(ENC_KNOB1+j,1); expected[arp_fields[k][j]]++;
            }
            CHECK(!memcmp(play_controls,expected,sizeof expected) && !memcmp(&edit,&patch,sizeof patch),
                  "ARP knobs edit only the four selected performance controls, not the sound patch");
            if (!k) {
                turn(ENC_SELECT,1); view_mailbox = 0; ui_draw();
                CHECK(bank == 1u && title_at("ARP 2/2") &&
                      !memcmp(plot,fb+WT_PLOT_Y*240u,sizeof plot) && ui_spectrum_peak[5] == 123,
                      "SELECT changes ARP bank/title without clearing the plot or peaks");
            }
        }
        turn(ENC_SELECT,1); CHECK(!bank, "ARP banks wrap forward");
        turn(ENC_SELECT,-1); CHECK(bank == 1u, "ARP banks wrap backward");
        tap(BTN_HOME); tap(BTN_LFO); tap(BTN_ARP); view_mailbox = 0; ui_draw();
        CHECK(bank == 1u && title_at("ARP 2/2") && !arp_on &&
              !memcmp(plot,fb+WT_PLOT_Y*240u,sizeof plot) && ui_spectrum_peak[5] == 123,
              "returning to ARP remembers its FM-style bank independently of other shortcuts");
        tap(BTN_ARP); view_mailbox = 0; ui_draw();
        CHECK(arp_on && host_led_keys[BTN_ARP] && title_at("ARP 2/2") &&
              !strcmp(message,"ARP ON, LATCHED") && !memcmp(plot,fb+WT_PLOT_Y*240u,sizeof plot),
              "second ARP press toggles on with latch feedback while retaining title, bank and plot");
        tap(BTN_ARP); CHECK(!arp_on && !host_led_keys[BTN_ARP] && bank == 1u,
                             "another ARP press toggles off rather than switching banks");
        input(1,1u<<BTN_SEQ,0); input(399,0,0); view_mailbox = 0; ui_draw();
        CHECK(page == PAGE_HOME && focus == FOCUS_SEQ && home_view == i && title_at("SEQ") &&
              host_led_keys[BTN_SEQ] && !seq_run && !rec &&
              !memcmp(plot,fb+WT_PLOT_Y*240u,sizeof plot) && ui_spectrum_peak[5] == 123,
              "399 ms SEQ exposes the FM controls/title/light while preserving HOME and stopped transport");
        for (j = 0; j < 4u; j++) {
            CHECK(ui_field(j) == U_PLAY+seq_fields[j] &&
                  !strcmp(ui_legend(ui_field(j)),seq_labels[j]),
                  "SEQ exposes the FM tempo/bars/div/click mappings and legends");
            turn(ENC_KNOB1+j,1); expected[seq_fields[j]]++;
        }
        CHECK(!memcmp(play_controls,expected,sizeof expected) && !memcmp(&edit,&patch,sizeof patch),
              "SEQ knobs edit only their shared-clock/looper controls");
        turn(ENC_SELECT,1); tap(BTN_SEQ); view_mailbox = 0; ui_draw();
        CHECK(!bank && !memcmp(play_controls,expected,sizeof expected) && !seq_run && !rec &&
              title_at("SEQ") && !memcmp(plot,fb+WT_PLOT_Y*240u,sizeof plot),
              "SEQ has one fixed bank: SELECT/repeated taps do not change controls, transport or HOME");
        tap(BTN_HOME); view_mailbox = 0; ui_draw();
        CHECK(!focus && home_view == i && !host_led_keys[BTN_SEQ] &&
              !memcmp(plot,fb+WT_PLOT_Y*240u,sizeof plot) && ui_spectrum_peak[5] == 123,
              "HOME exits performance shortcuts without cycling or clearing the view");
        sq[0].step = 0; sq[0].note = 60; sq[0].len = 2; sq_n = 1;
        input(1,1u<<BTN_SEQ,0); input(400,0,0); ui_draw();
        CHECK(page == PAGE_STEP && host_led_keys[BTN_SEQ] && ui_field(0) == U_LENGTH &&
              ui_field(3) == U_PLAY+UI_DIV && pixels_with(6,60,12,14,WT_ACC) == 168u,
              "400 ms SEQ still opens the real step grid, not the HOME shortcut");
        turn(ENC_SELECT,4); CHECK(stc == 4u, "SELECT still moves the full step editor's cursor");
        tap(BTN_SEQ); view_mailbox = 0; ui_draw();
        CHECK(page == PAGE_HOME && home_view == i && focus == FOCUS_SEQ && title_at("SEQ") &&
              sq_n == 1u && sq[0].note == 60u, "short SEQ leaves the step editor without losing the phrase");
    }

    boot(); owed = 0; sq[0].step = 0; sq[0].note = 60; sq[0].len = 8; sq_n = 1;
    tap(BTN_PLAY); pump(30); generation = seq_gen;
    tap(BTN_SEQ); ui_draw();
    CHECK(seq_run && seq_gen == generation && generated_count[60] &&
          title_at("SEQ") && host_led_keys[BTN_SEQ] && host_led_keys[BTN_PLAY],
          "SEQ shortcut during PLAY retains transport generation, sounding notes and the fixed FM title");
    tap(BTN_ARP); tap(BTN_HOME);
    CHECK(seq_run && seq_gen == generation && !arp_on && !host_led_keys[BTN_SEQ],
          "entering ARP or leaving a shortcut does not stop or restart the playing phrase");
    tap(BTN_SEQ); tap(BTN_REC); ui_draw();
    CHECK(rec == 2u && rec_to-rec_from == sq_len() && seq_run && title_at("SEQ"),
          "SEQ keeps its title while REC starts the existing one-loop overdub workflow");
    tap(BTN_PLAY); pump(250);
    CHECK(!rec && !seq_run && !generated_count[60] && title_at("SEQ"),
          "PLAY still stops recording and releases generated notes from the SEQ shortcut");
}

static void play_led_checks(void)
{
    static const uint8_t tempos[] = {40,80};
    uint32_t i, quarter_ms;
    for (i = 0; i < sizeof tempos/sizeof tempos[0]; i++) {
        boot(); owed = 0; input(1,0,0);
        CHECK(!host_led_keys[BTN_PLAY] && !host_led_glow[BTN_PLAY],
              "empty stopped phrase leaves PLAY's bright and glow layers off");
        tap(BTN_PLAY);
        CHECK(!seq_run && !host_led_keys[BTN_PLAY] && !host_led_glow[BTN_PLAY] &&
              !strcmp(message,"NOTHING RECORDED"), "PLAY on an empty phrase does not fabricate a light or transport");
        play_controls[UI_TEMPO] = tempos[i]; pl_dirty = 1;
        quarter_ms = 60000u/bpm();
        sq[0].step = 0; sq[0].note = 60; sq[0].len = 8; sq_n = 1;
        input(1,0,0);
        CHECK(!host_led_keys[BTN_PLAY] && host_led_glow[BTN_PLAY],
              "an available stopped phrase lights only PLAY's dim backlight");
        tap(BTN_PLAY);
        CHECK(seq_run && !host_led_keys[BTN_PLAY] && host_led_glow[BTN_PLAY],
              "PLAY beat flash waits for the audio clock to acknowledge this transport generation");
        pump(20);
        CHECK(host_led_keys[BTN_PLAY] && host_led_glow[BTN_PLAY] && generated_count[60],
              "the first sounding beat flashes PLAY above the available-phrase glow");
        pump(quarter_ms/2u); tap(BTN_ENV);
        CHECK(!host_led_keys[BTN_PLAY] && host_led_glow[BTN_PLAY] && seq_run,
              "between beats PLAY dims without going dark, including while navigating");
        pump(quarter_ms/2u);
        CHECK(host_led_keys[BTN_PLAY] && host_led_glow[BTN_PLAY],
              "PLAY flashes again on the next sample-clocked beat at both tested tempos");
        tap(BTN_PLAY);
        CHECK(!seq_run && !host_led_keys[BTN_PLAY] && host_led_glow[BTN_PLAY] && sq_n == 1u,
              "stopping clears PLAY's beat flash but keeps the available phrase backlit");
        input(1,1u<<BTN_REC,0); pump(400); input(0,0,0);
        CHECK(!sq_n && !seq_run && !rec && !host_led_keys[BTN_PLAY] && !host_led_glow[BTN_PLAY],
              "clearing the phrase also clears PLAY's dim backlight");
    }

    boot(); owed = 0; tap(BTN_REC); pump(20);
    CHECK(counting_in() && host_led_keys[BTN_PLAY] && !host_led_glow[BTN_PLAY],
          "empty-phrase count-in flashes PLAY on the beat without inventing a saved phrase");
    pump(250);
    CHECK(counting_in() && !host_led_keys[BTN_PLAY] && !host_led_glow[BTN_PLAY],
          "count-in beat flash switches off between beats when no phrase is available");
    pump(250);
    CHECK(counting_in() && host_led_keys[BTN_PLAY] && !host_led_glow[BTN_PLAY],
          "count-in continues flashing PLAY on subsequent beats");
    tap(BTN_PLAY);
    CHECK(!seq_run && !rec && !host_led_keys[BTN_PLAY] && !host_led_glow[BTN_PLAY],
          "stopping count-in clears PLAY when no phrase was recorded");
}

static void performance_checks(void)
{
    uint32_t i, generated;
    boot(); owed = 0;
    input(1,1u<<BTN_SEQ,0); input(400,0,0);
    CHECK(page == PAGE_STEP, "held SEQ opens the native step editor");
    input(1,0,1u<<7); input(1,0,0); turn(ENC_SELECT,4);
    input(1,0,1u<<11); input(1,0,0);
    CHECK(sq_n == 2u && sq[0].step == 0u && sq[0].note == 60u &&
          sq[1].step == 4u && sq[1].note == 64u, "step entry uses actual pitched keys and cursor positions");
    tap(BTN_PLAY); pump(30);
    CHECK(seq_run && generated_count[60] && !generated_count[64], "loop begins with its first recorded step");
    pump(500); CHECK(generated_count[64], "sample-clocked loop reaches its next quarter note");
    pump(1500); CHECK(generated_count[60], "one-bar phrase wraps without clock drift");
    tap(BTN_SAVE); tap(BTN_SAVE); pump(1100);
    CHECK(save_pending && !lit && !host_flash_erases, "flash stays deferred through even silent portions of a running loop");
    tap(BTN_PLAY); pump(250);
    generated = 0; for (i = 0; i < 128u; i++) generated += generated_count[i];
    CHECK(!seq_run && !generated && !lit, "PLAY stop releases generated notes, not just the transport flag");

    boot(); owed = 0; tap(BTN_REC); pump(1000);
    CHECK(rec == 2u && counting_in(), "stopped REC gives a real one-bar count-in");
    midi(0x90,60,100); pump(100); midi(0x80,60,0);
    CHECK(!sq_n && !rkeys[27u+60u], "early count-in notes are not added to the phrase");
    pump(950); midi(0x90,69,100); pump(166); midi(0x80,69,0); pump(2000);
    CHECK(!rec && seq_run && sq_n == 1u && sq[0].note == 69u && sq[0].step == 0u && sq[0].len == 2u,
          "MIDI note start/end quantize onto the audio clock and recording ends after one loop");
    tap(BTN_REC); pump(20); midi(0x90,72,100); pump(150); midi(0x80,72,0); pump(2000);
    CHECK(!rec && seq_run && sq_n == 2u, "recording under PLAY overdubs instead of discarding the phrase");
    input(1,1u<<BTN_REC,0); pump(400); input(0,0,0); pump(250);
    CHECK(!sq_n && !rec && !seq_run && !lit, "REC hold clears and stops without also starting a new recording");

    boot(); owed = 0; tap(BTN_ARP); tap(BTN_ARP);
    midi(0x90,60,100); midi(0x90,64,100); midi(0x90,67,100); pump(10);
    CHECK(arp_on && chord_n == 3u && generated_count[60], "ARP captures MIDI chord and starts on its first note");
    pump(125); CHECK(generated_count[64] && !generated_count[60], "arpeggiator advances and gates notes on its own sample clock");
    turn(ENC_KNOB4,1);
    midi(0x80,60,0); midi(0x80,64,0); midi(0x80,67,0); pump(200);
    CHECK(play_controls[UI_LATCH] && chord_n == 3u, "latched arp retains the last chord after release");
    tap(BTN_SAVE); tap(BTN_SAVE); pump(200);
    CHECK(save_pending && !host_flash_erases, "a latched chord blocks flash after all physical MIDI keys are released");
    tap(BTN_ARP);
    midi(0x90,72,100); pump(160);
    CHECK(chord_n == 1u && chord[0] == 72u, "next latched chord replaces the old one");
    turn(ENC_KNOB4,-1); midi(0x80,72,0); pump(250);
    generated = 0; for (i = 0; i < 128u; i++) generated += generated_count[i];
    CHECK(!chord_n && !generated && arp_note == 0xFFu, "unlatched arp releases all generated ownership");

    boot(); owed = 0;
    input(1,0,1u<<7); midi(0x90,60,100); pump(30);
    input(1,0,0); pump(30);
    CHECK(direct_count[60] == 1u && synth.voice[0].held, "local release cannot cut a MIDI-held copy of the same pitch");
    midi(0x80,60,0); pump(250);
    CHECK(!direct_count[60] && !lit, "last source release ends the shared pitch");
    tap(BTN_ARP); tap(BTN_ARP); midi(0x90,60,100); pump(30);
    midi(0xB0,123,0); pump(250);
    CHECK(!seq_run && !arp_on && !lit, "all-notes-off stops performance generators and cannot restart notes later");
    CHECK(!audio_fault, "native performance workflows keep audio ownership valid");
    boot(); owed = 0; sq[0].step = 0; sq[0].note = 60; sq[0].len = 8; sq_n = 1;
    tap(BTN_PLAY); pump(30); CHECK(sq_left[0] && generated_count[60], "long generated note before mode panic");
    ev_push(EV_MONO,1,0); audio();
    CHECK(!sq_left[0] && !generated_count[60] && arp_note == 0xFFu,
          "mode-change panic clears stale sequencer ownership as well as native voices");
}

static void combined_stress_checks(void)
{
    uint32_t i, b, frames = 0, active, moving;
    int32_t samples[512], initial;
    boot();
    edit.osc[0].table = 3; edit.osc[1].table = 6;
    edit.osc[0].position = edit.osc[1].position = 32768;
    edit.osc[1].gain = WT_GAIN_MAX; edit.osc[1].fine = -7;
    edit.filter_type = WT_FILTER_LP; edit.cutoff = 39320; edit.resonance = 26208;
    edit.lfo[0].rate = 2000; edit.lfo[0].shape = WT_LFO_TRIANGLE;
    edit.lfo[0].depth = 24570; edit.lfo[0].destination = WT_MOD_BOTH;
    edit.lfo[1].rate = 1000; edit.lfo[1].depth = 8190; edit.lfo[1].destination = WT_MOD_FILTER;
    for (i = 0; i < FX_N; i++) edit.fx[i*4u] = 30;
    patch_publish();
    for (i = 0; i < WT_VOICES; i++) performance_down(27u+48u+i*4u,48u+i*4u,100,0);
    input(1,0,0); audio(); initial = synth.voice[0].position[0]; moving = 0;
    for (b = 0; b < 512u; b++) {
        memset(samples,0,sizeof samples); app_audio(samples,HALF_FRAMES);
        for (i = 0; i < 512u; i++) {
            CHECK(samples[i] >= -8388607 && samples[i] <= 8388607, "combined workload stays bounded");
            if (samples[i]) frames++;
        }
        if (synth.voice[0].position[0] != initial) moving = 1;
        if (!(b&7u)) {
            home_view = (b>>3)%3u; ui_draw(); input(46,0,0);
        }
    }
    active = 0; for (i = 0; i < WT_VOICES; i++) active += synth.voice[i].active != 0;
    CHECK(active == 8u && moving && frames > 10000u && audio_effects.on == 15u && !audio_fault,
          "eight real dual-oscillator voices, moving modulation, resonant filters, four effects and live drawing run together");
    midi(0xB0,123,0); memset(edit.fx,0,sizeof edit.fx); patch_publish();
    for (b = 0; b < 50u; b++) audio();
    CHECK(!lit && !audio_fault, "combined workload panic ends every voice safely");
}

static void recovery_policy_checks(void)
{
    bootguard_t guard = {0,0,0};
    CHECK(bootguard_begin(&guard) == BOOT_NORMAL && guard.pending == 1u, "cold boot initializes the retained-RAM boot guard");
    CHECK(bootguard_begin(&guard) == BOOT_NORMAL && guard.failed == 1u, "first unsuccessful warm boot permits another normal start");
    CHECK(bootguard_begin(&guard) == BOOT_RECOVERY, "repeated early resets select application USB rescue");
    CHECK(bootguard_begin(&guard) == BOOT_ROM, "a reset in rescue selects ROM fallback");
    bootguard_clear(&guard);
    CHECK(bootguard_begin(&guard) == BOOT_NORMAL && !guard.failed, "stable-boot cleanup restores normal policy");
    CHECK(bootguard_manual(1) && !bootguard_manual(0) && !bootguard_manual(3),
          "OCT- alone requests rescue; both octave buttons retain calibration access");
}

#ifdef WT_CPU1
static void cpu1_ui_checks(void)
{
    wt_patch_t patch;
    uint32_t generation, reset, i;
    boot(); fm1_cpu1_on = wt_cpu1_enabled = 1; fm1_cpu1_gave_up = 0;
    tap(BTN_GLO);
    CHECK(!bank && ui_banks() == 4u, "CPU1 firmware retains only the normal four GLOBAL pages");
    turn(ENC_SELECT,3);
    CHECK(bank == 3u && ui_field(0) == U_PCOUT && ui_field(3) == U_BUILD,
          "normal system diagnostics retain their fourth GLOBAL page");
    patch = edit; generation = patch_generation;
    turn(ENC_KNOB2,1); turn(ENC_KNOB3,-1); turn(ENC_KNOB4,1);
    CHECK(!memcmp(&edit,&patch,sizeof patch) && patch_generation == generation && wt_cpu1_enabled,
          "normal CPU/peak/build fields remain read-only and never toggle the worker");
    reset = cpu_reset_generation; tap(BTN_SEL);
    CHECK(cpu_reset_generation == reset+1, "normal diagnostic meter reset stays audio-owned");
    audio(); CHECK(cpu_reset_seen == cpu_reset_generation, "audio consumes the normal meter reset");
    ui_draw();
    {
        uint32_t matches = 1;
        cv_begin(240,16,C_BLACK);
        cv_text((240-text_w(&FONT_S,"GLOBAL  4/4"))/2,0,&FONT_S,"GLOBAL  4/4",WT_ACC);
        for (i = 0; i < 240u*16u; i++)
            if (fb[24u*240u+i] != swap16(cv_px[i])) matches = 0;
        CHECK(matches,"normal diagnostic screen shows the restored four-page header");
    }
    turn(ENC_SELECT,1);
    CHECK(!bank && ui_field(0) == U_TUNE, "GLOBAL wraps to its normal first page, not worker profiling");
    fm1_cpu1_on = 0; fm1_cpu1_gave_up = FM1_CPU1_MISREAD;
    input(1,0,0);
    CHECK(!strcmp(message,"CPU1 FAILED / CPU0 FALLBACK"), "guarded fallback still notifies the user");
    fm1_cpu1_on = 1; fm1_cpu1_gave_up = 0;
}

#if WT_CPU1_JOB_SAMPLES == 64
static void cpu1_batch_checks(void)
{
    uint32_t i, jobs, batches;
    boot(); fm1_cpu1_on = wt_cpu1_enabled = 1; fm1_cpu1_gave_up = 0;
    for (i = 0; i < WT_VOICES; i++) CHECK(!ev_push(EV_ON,48u+i,100),"batch test note");
    audio();
    jobs = wt_cpu1_jobs; batches = wt_cpu1_batches; audio();
    CHECK(wt_cpu1_jobs-jobs == 4u && wt_cpu1_batches-batches == 4u,
          "settled eight-voice callback actually uses four 64-sample worker jobs");
    midi(0xB0,1,127);
    jobs = wt_cpu1_jobs; batches = wt_cpu1_batches; audio();
    CHECK(wt_cpu1_jobs-jobs == 8u && wt_cpu1_batches == batches,
          "controller smoothing retains eight 32-sample jobs and original update boundaries");
    boot(); fm1_cpu1_on = wt_cpu1_enabled = 1;
    for (i = 0; i < WT_VOICES; i++) CHECK(!ev_push(EV_ON,48u+i,100),"arp batch test note");
    plbuf[plcur].arp = 1;
    batches = wt_cpu1_batches; audio();
    CHECK(wt_cpu1_batches == batches,"arp/sequence event boundaries never use speculative batching");
}
#endif
#endif

int main(void)
{
    int32_t samples[512] = {0};
    uint32_t i, generation;
    wt_synth_init(&synth); wt_patch_init(&edit); ready = 1;
    patch_publish();
    CHECK(ev_push(EV_ON, 60, 100) == 0, "application note-on");
    app_audio(samples, 256);
    CHECK(lit && synth.voice[0].note == 60u, "published patch and note reach audio owner");
    for (i = 0; i < WT_EVENT_COUNT; i++) CHECK(ev_push(EV_ON, i, 100) == 0, "fill event ring");
    CHECK(ev_push(EV_OFF, 60, 0) == -1 && event_overflow, "overflow is explicitly reported");
    memset(samples, 0, sizeof samples); app_audio(samples, 256);
    CHECK(!lit && event_read == event_write && !event_overflow, "overflow panics rather than losing a note-off");
    CHECK(message && !strcmp(message, "EVENT OVERFLOW"), "overflow is visible on the panel");

    event_read = event_write = 0xFFFFFFFEu;
    CHECK(ev_push(EV_ON, 69, 100) == 0, "ring counter wrap note");
    memset(samples, 0, sizeof samples); app_audio(samples, 256);
    CHECK(ev_push(EV_OFF, 69, 0) == 0 && ev_push(EV_PANIC, 0, 0) == 0 && event_write == 1u,
          "ring counters wrap without losing an event");
    memset(samples, 0, sizeof samples); app_audio(samples, 256);
    CHECK(!lit && !audio_fault, "wrapped events release correctly");

    edit.osc[0].table = 3; patch_publish();
    edit.osc[0].table = 7; edit.osc[0].position = 65536; patch_publish();
    memset(samples, 0, sizeof samples); app_audio(samples, 256);
    CHECK(synth.patch.osc[0].table == 7u && synth.patch.osc[0].position == 65536u,
          "multiple publications deliver the complete latest patch");
    generation = patch_generation;
    edit.osc[0].gain = WT_GAIN_MAX + 1u; patch_publish();
    CHECK(patch_generation == generation && !strcmp(message, "INVALID PATCH"),
          "bad UI publication is rejected visibly");
    ui_checks(); octave_led_checks(); layout_checks(); output_quality_checks(); oscillator_home_checks(); envelope_home_checks(); lfo_home_checks(); ui_parity_checks();
    storage_checks(); effect_checks(); performance_home_checks(); play_led_checks(); performance_checks();
    midi_controller_checks(); control_cache_checks(); combined_stress_checks(); recovery_policy_checks();
#ifdef WT_CPU1
    cpu1_ui_checks();
#if WT_CPU1_JOB_SAMPLES == 64
    cpu1_batch_checks();
#endif
#endif
#ifdef WT_PROFILE_TEST
    profile_checks();
#endif
    printf("WT application ownership/event checks: %u failures\n", failures);
    return failures ? 1 : 0;
}
