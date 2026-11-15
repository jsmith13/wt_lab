/* SPDX-License-Identifier: GPL-3.0-only */
/* Main-owned WT editor. Included by wtlab.c; audio state is read only through its mailbox.
 * Gestures, layout, legends, and A/B conventions follow the GPL FM Lab reference. */
static const char *message = "WT LAB";
static uint32_t message_at;

enum { PAGE_HOME, PAGE_EDIT, PAGE_ENV, PAGE_LFO, PAGE_GLO, PAGE_FX, PAGE_STEP };
enum { FOCUS_OFF, FOCUS_EDIT, FOCUS_ENV, FOCUS_LFO, FOCUS_FX, FOCUS_ARP, FOCUS_SEQ };
#define LONG_MS 400u
#define WT_ACC RGB(180, 140, 255)
#define WT_LABEL RGB(150, 150, 150)
#define WT_OFF RGB(90, 90, 90)
#define WT_TRACE_FILL RGB(52, 62, 120)
#define WT_FILTER_CURVE RGB(110, 200, 160)
#define WT_PLOT_Y 64u
#define WT_ENV_Y 44u
#define WT_ENV_H 104u
#define WT_CELLS_Y 152u
typedef struct { uint32_t at, active, fired; } ui_hold_t;
static uint32_t page, bank, selected, mono, midi_channel, focus, home_view, env_selected, lfo_selected, fx_selected, arp_selected;
static uint32_t ab, sel_used, sel_context, sel_target, previous_keys, sounding;
static uint8_t played[27], played_channel[27];
static ui_hold_t hold[4], sel_hold, ab_hold, seq_hold, rec_hold, midi_hold;
static wt_patch_t other;
static wt_view_t display;
static int32_t octave;
static uint32_t ui_draw_signature;
static uint8_t ui_spectrum_peak[240];
static uint32_t ui_touched_field, ui_touched_at;
static uint32_t ui_banks(void);
enum {
    U_NONE, U_TABLE, U_POSITION, U_LEVEL, U_SEMI, U_FINE, U_POSENV,
    U_AMP_A, U_AMP_D, U_AMP_S, U_AMP_R, U_ENV_A, U_ENV_D, U_ENV_S, U_ENV_R,
    U_RATE, U_SHAPE, U_DEPTH, U_DEST, U_SYNC, U_CUTOFF, U_RESONANCE, U_TYPE, U_TRACK,
    U_FDEPTH, U_VELOCITY, U_MIDI, U_MODE, U_POSENV1, U_POSENV2, U_TUNE, U_TUNE_FINE,
    U_BITS, U_SRATE, U_OUTPUT, U_SPAN, U_PCOUT, U_CPU, U_PEAK, U_BUILD, U_FX, U_FX_END = U_FX+16,
    U_PLAY = U_FX_END, U_LENGTH = U_PLAY+PL_N,
    U_LEVEL1, U_POSITION1, U_DETUNE1, U_PENV1, U_LEVEL2, U_POSITION2, U_DETUNE2, U_PENV2, U_DETUNE
};
static const char *const UI_PAGE[] = {"WAVETABLE", "OSCILLATOR", "ENVELOPE", "LFO", "GLOBAL", "EFFECTS", "STEP EDIT"};
static const char *const SHAPES[] = {"SINE", "TRI", "SAW", "SQUARE", "RANDOM"};
static const char *const DESTS[] = {"NONE", "POS 1", "POS 2", "BOTH", "PITCH", "FILTER"};
static const char *const FILTERS[] = {"OFF", "LOW PASS", "BAND PASS", "HIGH PASS"};
static const uint32_t SPANS[] = {1000, 2000, 5000, 10000, 20000};
static const char *const EFFECT_NAMES[] = {"CHORUS", "PHASER", "DELAY", "REVERB"};
static const char *const PLAY_LABELS[] = {"TEMPO", "PATTERN", "OCTAVES", "LATCH", "BARS", "DIV", "CLICK",
                                         "GATE", "RATE", "SWING", "VELOCITY"};
static const uint8_t PLAY_MAX[] = {99,4,3,1,2,2,1,99,5,25,99};
static uint32_t spectrum_span = 2;
static int32_t scope_peak = 512;

static void say(const char *text) { message = text; message_at = fm1_ms; ui_touched_field = U_NONE; }

static char *ui_text(char *dst, const char *src)
{
    while (*src) *dst++ = *src++;
    *dst = 0;
    return dst;
}

static char *ui_number(char *dst, int32_t value)
{
    char tmp[12];
    uint32_t n = 0, u;
    if (value < 0) { *dst++ = '-'; value = -value; }
    u = (uint32_t)value;
    do { tmp[n++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    while (n) *dst++ = tmp[--n];
    *dst = 0;
    return dst;
}

#include "wt_workflow.c"

static uint32_t ui_osc_field(uint32_t field, uint32_t *oscillator)
{
    static const uint8_t fields[] = {U_LEVEL,U_POSITION,U_DETUNE,U_POSENV};
    *oscillator = selected;
    if (field >= U_LEVEL1 && field <= U_PENV2) {
        *oscillator = (field-U_LEVEL1)/4u;
        return fields[(field-U_LEVEL1)%4u];
    }
    return field;
}

static uint32_t ui_field(uint32_t row)
{
    if (page == PAGE_HOME) {
        if (focus == FOCUS_ENV) return (env_selected ? U_ENV_A : U_AMP_A) + row;
        if (focus == FOCUS_LFO) return U_RATE + row;
        if (focus == FOCUS_FX) return U_FX+row*4u;
        if (focus == FOCUS_ARP) {
            static const uint8_t controls[2][4] = {{UI_TEMPO,UI_APAT,UI_AOCT,UI_LATCH},
                                                 {UI_AGATE,UI_ARATE,UI_SWING,UI_AVEL}};
            return U_PLAY+controls[bank][row];
        }
        if (focus == FOCUS_SEQ) {
            static const uint8_t controls[] = {UI_TEMPO,UI_BARS,UI_DIV,UI_CLICK};
            return U_PLAY+controls[row];
        }
        if (home_view == 1u) return row == 3u ? U_SPAN : U_CUTOFF + row;
        {
            static const uint8_t fields[2][4] = {
                {U_LEVEL1,U_POSITION1,U_LEVEL2,U_POSITION2},
                {U_DETUNE1,U_PENV1,U_DETUNE2,U_PENV2}
            };
            return fields[bank][row];
        }
    }
    if (page == PAGE_EDIT)
        return bank ? (row == 0 ? U_FINE : row == 1 ? U_POSENV : U_NONE) : U_TABLE + row;
    if (page == PAGE_ENV) {
        static const uint8_t routing[] = {U_POSENV1, U_POSENV2, U_FDEPTH, U_TRACK};
        return bank == 2u ? routing[row] : (bank ? U_ENV_A : U_AMP_A) + row;
    }
    if (page == PAGE_LFO) return bank & 1u ? (row == 0 ? U_SYNC : U_NONE) : U_RATE + row;
    if (page == PAGE_GLO) {
        static const uint8_t fields[4][4] = {
            {U_TUNE, U_TUNE_FINE, U_MIDI, U_MODE},
            {U_CUTOFF, U_RESONANCE, U_TYPE, U_TRACK},
            {U_VELOCITY, U_SRATE, U_BITS, U_OUTPUT},
            {U_PCOUT, U_CPU, U_PEAK, U_BUILD}
        };
        return fields[bank][row];
    }
    if (page == PAGE_FX) return U_FX+bank*4u+row;
    if (page == PAGE_STEP) {
        static const uint8_t fields[] = {U_LENGTH,U_PLAY+UI_TEMPO,U_PLAY+UI_BARS,U_PLAY+UI_DIV};
        return fields[row];
    }
    return U_NONE;
}

static int32_t ui_adjust(int32_t value, int32_t delta, int32_t step, int32_t lo, int32_t hi)
{
    int64_t n = value + (int64_t)delta * step;
    return n < lo ? lo : n > hi ? hi : (int32_t)n;
}

static void wt_midi_touch(uint32_t field, uint32_t oscillator)
{
    uint32_t target = MI_NONE;
    if (field >= U_FX && field < U_FX_END) target = wt_midi_fx_targets[field-U_FX];
    else if (field >= U_AMP_A && field <= U_AMP_R) target = 21u+field-U_AMP_A;
    else if (field >= U_ENV_A && field <= U_ENV_R) target = 25u+field-U_ENV_A;
    else switch (field) {
    case U_POSITION: target = 13u+oscillator*4u; break;
    case U_LEVEL: target = 14u+oscillator*4u; break;
    case U_SEMI: case U_FINE: case U_DETUNE: target = 15u+oscillator*4u; break;
    case U_POSENV: target = 16u+oscillator*4u; break;
    case U_POSENV1: target = 16; break;
    case U_POSENV2: target = 20; break;
    case U_RATE: target = 29u+lfo_selected*2u; break;
    case U_DEPTH: target = 30u+lfo_selected*2u; break;
    case U_CUTOFF: target = MI_CUTOFF; break;
    case U_RESONANCE: target = MI_RESONANCE; break;
    case U_TRACK: target = 33; break;
    case U_FDEPTH: target = 34; break;
    case U_VELOCITY: target = 35; break;
    default: break;
    }
    mi_panel_target(target);
}

static void ui_edit(uint32_t field, int32_t delta)
{
    uint32_t original = field, oscillator;
    wt_osc_patch_t *o;
    wt_lfo_patch_t *l = &edit.lfo[lfo_selected];
    uint32_t fine = (fm1_in.buttons >> BTN_SEL) & 1u;
    int32_t step = fine ? 328 : 3277;
    wt_adsr_t *env;
    field = ui_osc_field(field,&oscillator); o = &edit.osc[oscillator];
    if (field == U_NONE || field == U_CPU || field == U_PEAK || field == U_BUILD) return;
    wt_midi_touch(field,oscillator);
    if (original >= U_LEVEL1 && original <= U_PENV2) selected = oscillator;
    ui_touched_field = original; ui_touched_at = fm1_ms;
    message = 0;
    if (fine) sel_used = 1;
    if (field >= U_FX && field < U_FX_END) {
        uint32_t index = field-U_FX;
        edit.fx[index] = (uint8_t)ui_adjust(edit.fx[index], delta, fine ? 1 : 5, 0, 99);
        patch_publish(); return;
    }
    if (field >= U_PLAY && field < U_LENGTH) {
        uint32_t index = field-U_PLAY, old = play_controls[index];
        play_controls[index] = (uint8_t)ui_adjust((int32_t)old, delta, 1, 0, PLAY_MAX[index]);
        if (index == UI_LATCH && old != play_controls[index]) performance_latch();
        if (index == UI_BARS && stc >= sq_len()) stc = 0;
        pl_dirty = 1; return;
    }
    if (field == U_LENGTH) { st_len(delta); return; }
#define ADJ(member, unit, lo, hi) (member) = ui_adjust((int32_t)(member), delta, (unit), (lo), (hi))
    switch (field) {
    case U_TABLE: o->table = ui_wrap(o->table, delta, WT_TABLE_COUNT); break;
    case U_POSITION: ADJ(o->position, step, 0, WT_POSITION_MAX); break;
    case U_LEVEL: ADJ(o->gain, fine ? 164 : 1638, 0, WT_GAIN_MAX); break;
    case U_SEMI: ADJ(o->semitone, 1, -24, 24); break;
    case U_FINE: ADJ(o->fine, fine ? 1 : 13, -256, 256); break;
    case U_DETUNE: {
        int32_t total = ui_adjust(o->semitone*256+o->fine,delta,fine ? 1 : 256,-25*256,25*256);
        o->semitone = wts_clamp(total/256,-24,24);
        o->fine = total-o->semitone*256;
        break;
    }
    case U_POSENV: ADJ(o->position_env, step, -(int32_t)WT_POSITION_MAX, WT_POSITION_MAX); break;
    case U_POSENV1: ADJ(edit.osc[0].position_env, step, -(int32_t)WT_POSITION_MAX, WT_POSITION_MAX); break;
    case U_POSENV2: ADJ(edit.osc[1].position_env, step, -(int32_t)WT_POSITION_MAX, WT_POSITION_MAX); break;
    case U_RATE: ADJ(l->rate, fine ? 10 : 250, 0, 20000); break;
    case U_SHAPE: ADJ(l->shape, 1, 0, WT_LFO_SHAPES - 1u); break;
    case U_DEPTH: ADJ(l->depth, fine ? 164 : 1638, 0, WT_GAIN_MAX); break;
    case U_DEST: ADJ(l->destination, 1, 0, WT_MOD_DESTS - 1u); break;
    case U_SYNC: ADJ(l->key_sync, 1, 0, 1); break;
    case U_CUTOFF: ADJ(edit.cutoff, step, 0, WT_CUTOFF_MAX); break;
    case U_RESONANCE: ADJ(edit.resonance, fine ? 164 : 1638, 0, WT_GAIN_MAX); break;
    case U_TYPE: edit.filter_type = ui_wrap(edit.filter_type, delta, 4); break;
    case U_TRACK: ADJ(edit.keytrack, fine ? 164 : 1638, 0, WT_GAIN_MAX); break;
    case U_FDEPTH: ADJ(edit.filter_env, step, -(int32_t)WT_POSITION_MAX, WT_POSITION_MAX); break;
    case U_VELOCITY: ADJ(edit.velocity, fine ? 164 : 1638, 0, WT_GAIN_MAX); break;
    case U_TUNE: ADJ(preferences.transpose, 1, -24, 24); break;
    case U_TUNE_FINE: ADJ(preferences.fine, fine ? 1 : 13, -256, 256); break;
    case U_BITS: ADJ(preferences.bits, -1, 0, 3); break;
    case U_SRATE: ADJ(preferences.sample_rate, -1, 0, WT_SAMPLE_RATE_COUNT-1u); break;
    case U_OUTPUT: ADJ(preferences.output_mono, 1, 0, 1); break;
    case U_PCOUT: ADJ(preferences.pc_send, 1, 0, 1); break;
    case U_SPAN: ADJ(spectrum_span, 1, 0, 4); return;
    case U_MIDI:
        ADJ(midi_channel, 1, 0, 16);
        performance_stop(); wts_clear(input_owner, sizeof input_owner);
        ev_push(EV_PANIC, 0, 0);                /* changing the receive channel cannot strand notes */
        mi_reset_input(); mi_publish(); ev_push(EV_SUSTAIN,0,0);
        return;
    case U_MODE:
        ADJ(mono, 1, 0, 1);
        ev_push(EV_MONO, mono, 0);
        return;
    default: {
        uint32_t index = field >= U_ENV_A ? field - U_ENV_A : field - U_AMP_A;
        env = field >= U_ENV_A ? &edit.envelope : &edit.amp;
        if (index == 0) ADJ(env->attack, fine ? 1 : 25, 0, 5000);
        if (index == 1) ADJ(env->decay, fine ? 1 : 25, 0, 5000);
        if (index == 2) ADJ(env->sustain, fine ? 164 : 1638, 0, WT_GAIN_MAX);
        if (index == 3) ADJ(env->release, fine ? 1 : 25, 0, 5000);
        break;
    }
    }
#undef ADJ
    patch_publish();
}

static const char *ui_value(uint32_t field, char *out)
{
    uint32_t original = field, oscillator;
    const wt_osc_patch_t *o;
    const wt_lfo_patch_t *l = &edit.lfo[lfo_selected];
    int32_t n = 0;
    const char *unit = "", *label = "";
    field = ui_osc_field(field,&oscillator); o = &edit.osc[oscillator];
    if (field >= U_FX && field < U_FX_END) {
        static const char *const names[4][4] = {{"MIX","RATE","DEPTH","WIDTH"}, {"MIX","RATE","DEPTH","FEEDBACK"},
                                                {"MIX","TIME","FEEDBACK","TONE"}, {"MIX","DECAY","DAMP","PRE-DELAY"}};
        static const char *const mixes[] = {"CHORUS MIX","PHASER MIX","DELAY MIX","REVERB MIX"};
        uint32_t index = field-U_FX, effect = index/4u, row = index%4u, value = edit.fx[index];
        label = page == PAGE_HOME && focus == FOCUS_FX && row == 0u ? mixes[effect] : names[effect][row];
        n = (int32_t)(value*100u/99u);
        if (effect < 2u && row == 1u) {
            char *p = ui_number(out, FX_RATE[value]/100u);
            *p++ = '.'; *p++ = (char)('0'+FX_RATE[value]/10u%10u); *p++ = (char)('0'+FX_RATE[value]%10u);
            ui_text(p, "HZ"); return label;
        }
        if (effect == FX_DLY && row == 1u) { n = (int32_t)(DLY_T[value]*1000u/FS); unit = "MS"; }
        if (effect == FX_REV && row == 1u) {
            char *p = ui_number(out, RV_RT[value]/10u); *p++ = '.'; *p++ = (char)('0'+RV_RT[value]%10u);
            ui_text(p, "S"); return label;
        }
        if (effect == FX_REV && row == 3u) { n = (int32_t)value; unit = "MS"; }
        ui_text(ui_number(out, n), unit); return label;
    }
    if (field >= U_PLAY && field < U_LENGTH) {
        static const char *const patterns[] = {"UP","DOWN","UP/DOWN","RANDOM","PLAYED"};
        static const char *const rates[] = {"1/4","1/8","1/8T","1/16","1/16T","1/32"};
        static const char *const divisions[] = {"1/16","1/8","1/4"};
        uint32_t index = field-U_PLAY, value = play_controls[index];
        label = PLAY_LABELS[index]; n = (int32_t)value;
        if (index == UI_TEMPO) { n = (int32_t)bpm(); unit = " BPM"; }
        if (index == UI_APAT) { ui_text(out, patterns[value]); return label; }
        if (index == UI_AOCT || index == UI_AGATE) n++;
        if (index == UI_LATCH || index == UI_CLICK) { ui_text(out, value ? "ON" : "OFF"); return label; }
        if (index == UI_BARS) n = 1 << value;
        if (index == UI_DIV) { ui_text(out, divisions[value]); return label; }
        if (index == UI_ARATE) { ui_text(out, rates[value]); return label; }
        if (index == UI_AGATE) unit = "%";
        if (index == UI_SWING) { n += 50; unit = "%"; }
        if (index == UI_AVEL) n = value ? (int32_t)((value*127u+49u)/99u) : 1;
        ui_text(ui_number(out, n), unit); return label;
    }
    if (field == U_LENGTH) {
        uint32_t i;
        for (i = 0; i < sq_n && sq[i].step != stc; i++) {}
        if (i == sq_n) ui_text(out, "EMPTY"); else ui_number(out, sq[i].len);
        return "LENGTH";
    }
    switch (field) {
    case U_TABLE: ui_text(out, wt_table_info[o->table].name); return "TABLE";
    case U_POSITION: label = "POSITION"; n = (int32_t)o->position * 100 / 65536; unit = "%"; break;
    case U_LEVEL: label = "LEVEL"; n = (int32_t)o->gain * 100 / 32768; unit = "%"; break;
    case U_SEMI: label = "SEMITONE"; n = o->semitone; break;
    case U_FINE: label = "FINE"; n = o->fine * 100 / 256; unit = " CENT"; break;
    case U_DETUNE: label = "DETUNE"; n = o->semitone*256+o->fine; break;
    case U_POSENV: label = "ENV > POS"; n = o->position_env * 100 / 65536; unit = "%"; break;
    case U_POSENV1: label = "ENV > POS1"; n = edit.osc[0].position_env * 100 / 65536; unit = "%"; break;
    case U_POSENV2: label = "ENV > POS2"; n = edit.osc[1].position_env * 100 / 65536; unit = "%"; break;
    case U_RATE: {
        char *p = ui_number(out, (int32_t)(l->rate / 1000u));
        *p++ = '.'; *p++ = (char)('0' + l->rate / 100u % 10u); *p++ = (char)('0' + l->rate / 10u % 10u);
        ui_text(p, " HZ");
        return "RATE";
    }
    case U_SHAPE: ui_text(out, SHAPES[l->shape]); return "SHAPE";
    case U_DEPTH: label = "DEPTH"; n = (int32_t)l->depth * 100 / 32768; unit = "%"; break;
    case U_DEST: ui_text(out, DESTS[l->destination]); return "DEST";
    case U_SYNC: ui_text(out, l->key_sync ? "ON" : "OFF"); return "KEY SYNC";
    case U_CUTOFF: {
        uint32_t hz = wt_filter_hz(edit.cutoff);
        char *p;
        if (hz < 1000u) ui_text(ui_number(out, (int32_t)hz), "HZ");
        else {
            p = ui_number(out, (int32_t)(hz/1000u)); *p++ = '.';
            *p++ = (char)('0'+hz/100u%10u);
            if (hz < 10000u) *p++ = (char)('0'+hz/10u%10u);
            ui_text(p,"KHZ");
        }
        return page == PAGE_HOME ? "CUT" : "CUTOFF";
    }
    case U_RESONANCE: label = "RESO"; n = (int32_t)edit.resonance * 100 / 32768; break;
    case U_TYPE: ui_text(out, FILTERS[edit.filter_type]); return page == PAGE_HOME ? "TYPE" : "FILTER";
    case U_TRACK: label = "KEY TRACK"; n = (int32_t)edit.keytrack * 100 / 32768; unit = "%"; break;
    case U_FDEPTH: label = "ENV > CUT"; n = edit.filter_env * 100 / 65536; unit = "%"; break;
    case U_VELOCITY: label = "VELOCITY"; n = (int32_t)edit.velocity * 100 / 32768; unit = "%"; break;
    case U_TUNE: label = "TRANSPOSE"; n = preferences.transpose; break;
    case U_TUNE_FINE: label = "MASTER FINE"; n = preferences.fine * 100 / 256; unit = " CENT"; break;
    case U_BITS: {
        static const char *const values[] = {"24 BIT", "16 BIT", "12 BIT", "8 BIT"};
        ui_text(out, values[preferences.bits]); return "BIT DEPTH";
    }
    case U_SRATE: {
        static const char *const values[] = {"44.1K","32K","22K","16K","11K","8K"};
        ui_text(out,values[preferences.sample_rate]); return "SAMPLE RATE";
    }
    case U_OUTPUT: ui_text(out, preferences.output_mono ? "MONO" : "STEREO"); return "OUTPUT";
    case U_PCOUT: ui_text(out, preferences.pc_send ? "ON" : "OFF"); return "PROGRAM OUT";
    case U_CPU: case U_PEAK:
#ifdef DINGHY_HOST
        ui_text(out, "NOT DEVICE");
#else
        ui_text(ui_number(out, (int32_t)(field == U_CPU ? cpu_percent : cpu_peak)), "%");
#endif
        return field == U_CPU ? "APP CPU" : "APP PEAK";
    case U_BUILD: {
        uint32_t i = 0;
        while (i < 20u && APP_VER[i]) { out[i] = APP_VER[i]; i++; }
        if (APP_VER[i]) out[17] = out[18] = out[19] = '.';
        out[i] = 0;
        return "BUILD";
    }
    case U_SPAN: label = "SPAN"; n = (int32_t)(SPANS[spectrum_span] / 1000u); unit = "KHZ"; break;
    case U_MIDI: if (!midi_channel) { ui_text(out, "OMNI"); return "MIDI"; } label = "MIDI"; n = (int32_t)midi_channel; break;
    case U_MODE: ui_text(out, mono ? "MONO" : "POLY"); return "MODE";
    case U_NONE: *out = 0; return "";
    default: {
        const wt_adsr_t *e = field >= U_ENV_A ? &edit.envelope : &edit.amp;
        uint32_t index = field >= U_ENV_A ? field - U_ENV_A : field - U_AMP_A;
        static const char *const names[] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
        label = names[index];
        n = (int32_t)(index == 0 ? e->attack : index == 1 ? e->decay : index == 2 ? e->sustain : e->release);
        if (index == 2) { n = n * 100 / 32768; unit = "%"; }
        else { if (n < 3) n = 3; unit = " MS"; }
        break;
    }
    }
    if (field == U_DETUNE) {
        uint32_t thousandths = ((uint32_t)(n < 0 ? -n : n)*1000u+128u)/256u;
        char *p = out;
        if (n < 0) *p++ = '-';
        else if (n > 0) *p++ = '+';
        p = ui_number(p,(int32_t)(thousandths/1000u)); *p++ = '.';
        *p++ = (char)('0'+thousandths/100u%10u);
        *p++ = (char)('0'+thousandths/10u%10u);
        *p++ = (char)('0'+thousandths%10u); ui_text(p,"ST");
    } else ui_text(ui_number(out, n), unit);
    if (original >= U_LEVEL1 && original <= U_PENV2) {
        static const char *const labels[] = {"LEVEL 1","POSITION 1","DETUNE 1","ENV > POS1",
                                             "LEVEL 2","POSITION 2","DETUNE 2","ENV > POS2"};
        return labels[original-U_LEVEL1];
    }
    return label;
}

static const char *ui_legend(uint32_t field)
{
    static const char *const play_short[] = {"TEMPO","PATTERN","OCTAVE","LATCH","BARS","DIV","CLICK","GATE","RATE","SWING","VELO"};
    static const char *const fx[4][4] = {{"MIX","RATE","DEPTH","WIDTH"},{"MIX","RATE","DEPTH","FEEDBK"},
                                        {"MIX","TIME","FEEDBK","TONE"},{"MIX","DECAY","DAMP","PRE"}};
    if (field >= U_LEVEL1 && field <= U_PENV2) {
        static const char *const labels[] = {"LVL 1","POS 1","DET 1","PENV 1","LVL 2","POS 2","DET 2","PENV 2"};
        return labels[field-U_LEVEL1];
    }
    if (field == U_DETUNE) return "DETUNE";
    if (page == PAGE_HOME && focus == FOCUS_FX && field >= U_FX && field < U_FX_END && (field-U_FX)%4u == 0u)
        return EFFECT_NAMES[(field-U_FX)/4u];
    if (field == U_PCOUT) return "PC OUT";
    if (field == U_CPU) return "CPU";
    if (field == U_PEAK) return "PEAK";
    if (field == U_BUILD) return "BUILD";
    if (field >= U_FX && field < U_FX_END) return fx[(field-U_FX)/4u][(field-U_FX)%4u];
    if (field >= U_PLAY && field < U_LENGTH) return play_short[field-U_PLAY];
    if (field == U_LENGTH) return "LENGTH";
    static const char *const names[] = {
        "", "TABLE", "POS", "LEVEL", "SEMI", "FINE", "POSENV",
        "ATK", "DEC", "SUS", "REL", "ATK", "DEC", "SUS", "REL",
        "RATE", "SHAPE", "DEPTH", "DEST", "KEYSYN", "CUT", "RESO", "TYPE", "TRACK",
        "CUTENV", "VEL", "MIDI", "MODE", "ENV P1", "ENV P2", "TUNE", "M FINE",
        "BITS", "S RATE", "OUTPUT", "SPAN"
    };
    return names[field];
}

static void ui_legend_draw(void)
{
    uint32_t i;
    cv_begin(240, 16, C_BLACK);
    for (i = 0; i < 4u; i++) {
        const char *s = saving ? (i == 0 ? "LETTER" : i == 1 ? "CHAR" : i == 2 ? "SLOT" : "") : ui_legend(ui_field(i));
        int32_t pad = (60 - text_w(&FONT_S, s)) / 2;
        if (i) cv_rect((int32_t)i * 60, 4, 1, 8, RGB(44, 44, 44));
        cv_text((int32_t)i * 60 + (pad < 2 ? 2 : pad), 0, &FONT_S, s,
                fm1_in.buttons & (1u << BTN_SEL) ? WT_ACC : WT_OFF);
    }
    cv_blit(0, 208);
}

static void ui_wave_panel(uint32_t oscillator, uint32_t x, uint32_t height)
{
    wt_osc_t left = display.osc[oscillator], right = left;
    uint32_t i, pos = (uint32_t)left.position.current, frame = pos * 7u >> 16;
    uint32_t next = frame < 7u ? frame + 1u : frame, width = 120u, inset = 4u;
    uint32_t span = width-9u, rail = width-2u*inset;
    int32_t prev[3] = {0, 0, 0}, middle = (int32_t)height/2+6;
    int32_t half = (int32_t)height/2-40;
    uint32_t muted = oscillator_mutes >> oscillator & 1u;
    const char *text = wt_table_info[display.osc[oscillator].table].name;
    /* Closest Q16 positions to each neighboring frame endpoint. */
    left.position.current = (int32_t)((frame * 65536u + 6u) / 7u);
    right.position.current = (int32_t)((next * 65536u + 6u) / 7u);
    cv_line((int32_t)x+4, middle, (int32_t)(x+width)-5, middle, RGB(30, 30, 30));
    for (i = 0; i <= span; i++) {
        uint32_t phase = i * (0xFFFFFFFFu / span), j;
        int32_t samples[3];
        samples[0] = wt_osc_peek(&left, phase); samples[1] = wt_osc_peek(&right, phase);
        samples[2] = wt_osc_peek(&display.osc[oscillator], phase);
        for (j = 0; j < 3u; j++) {
            int32_t y = middle - samples[j] * half / 32768;
            uint16_t color = j == 0 ? RGB(70, 74, 100) : j == 1 ? WT_OFF :
                             muted || !edit.osc[oscillator].gain ? WT_OFF : C_WHITE;
            if (i) cv_line((int32_t)(x+i)+3, prev[j], (int32_t)(x+i)+4, y, color);
            prev[j] = y;
        }
    }
    {
        uint32_t start = x+inset+28u, length = rail-28u;
        cv_text((int32_t)x+4,(int32_t)height-32,&FONT_S,"POS",WT_LABEL);
        cv_rect((int32_t)start,(int32_t)height-25,(int32_t)length,3,RGB(40,40,40));
        for (i = 0; i < 8u; i++)
            cv_rect((int32_t)(start+i*(length-2u)/7u),(int32_t)height-27,1,7,RGB(70,70,70));
        cv_rect((int32_t)(start+edit.osc[oscillator].position*(length-2u)/65536u),
                (int32_t)height-28,1,9,WT_LABEL);
        cv_rect((int32_t)(start+pos*(length-2u)/65536u),(int32_t)height-29,2,11,WT_ACC);
        cv_text((int32_t)x+4,(int32_t)height-16,&FONT_S,"LVL",WT_LABEL);
        cv_rect((int32_t)start,(int32_t)height-8,(int32_t)length,4,RGB(40,40,40));
        cv_rect((int32_t)start,(int32_t)height-8,(int32_t)(edit.osc[oscillator].gain*length/WT_GAIN_MAX),
                4,muted ? WT_OFF : C_WHITE);
    }
    cv_text((int32_t)x+((int32_t)width-text_w(&FONT_S,text))/2, 0, &FONT_S, text,
            oscillator == selected ? WT_ACC : WT_LABEL);
    if (muted || !edit.osc[oscillator].gain) {
        text = muted ? "OFF" : "SILENT";
        cv_text((int32_t)x+((int32_t)width-text_w(&FONT_S,text))/2, 18, &FONT_S, text, WT_OFF);
    }
}

static void ui_wave_draw(uint32_t height)
{
    cv_begin(240, height, C_BLACK);
    ui_wave_panel(0, 0, height);
    ui_wave_panel(1, 120, height);
    cv_rect(119, 0, 1, (int32_t)height, RGB(44, 44, 44));
    cv_blit(0, WT_PLOT_Y);
}

static void ui_frame_draw(void)
{
    uint32_t pos = (uint32_t)display.osc[selected].position.current, frame = pos*7u>>16;
    char text[32], *p = ui_number(ui_text(text,"F"),(int32_t)frame+1);
    p = ui_number(ui_text(p,">"),(int32_t)(frame < 7u ? frame+2u : 8u));
    p = ui_number(ui_text(p,"  MIX "),(int32_t)((pos*7u&65535u)*100u/65536u));
    ui_text(p,"%");
    draw_text_box(0,40,240,&FONT_S,text,WT_LABEL,1);
}

static void ui_env_draw(uint32_t height)
{
    const wt_adsr_t *p = env_selected ? &edit.envelope : &edit.amp;
    const wt_env_t *live = env_selected ? &display.envelope : &display.amp;
    uint32_t times[4], total, i, sum = 0;
    int32_t x[5], y[5], marker = 0, low = (int32_t)height - 3, high = 3;
    times[0] = p->attack < 3u ? 3u : p->attack;
    times[1] = p->decay < 3u ? 3u : p->decay;
    times[2] = 500; times[3] = p->release < 3u ? 3u : p->release;
    total = times[0] + times[1] + times[2] + times[3];
    x[0] = 4; y[0] = y[4] = low; y[1] = high;
    y[2] = y[3] = low - (int32_t)(p->sustain * (uint32_t)(low - high) / 32768u);
    cv_begin(240, height, C_BLACK);
    cv_line(0, (int32_t)height - 2, 239, (int32_t)height - 2, RGB(40, 40, 40));
    for (i = 0; i < 4u; i++) {
        sum += times[i]; x[i + 1u] = 4 + (int32_t)(sum * 231u / total);
        cv_line(x[i], y[i], x[i + 1u], y[i + 1u], WT_ACC);
    }
    if (display.voices && live->stage) {
        uint32_t segment = live->stage == WT_ENV_ATTACK ? 0u : live->stage == WT_ENV_DECAY ? 1u :
                           live->stage == WT_ENV_SUSTAIN ? 2u : 3u;
        uint32_t spent = live->span > live->left ? live->span - live->left : 0u;
        marker = x[segment];
        if (segment == 2u) marker = (x[2] + x[3]) / 2;
        else if (live->span) marker += (int32_t)(spent * (uint32_t)(x[segment + 1u] - x[segment]) / live->span);
        cv_rect(marker - 1, low - (int32_t)(((int64_t)live->value * (low - high)) >> 24) - 1, 3, 3, C_WHITE);
    }
    cv_blit(0, page == PAGE_HOME ? WT_PLOT_Y : WT_ENV_Y);
}

static int32_t ui_lfo_shape(uint32_t shape, uint32_t phase)
{
    if (shape == WT_LFO_RANDOM) return display.lfo[lfo_selected]; /* no invented future random values */
    return wts_lfo_wave(shape, phase, 0);
}

static void ui_lfo_draw(uint32_t height)
{
    uint32_t i;
    int32_t prev = 0, middle = (int32_t)height / 2, half = middle - 4;
    cv_begin(240, height, C_BLACK);
    cv_line(4, middle, 236, middle, RGB(38, 38, 46));
    cv_line(0, (int32_t)height - 2, 239, (int32_t)height - 2, RGB(40, 40, 40));
    for (i = 0; i <= 232u; i++) {
        int32_t s = ui_lfo_shape(edit.lfo[lfo_selected].shape, i * (0xFFFFFFFFu / 232u));
        int32_t y = middle - s * half / 32768;
        if (i) cv_line((int32_t)i + 3, prev, (int32_t)i + 4, y, WT_ACC);
        prev = y;
    }
    cv_rect(4 + (int32_t)((display.phase[lfo_selected] >> 16) * 232u / 65535u), 2, 1, (int32_t)height - 4, C_WHITE);
    cv_blit(0, page == PAGE_HOME ? WT_PLOT_Y : WT_ENV_Y);
}

static int32_t ui_note_pitch(void)
{
    return wts_clamp((int32_t)display.note*256 + preferences.transpose*256 + preferences.fine,
                     0, WT_PITCH_MAX*256);
}

static void ui_note_label(char *text)
{
    static const char *const names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    uint32_t note = (uint32_t)ui_note_pitch()/256u;
    ui_number(ui_text(text,names[note%12u]), (int32_t)(note/12u)-1);
}

static int32_t ui_log64(uint64_t x)
{
    uint32_t shift = 0;
    while (x >> 31) { x >>= 1; shift++; }
    return (int32_t)(sp_log2_q8((uint32_t)x)+(shift<<8));
}

static int32_t ui_filter_lt(uint32_t hz16)
{
    uint32_t lo = 0, hi = WT_FILTER_STEPS, mid;
    if (hz16 < 320u)
        return WT_FILTER_LT[0]+((int32_t)sp_log2_q8(hz16)-(int32_t)sp_log2_q8(320u))*16;
    if (hz16 >= (uint32_t)WT_FILTER_HZ[hi]*16u) return WT_FILTER_LT[hi];
    while (hi-lo > 1u) {
        mid = (lo+hi)/2u;
        if (hz16 < (uint32_t)WT_FILTER_HZ[mid]*16u) hi = mid;
        else lo = mid;
    }
    return wt_lerp(WT_FILTER_LT[lo],WT_FILTER_LT[hi],
                   (hz16-(uint32_t)WT_FILTER_HZ[lo]*16u)*4096u/(uint32_t)(WT_FILTER_HZ[hi]-WT_FILTER_HZ[lo]));
}

/* FM Lab's warped-TPT response plot, using WT's cutoff/resonance grid rather than FM's tables. */
static int32_t ui_filter_db16(uint32_t hz16, uint32_t cutoff, uint32_t resonance, uint32_t type)
{
    uint32_t c = cutoff*128u, ci = c>>16, cn = ci < WT_FILTER_STEPS ? ci+1u : ci;
    uint32_t r = resonance*30u, ri = r>>16, rn = ri < 15u ? ri+1u : ri;
    int32_t u, exponent, logcut;
    uint32_t fraction, m, W, k, kk;
    int64_t d;
    uint64_t denominator, numerator;
    if (!type) return 0;
    logcut = wt_lerp(WT_FILTER_LT[ci],WT_FILTER_LT[cn],c&65535u);
    u = wts_clamp(2*(ui_filter_lt(hz16)-logcut),-14*4096,14*4096);
    exponent = u>>12; fraction = (uint32_t)u&4095u;
    m = (uint32_t)wt_lerp((int32_t)WT_FILTER_EXP2[fraction>>8],
                         (int32_t)WT_FILTER_EXP2[(fraction>>8)+1u],(fraction&255u)*256u);
    W = exponent >= 0 ? m<<exponent : m>>-exponent;
    k = (uint32_t)wt_lerp(WT_FILTER_K[ri],WT_FILTER_K[rn],r&65535u)>>14;
    kk = (uint32_t)(((uint64_t)k*k)>>16);
    d = (int64_t)65536-W;
    denominator = (uint64_t)(d*d)+(uint64_t)kk*W;
    numerator = type == WT_FILTER_LP ? 1ull<<32 : type == WT_FILTER_BP ? (uint64_t)kk*W : (uint64_t)W*W;
    if (!numerator) numerator = 1u;
    return ((ui_log64(numerator)-ui_log64(denominator))*12330)>>16;
}

static void ui_spectrum_draw(void)
{
    uint32_t col, k, span = SPANS[spectrum_span], f0 = 0;
    int32_t previous = 0;
    if (display.voices)
        f0 = (uint32_t)(((uint64_t)wt_pitch_increment(ui_note_pitch())*FS+0x80000000u)>>32);
    sp_analyze(captured_audio);
    cv_begin(240, SP_H, C_BLACK);
    cv_line(0, SP_H / 3, 239, SP_H / 3, RGB(22, 22, 22));
    cv_line(0, SP_H * 2 / 3, 239, SP_H * 2 / 3, RGB(22, 22, 22));
    cv_line(0, SP_H - 1, 239, SP_H - 1, RGB(48, 48, 48));
    {
        uint32_t grid = span <= 2000u ? 500u : span <= 5000u ? 1000u : span <= 10000u ? 2000u : 5000u;
        for (k = grid; k < span; k += grid)
            cv_rect((int32_t)(k * 240u / span), SP_H - 5, 1, 4, RGB(70, 70, 70));
    }
    if (f0*240u/span >= 4u)
        for (k = 1; f0 && k <= 16u && k*f0 < span; k++)
            cv_rect((int32_t)(k*f0*240u/span),0,1,k == 1u ? 9 : 5,
                    k == 1u ? RGB(150,118,210) : RGB(96,76,136));
    for (col = 0; col < 240u; col++) {
        uint32_t first = col * span / 240u * SP_N / FS;
        uint32_t end = (col + 1u) * span / 240u * SP_N / FS, h = 0;
        if (end <= first) end = first + 1u;
        if (end > SP_BINS) end = SP_BINS;
        for (k = first; k < end; k++) if (sp_bin[k] > h) h = sp_bin[k];
        ui_spectrum_peak[col] = (uint8_t)(h > ui_spectrum_peak[col] ? h :
            ui_spectrum_peak[col] > 2u ? ui_spectrum_peak[col] - 2u : 0u);
        if (h) {
            cv_rect((int32_t)col, SP_H - (int32_t)h, 1, (int32_t)h, WT_TRACE_FILL);
            cv_rect((int32_t)col, SP_H - (int32_t)h, 1, 1, C_WHITE);
        }
        if (ui_spectrum_peak[col] > h)
            cv_rect((int32_t)col, SP_H - (int32_t)ui_spectrum_peak[col], 1, 1, WT_ACC);
    }
    for (col = 0; col < 240u; col++) {
        int32_t db = ui_filter_db16((2u*col+1u)*span*8u/240u,edit.cutoff,edit.resonance,edit.filter_type);
        int32_t y = wts_clamp((int32_t)(SP_H/3u)-db*(int32_t)SP_H/SP_RANGE,0,SP_H-2);
        if (col) cv_line((int32_t)col-1,previous,(int32_t)col,y,WT_FILTER_CURVE);
        previous = y;
    }
    if (display.voices) {
        char text[16]; ui_note_label(text);
        cv_text(3,14,&FONT_S,text,RGB(120,100,160));
    }
    { char text[16]; ui_text(ui_number(ui_text(text, "0-"), (int32_t)(span / 1000u)), "K");
      cv_text(237 - text_w(&FONT_S, text), 14, &FONT_S, text, RGB(80, 80, 80)); }
    cv_blit(0, WT_PLOT_Y);
}

static void ui_scope_draw(uint32_t height)
{
    uint32_t i, col, width = SP_N, trigger = 0, period = 0;
    int32_t peak = 0, best = -1, prev = 0;
    if (display.voices) {
        period = 0xFFFFFFFFu / wt_pitch_increment(ui_note_pitch());
        if (period && period * 3u <= SP_N) width = period * 2u;
        else if (period && period * 2u <= SP_N) width = period;
        else period = 0;
    }
    for (i = 0; i < SP_N; i++) {
        int32_t n = captured_audio[i];
        if (n < 0) n = -n;
        if (n > peak) peak = n;
    }
    for (i = 1; i <= period; i++)
        if (captured_audio[i - 1u] < 0 && captured_audio[i] >= 0 &&
            captured_audio[i] - captured_audio[i - 1u] > best) {
            best = captured_audio[i] - captured_audio[i - 1u]; trigger = i;
        }
    scope_peak -= scope_peak / 8;
    if (peak > scope_peak) scope_peak = peak;
    if (scope_peak < 512) scope_peak = 512;
    int32_t middle = (int32_t)height/2, half = middle-16;
    cv_begin(240, height, C_BLACK);
    cv_line(0, middle, 239, middle, RGB(40, 40, 40));
    cv_line(0, middle-half, 239, middle-half, RGB(22, 22, 22));
    cv_line(0, middle+half, 239, middle+half, RGB(22, 22, 22));
    for (col = 0; col < 240u; col++) {
        uint32_t q = col * width * 256u / 240u, index = trigger + (q >> 8);
        int32_t sample, low, high, y;
        if (width >= 240u) {
            uint32_t end = trigger + (col + 1u) * width / 240u;
            low = high = captured_audio[index];
            for (i = index + 1u; i < end; i++) {
                if (captured_audio[i] < low) low = captured_audio[i];
                if (captured_audio[i] > high) high = captured_audio[i];
            }
            sample = captured_audio[end-1u];
        } else {
            low = high = sample = wt_lerp(captured_audio[index], captured_audio[index + 1u], (q & 255u) * 256u);
        }
        if (col && prev < low) low = prev;
        if (col && prev > high) high = prev;
        prev = sample; y = middle-sample*half/scope_peak;
        if (y < middle) cv_rect((int32_t)col, y, 1, middle-y, WT_TRACE_FILL);
        else if (y > middle) cv_rect((int32_t)col, middle+1, 1, y-middle, WT_TRACE_FILL);
        cv_line((int32_t)col, middle-high*half/scope_peak, (int32_t)col, middle-low*half/scope_peak, C_WHITE);
    }
    {
        char text[16], *p;
        uint32_t tenths = width*10000u/FS;
        if (display.voices) {
            ui_note_label(text); cv_text(3,4,&FONT_S,text,RGB(120,100,160));
        }
        p = ui_number(text,(int32_t)(tenths/10u)); *p++ = '.';
        *p++ = (char)('0'+tenths%10u); ui_text(p,"MS");
        cv_text(237-text_w(&FONT_S,text),4,&FONT_S,text,RGB(80,80,80));
    }
    cv_blit(0, WT_PLOT_Y);
}

static void ui_voice_draw(void)
{
    char text[64], prefix[WT_NAME_LEN], *p;
    const char *label;
    uint32_t y = page == PAGE_HOME ? 44u : 188u, i;
    uint16_t color = C_WHITE;
    if (saving) {
        int32_t x, name_x;
        p = workflow_slot(ui_text(text, "SAVE "), save_slot);
        p = ui_text(p, " ");
        name_x = text_w(&FONT_S, text);
        ui_text(p, save_name);
        x = (240 - text_w(&FONT_S, text)) / 2;
        cv_begin(240, 16, C_BLACK);
        cv_text(x, 0, &FONT_S, text, C_WHITE);
        for (i = 0; i < save_cursor; i++) prefix[i] = save_name[i];
        prefix[i] = 0;
        cv_rect(x + name_x + text_w(&FONT_S, prefix), 14, FONT_S.adv[glyph(&FONT_S, save_name[save_cursor])], 2, WT_ACC);
        cv_blit(0, y);
        return;
    }
    if (audio_fault) label = "AUDIO CONTROL ERROR";
    else if (message && fm1_ms - message_at < 1500u) label = message;
    else if (page == PAGE_HOME && ui_touched_field && fm1_ms-ui_touched_at < 1500u) {
        char value[48];
        p = ui_text(text, ui_value(ui_touched_field, value));
        ui_text(ui_text(p, " "), value);
        label = text; color = WT_ACC;
    }
    else if (save_pending) label = "SAVE WAITING FOR QUIET";
    else if (rec) label = counting_in() ? "REC: COUNT-IN" : "RECORDING";
    else {
        text[0] = ab ? 'B' : 'A'; text[1] = ' '; text[2] = 0;
        if (preset_index >= WT_FACTORY_COUNT) p = workflow_slot(text + 2, preset_index-WT_FACTORY_COUNT);
        else p = ui_number(ui_text(text + 2, "F"), (int32_t)preset_index+1);
        p = ui_text(p, workflow_modified() ? "* " : " ");
        p = ui_text(p, edit.name);
        if (settings_dirty) p = ui_text(p, " CFG*");
        if (arp_on) p = ui_text(p, " ARP");
        if (seq_run) p = ui_text(p, " PLAY");
        label = text;
        if (workflow_modified()) color = WT_ACC;
    }
    if (label != text) color = WT_ACC;
    draw_text_box(0, y, 240, &FONT_S, label, color, 1);
}

static void ui_steps_draw(void)
{
    uint8_t cells[SQ_MAX];
    uint32_t i, j, q, play = SQ_MAX, length = sq_len();
    wts_clear(cells, sizeof cells);
    for (i = 0; i < sq_n; i++) if (sq[i].step < length) {
        for (j = 1; j < sq[i].len && j < length; j++) {
            uint32_t at = (sq[i].step+j)&(length-1u);
            if (!cells[at]) cells[at] = 1;
        }
    }
    for (i = 0; i < sq_n; i++) if (sq[i].step < length) cells[sq[i].step] = 2;
    if (clock_at(&q) && (q>>8) >= seq_cin) play = ((q>>8)-seq_cin)&(length-1u);
    cv_begin(240, SP_H, C_BLACK);
    for (i = 0; i < length; i++) {
        int32_t x = 6+(int32_t)(i&15u)*14+(int32_t)((i&15u)>>2)*2;
        int32_t y = 1+(int32_t)(i>>4)*30;
        if (i == stc) cv_rect(x-1,y-1,14,16,C_WHITE);
        cv_rect(x,y,12,14,cells[i] == 2 ? WT_ACC : cells[i] ? RGB(80, 62, 125) :
                (i & 3u) ? RGB(34, 34, 34) : RGB(62, 62, 62));
        if (i == play) cv_rect(x,y+16,12,3,RGB(255, 200, 80));
    }
    cv_blit(0, page == PAGE_HOME ? WT_PLOT_Y : 59u);
}

static uint32_t ui_amount(uint32_t field, uint32_t width)
{
    uint32_t oscillator;
    const wt_osc_patch_t *o;
    const wt_lfo_patch_t *l = &edit.lfo[lfo_selected];
    int32_t value = 0, lo = 0, hi = 1;
    field = ui_osc_field(field,&oscillator); o = &edit.osc[oscillator];
    if (field >= U_FX && field < U_FX_END) { value = edit.fx[field-U_FX]; hi = 99; }
    else if (field >= U_PLAY && field < U_LENGTH) {
        value = play_controls[field-U_PLAY]; hi = PLAY_MAX[field-U_PLAY];
    } else if (field >= U_AMP_A && field <= U_ENV_R) {
        const wt_adsr_t *e = field >= U_ENV_A ? &edit.envelope : &edit.amp;
        uint32_t index = field >= U_ENV_A ? field-U_ENV_A : field-U_AMP_A;
        value = index == 0 ? e->attack : index == 1 ? e->decay : index == 2 ? e->sustain : e->release;
        hi = index == 2 ? WT_GAIN_MAX : 5000;
    } else {
        switch (field) {
        case U_TABLE: value = o->table; hi = WT_TABLE_COUNT-1; break;
        case U_POSITION: value = o->position; hi = WT_POSITION_MAX; break;
        case U_LEVEL: value = o->gain; hi = WT_GAIN_MAX; break;
        case U_SEMI: value = o->semitone; lo = -24; hi = 24; break;
        case U_FINE: value = o->fine; lo = -256; hi = 256; break;
        case U_DETUNE: value = o->semitone*256+o->fine; lo = -25*256; hi = 25*256; break;
        case U_POSENV: case U_POSENV1: case U_POSENV2:
            value = edit.osc[field == U_POSENV ? selected : field == U_POSENV1 ? 0u : 1u].position_env;
            lo = -(int32_t)WT_POSITION_MAX; hi = WT_POSITION_MAX; break;
        case U_RATE: value = l->rate; hi = 20000; break;
        case U_SHAPE: value = l->shape; hi = WT_LFO_SHAPES-1; break;
        case U_DEPTH: value = l->depth; hi = WT_GAIN_MAX; break;
        case U_DEST: value = l->destination; hi = WT_MOD_DESTS-1; break;
        case U_SYNC: value = l->key_sync; break;
        case U_CUTOFF: value = edit.cutoff; hi = WT_CUTOFF_MAX; break;
        case U_RESONANCE: value = edit.resonance; hi = WT_GAIN_MAX; break;
        case U_TYPE: value = edit.filter_type; hi = WT_FILTER_HP; break;
        case U_TRACK: value = edit.keytrack; hi = WT_GAIN_MAX; break;
        case U_FDEPTH: value = edit.filter_env; lo = -(int32_t)WT_POSITION_MAX; hi = WT_POSITION_MAX; break;
        case U_VELOCITY: value = edit.velocity; hi = WT_GAIN_MAX; break;
        case U_MIDI: value = (int32_t)midi_channel; hi = 16; break;
        case U_MODE: value = (int32_t)mono; break;
        case U_TUNE: value = preferences.transpose; lo = -24; hi = 24; break;
        case U_TUNE_FINE: value = preferences.fine; lo = -256; hi = 256; break;
        case U_BITS: value = 3-(int32_t)preferences.bits; hi = 3; break;
        case U_SRATE: value = WT_SAMPLE_RATE_COUNT-1u-preferences.sample_rate; hi = WT_SAMPLE_RATE_COUNT-1u; break;
        case U_OUTPUT: value = preferences.output_mono; break;
        case U_PCOUT: value = preferences.pc_send; break;
        case U_SPAN: value = (int32_t)spectrum_span; hi = 4; break;
        default: return 0;
        }
    }
    return (uint32_t)(value-lo) * width / (uint32_t)(hi-lo);
}

static void ui_row_draw(uint32_t row, uint32_t y, int compact)
{
    char value[48];
    uint32_t field = ui_field(row), vw;
    const char *label = ui_value(field, value);
    vw = (uint32_t)text_w(&FONT_S, value);
    if (vw < 84u) vw = 84u;
    cv_begin(240, compact ? 16u : 30u, C_BLACK);
    cv_text(8, 0, &FONT_S, label, WT_LABEL);
    cv_rect(234-(int32_t)vw,0,(int32_t)vw,FONT_S.h,C_BLACK);
    cv_text(234-text_w(&FONT_S,value),0,&FONT_S,value,C_WHITE);
    if (!compact && field != U_NONE) {
        cv_rect(6, 24, 228, 4, RGB(40, 40, 40));
        cv_rect(6, 24, (int32_t)ui_amount(field, 228), 4, WT_ACC);
    }
    cv_blit(0, y);
}

static void ui_cells_draw(void)
{
    char value[48], *r, *w;
    uint32_t i;
    cv_begin(240, 32, C_BLACK);
    for (i = 0; i < 4u; i++) {
        uint32_t field = ui_field(i);
        const char *label = ui_legend(field);
        int32_t x = (int32_t)i*60+3, vw, lw;
        ui_value(field, value);
        for (r = w = value; *r; r++) if (*r != ' ') *w++ = *r;
        *w = 0;
        if ((field >= U_AMP_A && field <= U_ENV_R) && w-value >= 2 && w[-2] == 'M') w[-2] = 0;
        vw = text_w(&FONT_S, value); lw = text_w(&FONT_S, label);
        if (field == U_NONE) continue;
        if (lw + vw + 4 <= 54) cv_text(x, 4, &FONT_S, label, WT_LABEL);
        cv_text(x + 54-vw, 4, &FONT_S, value, C_WHITE);
        cv_rect(x, 26, 54, 4, RGB(40, 40, 40));
        cv_rect(x, 26, (int32_t)ui_amount(field, 54), 4, WT_ACC);
    }
    cv_blit(0, WT_CELLS_Y);
}

static void ui_head_draw(void)
{
    char text[64], *p;
    if (page == PAGE_HOME) {
        if (focus == FOCUS_EDIT) {
            p = ui_number(ui_text(text,"EDIT WT "),(int32_t)selected+1);
        }
        else if (focus == FOCUS_ENV) p = ui_text(text, env_selected ? "ENV FILTER" : "ENV AMP");
        else if (focus == FOCUS_LFO) p = ui_number(ui_text(text, "LFO "), (int32_t)lfo_selected+1);
        else if (focus == FOCUS_FX) p = ui_text(text,"FX MIX");
        else if (focus == FOCUS_ARP) {
            p = ui_number(ui_text(text, "ARP "), (int32_t)bank+1);
            p = ui_text(p, "/2");
        } else if (focus == FOCUS_SEQ) p = ui_text(text, "SEQ");
        else p = ui_text(text, home_view == 1u ? "FILTER" : home_view == 2u ? "SCOPE" : "WT LAB");
        /* FONT_L's 32px box otherwise clears the first six rows of the patch line at y44. */
        cv_begin(240, 26, C_BLACK);
        cv_text((240-text_w(&FONT_L,text))/2, 0, &FONT_L, text, WT_ACC);
        cv_blit(0, 18);
    } else {
        p = ui_text(text, page == PAGE_FX ? EFFECT_NAMES[fx_selected] : page == PAGE_ENV ?
                    (bank == 2u ? "ENV ROUTING" : env_selected ? "ENVELOPE FILTER" : "ENVELOPE AMP") :
                    page == PAGE_STEP ? "STEPS" : UI_PAGE[page]);
        if (page == PAGE_EDIT || page == PAGE_LFO) {
            p = ui_number(ui_text(p, " "), (int32_t)(page == PAGE_EDIT ? selected : lfo_selected)+1);
        }
        if (page != PAGE_STEP) {
            p = ui_number(ui_text(p, "  "), (int32_t)(page == PAGE_LFO ? bank%2u : bank)+1);
            ui_number(ui_text(p, "/"), (int32_t)(page == PAGE_LFO ? 2u : ui_banks()));
        }
        draw_text_box(0, 24, 240, &FONT_S, text, WT_ACC, 1);
    }
}

static void ui_corners_draw(void)
{
    uint32_t i, level = batt_raw >= 591 ? 3u : batt_raw >= 561 ? 2u : batt_raw >= 531 ? 1u : 0u;
    int charging = fm1_usb_host();
    uint16_t color = charging ? WT_OFF : !level ? RGB(255, 90, 90) : level == 1u ? RGB(235, 170, 60) : WT_OFF;
    if (charging) level = 1u+(fm1_ms/600u)%3u;
    cv_begin(240, 16, C_BLACK);
    cv_rect(6, 3, 17, 1, color); cv_rect(6, 11, 17, 1, color);
    cv_rect(6, 3, 1, 9, color); cv_rect(22, 3, 1, 9, color); cv_rect(23, 5, 2, 5, color);
    for (i = 0; i < level; i++) cv_rect(8+(int32_t)i*5, 5, 3, 5, color);
    if (page == PAGE_HOME)
        for (i = 0; i < 3u; i++) cv_rect(208+(int32_t)i*10, 5, 6, 6, i == home_view ? WT_ACC : RGB(56, 56, 64));
    if (page == PAGE_GLO) {
        char version[48];
        ui_value(U_BUILD, version);
        cv_text(234-text_w(&FONT_S, version), 0, &FONT_S, version, WT_OFF);
    }
    cv_blit(0, 0);
}

static void ui_diagnostics_draw(void)
{
    char text[64];
#ifndef DINGHY_HOST
    char *p;
#endif
    uint32_t i;
    cv_begin(240, 72, C_BLACK);
#ifdef DINGHY_HOST
    cv_text(4, 0, &FONT_S, "HOST / NO DEVICE TIMING", WT_LABEL);
    ui_number(ui_text(text, "VOICES "), (int32_t)display.voices);
    cv_text(4, 18, &FONT_S, text, C_WHITE);
    ui_text(text, "LATE: NOT DEVICE");
#else
    p = ui_number(ui_text(text,"PK C0 "), (int32_t)display.profile.render);
    p = ui_number(ui_text(p,"% WAIT "), (int32_t)display.profile.wait); ui_text(p,"%");
    cv_text(4, 0, &FONT_S, text, WT_LABEL);
    p = ui_number(ui_text(text,"FX "), (int32_t)display.profile.fx);
    p = ui_number(ui_text(p,"% OTHER "), (int32_t)display.profile.other); ui_text(p,"%");
    cv_text(4, 18, &FONT_S, text, C_WHITE);
    p = ui_number(ui_text(text,"O "), (int32_t)display.profile.osc);
    p = ui_number(ui_text(p,"% V "), (int32_t)display.profile.voice);
    p = ui_number(ui_text(p,"% C "), (int32_t)display.profile.control); ui_text(p,"%");
#endif
    cv_text(4, 36, &FONT_S, text, C_WHITE);
#ifndef DINGHY_HOST
    ui_text(ui_number(ui_text(text,"LATE "), (int32_t)display.profile.late)," / SEL RESET");
    cv_text(4, 54, &FONT_S, text, WT_LABEL);
#else
    cv_text(4, 54, &FONT_S, "SEL: RESET PEAK / LATE", WT_LABEL);
#endif
    cv_blit(0, WT_ENV_Y);
    for (i = 0; i < 4u; i++) ui_row_draw(i, 120u+i*16u, 1);
}

static void ui_status_draw(void)
{
    char text[64], *p = text;
    if (page == PAGE_GLO) {
        p = ui_number(ui_text(p, octave < 0 ? "OCT" : "OCT+"), octave);
        p = ui_number(ui_text(p, " VOX"), (int32_t)display.voices);
#ifdef DINGHY_HOST
        ui_text(p, " / NOT DEVICE");
#else
        p = ui_number(ui_text(p, " CPU"), (int32_t)cpu_percent);
        p = ui_number(ui_text(p, "% PK"), (int32_t)cpu_peak); ui_text(p, "%");
#endif
    } else if (page == PAGE_STEP) {
        p = ui_number(ui_text(p, "STEP "), (int32_t)stc+1);
        p = ui_number(ui_text(p, "/"), (int32_t)sq_len());
        p = ui_number(ui_text(p, " NOTES "), sq_n);
        ui_text(ui_number(ui_text(p, " "), (int32_t)bpm()), " BPM");
    } else *p = 0;
    draw_text_box(0, 224, 240, &FONT_S, text, WT_OFF, 1);
}

static void ui_draw(void)
{
    if (mi_editor) { mi_draw(); return; }
    uint32_t i, fresh = view_mailbox == 2u;
    uint32_t controls_only = page == PAGE_HOME;
    uint32_t plot_bank = controls_only ? 0u : bank;
    uint32_t signature = page | (controls_only ? 0u : focus)<<4 | home_view<<8 | plot_bank<<12 | saving<<16;
    if (fresh) { RING_PUBLISH(); display = captured; }
    if (signature != ui_draw_signature) {
        lcd_fill(0, 0, 240, 240, C_BLACK);
        wts_clear(ui_spectrum_peak, sizeof ui_spectrum_peak);
        ui_touched_field = U_NONE;
        ui_draw_signature = signature;
    }
    ui_head_draw();
    ui_corners_draw();
    if (page == PAGE_HOME) {
        if (home_view == 1u) { if (fresh) ui_spectrum_draw(); }
        else if (home_view == 2u) { if (fresh) ui_scope_draw(SP_H); }
        else ui_wave_draw(SP_H);
    } else if (page == PAGE_ENV && bank < 2u) {
        ui_env_draw(WT_ENV_H); ui_cells_draw();
    } else if (page == PAGE_LFO) {
        ui_lfo_draw(WT_ENV_H); ui_cells_draw();
    } else if (page == PAGE_GLO && bank == 3u) ui_diagnostics_draw();
    else if (page == PAGE_STEP) ui_steps_draw();
    else for (i = 0; i < 4u; i++) ui_row_draw(i, 56u+i*32u, 0);
    if (page == PAGE_EDIT) ui_frame_draw();
    ui_voice_draw();
    ui_legend_draw();
    ui_status_draw();
    if (fresh) { RING_PUBLISH(); view_mailbox = 0; }
    if (!view_mailbox) { RING_PUBLISH(); view_mailbox = 1; }
}

static int ui_osc_context(void)
{
    return page == PAGE_EDIT || (page == PAGE_ENV && bank == 2u) ||
           (page == PAGE_HOME && focus == FOCUS_EDIT);
}

static uint32_t ui_banks(void)
{
    return page == PAGE_LFO || page == PAGE_GLO || page == PAGE_FX ? 4u : page == PAGE_ENV ? 3u : 2u;
}

static uint32_t ui_wrap(uint32_t value, int32_t delta, uint32_t count)
{
    return (uint32_t)(((int32_t)value + delta % (int32_t)count + (int32_t)count) % (int32_t)count);
}

static void ui_step(int32_t delta)
{
    if (page == PAGE_STEP) { stc = ui_wrap(stc, delta, sq_len()); audition_cursor(); return; }
    if (page == PAGE_HOME && (!focus || focus == FOCUS_EDIT)) {
        if (home_view != 1u) bank = ui_wrap(bank,delta,2);
        return;
    }
    if (page == PAGE_HOME && focus) {
        if (focus == FOCUS_ENV) env_selected = ui_wrap(env_selected, delta, 2);
        if (focus == FOCUS_LFO) lfo_selected = ui_wrap(lfo_selected, delta, 2);
        if (focus == FOCUS_ARP) bank = arp_selected = ui_wrap(bank, delta, 2);
    } else if (!(page == PAGE_HOME && home_view == 1u)) {
        bank = ui_wrap(bank, delta, ui_banks());
        if (page == PAGE_ENV) env_selected = bank != 0u;
        if (page == PAGE_LFO) lfo_selected = bank / 2u;
        if (page == PAGE_FX) fx_selected = bank;
    }
}

static void ui_open(uint32_t target)
{
    if (sel_hold.active) sel_used = 1;
    focus = FOCUS_OFF;
    if (page == target) ui_step(1);
    else {
        page = target; bank = target == PAGE_ENV ? env_selected : target == PAGE_LFO ? lfo_selected * 2u :
                              target == PAGE_FX ? fx_selected : 0;
    }
}

static void ui_home(void)
{
    if (mi_editor) { mi_editor = mi_learn = 0; mi_drawn = ui_draw_signature = ~0u; }
    workflow_cancel();
    if (sel_hold.active) sel_used = 1;
    if (page != PAGE_HOME || focus) { page = PAGE_HOME; bank = 0; focus = FOCUS_OFF; }
    else home_view = (home_view + 1u) % 3u;
}

static void ui_ab_copy(void)
{
    other = edit; say(ab ? "B COPIED TO A" : "A COPIED TO B");
}

static void ui_ab_swap(void)
{
    wt_patch_t temp = edit;
    edit = other; other = temp; ab ^= 1u;
    patch_publish(); say(ab ? "COMPARE B" : "COMPARE A");
}

static void ui_buttons(uint32_t buttons, uint32_t released)
{
    static const uint8_t ids[] = {BTN_EDIT, BTN_ENV, BTN_LFO, BTN_FX};
    static const uint8_t pages[] = {PAGE_EDIT, PAGE_ENV, PAGE_LFO, PAGE_FX};
    uint32_t i;
    if (buttons & (1u << BTN_SEL) && mi_editor) {
        mi_toggle_learn();
    } else if (buttons & (1u << BTN_SEL)) {
        sel_hold.at = fm1_ms; sel_hold.active = 1; sel_used = 0;
        sel_context = (uint32_t)ui_osc_context(); sel_target = selected;
    }
    if (buttons & (1u << BTN_HOME)) ui_home();
    if (buttons & (1u << BTN_GLO)) {
        if (mi_editor) mi_slot_turn(1);
        else {
            ui_open(PAGE_GLO); midi_hold.at = fm1_ms; midi_hold.active = 1; midi_hold.fired = 0;
        }
    }
    if (midi_hold.active && !midi_hold.fired && fm1_ms-midi_hold.at >= LONG_MS &&
        ((fm1_in.buttons | released) & (1u << BTN_GLO))) {
        midi_hold.fired = 1; mi_open_editor(); workflow_cancel();
    }
    if (released & (1u << BTN_GLO)) midi_hold.active = 0;
    if (mi_editor && (buttons & ((1u << BTN_EDIT) | (1u << BTN_ENV) | (1u << BTN_LFO) |
        (1u << BTN_FX) | (1u << BTN_ARP) | (1u << BTN_SEQ)))) {
        mi_editor = mi_learn = 0; ui_draw_signature = ~0u;
    }
    if (buttons & (1u << BTN_OCT_DOWN)) octave = wts_clamp(octave - 1, -2, 2);
    if (buttons & (1u << BTN_OCT_UP)) octave = wts_clamp(octave + 1, -2, 2);
    for (i = 0; i < 4u; i++) {
        uint32_t mask = 1u << ids[i], elapsed;
        if (buttons & mask) { hold[i].at = fm1_ms; hold[i].active = 1; hold[i].fired = 0; }
        elapsed = fm1_ms - hold[i].at;
        if (hold[i].active && !hold[i].fired && elapsed >= LONG_MS &&
            ((fm1_in.buttons | released) & mask)) {
            ui_open(pages[i]); hold[i].fired = 1;
        }
        if ((released & mask) && hold[i].active) {
            if (!hold[i].fired) {
                if (sel_hold.active) sel_used = 1;
                if (page == pages[i]) ui_step(1);
                else if (page == PAGE_HOME && focus == i + 1u) ui_step(1);
                else {
                    if (page != PAGE_HOME || focus || i) bank = 0;
                    if (i == 1u) env_selected = 0;
                    page = PAGE_HOME; focus = i + 1u;
                }
            }
            hold[i].active = 0;
        }
    }
    if (buttons & (1u << BTN_SAVE)) {
        if (mi_editor) {
            mi_clear();
        } else { if (sel_hold.active) sel_used = 1; workflow_save(); }
    }
    if (buttons & (1u << BTN_ARP)) {
        if (sel_hold.active) sel_used = 1;
        if (page == PAGE_HOME && focus == FOCUS_ARP) {
            arp_on ^= 1u; chord_n = 0;
            say(arp_on ? (play_controls[UI_LATCH] ? "ARP ON, LATCHED" : "ARP ON") : "ARP OFF");
        } else { page = PAGE_HOME; bank = arp_selected; focus = FOCUS_ARP; }
    }
    if (buttons & (1u << BTN_SEQ)) { seq_hold.active = 1; seq_hold.fired = 0; seq_hold.at = fm1_ms; }
    if (seq_hold.active && !seq_hold.fired && fm1_ms-seq_hold.at >= LONG_MS) {
        seq_hold.fired = 1; page = PAGE_STEP; bank = 0; focus = FOCUS_OFF; stc %= sq_len();
        if (sel_hold.active) sel_used = 1;
    }
    if ((released & (1u << BTN_SEQ)) && seq_hold.active) {
        if (!seq_hold.fired) { page = PAGE_HOME; bank = 0; focus = FOCUS_SEQ; }
        seq_hold.active = 0;
    }
    if (buttons & (1u << BTN_REC)) { rec_hold.active = 1; rec_hold.fired = 0; rec_hold.at = fm1_ms; }
    if (rec_hold.active && !rec_hold.fired && fm1_ms-rec_hold.at >= LONG_MS) {
        rec_hold.fired = 1; performance_clear(); say("SEQ CLEARED");
    }
    if ((released & (1u << BTN_REC)) && rec_hold.active) {
        if (!rec_hold.fired) {
            if (rec) { rec_stop(); say("REC DONE"); }
            else if (seq_run) { wts_clear(rkeys, sizeof rkeys); rec = 1; say("REC: OVERDUB"); }
            else {
                seq_start(16); wts_clear(rkeys, sizeof rkeys); rec = 2;
                rec_from = 16; rec_to = 16+sq_len(); say("REC: COUNT-IN");
            }
        }
        rec_hold.active = 0;
    }
    if (buttons & (1u << BTN_PLAY)) {
        if (seq_run) { rec_stop(); seq_run = 0; say("STOP"); }
        else if (!sq_n) say("NOTHING RECORDED");
        else { seq_start(0); say("PLAY"); }
    }
}

static void ui_sel_release(uint32_t released)
{
    if ((released & (1u << BTN_SEL)) && sel_hold.active) {
        if (!sel_used && !sel_context && page == PAGE_GLO && bank == 3u) {
            RING_PUBLISH(); cpu_reset_generation++;
            say("AUDIO METER RESET");
        }
        if (!sel_used && sel_context && ui_osc_context() && selected == sel_target) {
            if (fm1_ms - sel_hold.at >= LONG_MS) {
                uint32_t only = 3u ^ (1u << selected);
                oscillator_mutes = oscillator_mutes == only ? 0u : only;
                say(oscillator_mutes ? (selected ? "OSC 2 SOLO" : "OSC 1 SOLO") : "BOTH OSC ON");
            } else {
                oscillator_mutes ^= 1u << selected;
                say(oscillator_mutes >> selected & 1u ? "OSCILLATOR OFF" : "OSCILLATOR ON");
            }
            patch_publish();
        }
        sel_hold.active = 0;
    }
}

static int32_t ui_encoder(uint32_t encoder)
{
    int32_t delta = fm1_enc_take(encoder);
    if (delta && sel_hold.active) sel_used = 1;
    return delta;
}

static void ui_keys(uint32_t keys)
{
    uint32_t down = keys & ~previous_keys, up = previous_keys & ~keys, i;
    previous_keys = keys;
    for (i = 0; i < 27u; i++) {
        if (down >> i & 1u) {
            if (ui_osc_context() && (i == 1u || i == 3u)) {
                selected = i == 3u;
                if (sel_hold.active) sel_used = 1;
            } else if (ui_osc_context() && (i == 20u || i == 22u)) {
                mono = i == 20u; ev_push(EV_MONO, mono, 0);
                if (sel_hold.active) sel_used = 1;
            } else if (!mi_editor && i == 25u && page != PAGE_STEP &&
                       (page != PAGE_HOME || (focus && focus <= FOCUS_FX)) && !saving) {
                ab_hold.at = fm1_ms; ab_hold.active = 1; ab_hold.fired = 0;
            } else {
                played[i] = (uint8_t)(53 + (int32_t)i + 12 * octave);
                if (page == PAGE_STEP && !rec) st_toggle(played[i]);
                performance_down(i, played[i], 100, page == PAGE_STEP && !rec);
                if (input_owner[i]) {
                    sounding |= 1u << i; played_channel[i] = (uint8_t)(midi_channel ? midi_channel - 1u : 0u);
                    midi_send(0x90u | played_channel[i], played[i], 100);
                }
            }
        }
        if ((up & sounding) >> i & 1u) {
            performance_up(i); midi_send(0x80u | played_channel[i], played[i], 0);
            sounding &= ~(1u << i);
        }
        fm1_led_key(KEY_LED(i), (keys >> i) & 1u);
    }
    if (ab_hold.active && !ab_hold.fired && fm1_ms - ab_hold.at >= LONG_MS) {
        ui_ab_copy(); ab_hold.fired = 1;
    }
    if ((up >> 25 & 1u) && ab_hold.active) {
        if (!ab_hold.fired) ui_ab_swap();
        ab_hold.active = 0;
    }
}

static void ui_poll(void)
{
#ifdef WT_CPU1
    static uint32_t failure_seen;
    if (fm1_cpu1_gave_up && failure_seen != fm1_cpu1_gave_up) {
        failure_seen = fm1_cpu1_gave_up;
        say("CPU1 FAILED / CPU0 FALLBACK");
    }
#endif
    uint32_t released, buttons = fm1_input_edges(&released), i;
    int32_t delta;
    uint8_t st, d1, d2;
    ui_buttons(buttons, released);
    rec_poll();
    delta = ui_encoder(ENC_SELECT);
    if (delta) {
        if (mi_editor) mi_slot_turn(delta);
        else if (saving) save_cursor = ui_wrap(save_cursor, delta, WT_NAME_LEN-1u);
        else ui_step(delta);
    }
    delta = ui_encoder(ENC_PRESETS);
    if (delta) { if (mi_editor) mi_group_turn(delta); else workflow_presets(delta); }
    delta = ui_encoder(ENC_ALGORITHM);
    if (delta) {
        if (mi_editor) mi_target_turn(delta);
        else if (saving) workflow_name_turn(delta);
        else ui_edit(U_TABLE, delta);
    }
    for (i = 0; i < 4u; i++) {
        delta = ui_encoder(ENC_KNOB1 + i);
        if (delta) {
            if (mi_editor) mi_knob(i,delta);
            else if (saving) {
                if (i == 0u) save_cursor = ui_wrap(save_cursor, delta, WT_NAME_LEN-1u);
                if (i == 1u) workflow_name_turn(delta);
                if (i == 2u) save_slot = ui_wrap(save_slot, delta, WT_USER_COUNT);
            } else ui_edit(ui_field(i), delta);
        }
    }
    ui_keys(fm1_in.notes);
    if (ui_osc_context()) {
        fm1_led_key(KEY_LED(1),selected == 0u);
        fm1_led_key(KEY_LED(3),selected == 1u);
        fm1_led_key(KEY_LED(20),mono != 0u);
        fm1_led_key(KEY_LED(22),mono == 0u);
    }
    ui_sel_release(released);
    while (midi_get(&st, &d1, &d2)) {
        uint32_t kind = st & 0xF0u;
        if (midi_channel && (st & 15u) + 1u != midi_channel) continue;
        int action;
        if (d1 > 127u || (kind != 0xC0u && kind != 0xD0u && d2 > 127u)) { say("INVALID MIDI"); mi_invalid(); continue; }
        action = mi_receive(kind,d1,d2);
        if (kind == 0x90u && d2) performance_down(27u+d1, d1, d2, 0);
        else if (kind == 0x80u || kind == 0x90u) performance_up(27u+d1);
        else if (action == MI_PEDAL) ev_push(EV_SUSTAIN,d2 >= 64u,0);
        else if (action == MI_RESET) ev_push(EV_SUSTAIN,0,0);
        else if (action == MI_SOUND_OFF || action == MI_NOTES_OFF) {
            performance_stop(); wts_clear(input_owner, sizeof input_owner);
            ev_push(action == MI_SOUND_OFF ? EV_SILENCE : EV_RELEASE_ALL,0,0);
        } else if (action == MI_LEARNED) say("CC LEARNED");
        else if (action == MI_RESERVED) {
            say("RESERVED CC"); mi_learn = 0; mi_drawn = ~0u;
        } else if (kind == 0xB0u && d1 == 0u) pc_bank = d2;
        else if (kind == 0xC0u) workflow_program(d1);
    }
    if (event_overflow) performance_stop();
    if (audition_count && (page != PAGE_STEP || seq_run || fm1_ms-audition_at >= 250u)) audition_stop();
    pl_sync(); workflow_poll();
    {
        uint32_t phase = (fm1_ms % 900u) < 600u;
        fm1_led_key(BTN_OCT_DOWN, octave < 0 && (octave > -2 || phase));
        fm1_led_key(BTN_OCT_UP, octave > 0 && (octave < 2 || phase));
        fm1_led_key_glow(BTN_OCT_DOWN, octave == -2);
        fm1_led_key_glow(BTN_OCT_UP, octave == 2);
    }
    fm1_led_key(BTN_HOME, page == PAGE_HOME);
    fm1_led_key(BTN_EDIT, page == PAGE_EDIT || focus == FOCUS_EDIT);
    fm1_led_key(BTN_ENV, page == PAGE_ENV || focus == FOCUS_ENV);
    fm1_led_key(BTN_LFO, page == PAGE_LFO || focus == FOCUS_LFO);
    fm1_led_key(BTN_FX, page == PAGE_FX || focus == FOCUS_FX);
    fm1_led_key(BTN_GLO, page == PAGE_GLO);
    fm1_led_key(BTN_SEL, mi_editor ? mi_learn : oscillator_mutes != 0u);
    fm1_led_key(BTN_ARP, arp_on);
    fm1_led_key(BTN_SEQ, (page == PAGE_HOME && focus == FOCUS_SEQ) || page == PAGE_STEP);
    {
        uint32_t beat;
        fm1_led_key(BTN_PLAY, clock_at(&beat) && (beat & 0x3FFu) < 0x100u);
        fm1_led_key_glow(BTN_PLAY, sq_n != 0u);
    }
    fm1_led_key(BTN_REC, rec != 0u);
}

static void ui_init(void)
{
    uint32_t i;
    const char *boot_message;
    ready = 0;
    prepared_valid = control_cache_hits = control_cache_misses = 0;
    mi_init();
    cpu_percent = cpu_peak = cpu_late = cpu_reset_generation = cpu_reset_seen = 0;
    wts_clear(&cpu_profile_peak,sizeof cpu_profile_peak);
    wt_synth_init(&synth); wt_patch_init(&edit); other = edit;
    wts_clear(&preferences, sizeof preferences); wts_clear(&audio_preferences, sizeof audio_preferences);
    wt_sample_rate_reset(0);
    preferences.pc_send = 1;
    wts_clear(&xs, sizeof xs); xs.ph_a = PH_A[PH_LO >> 8];
    audio_panic(); performance_init(); flash_hold = quiet_blocks = 0;
    wts_clear(&display, sizeof display); wts_clear(&captured, sizeof captured);
    wts_clear(hold, sizeof hold); wts_clear(&sel_hold, sizeof sel_hold); wts_clear(&ab_hold, sizeof ab_hold);
    wts_clear(&seq_hold, sizeof seq_hold); wts_clear(&rec_hold, sizeof rec_hold);
    wts_clear(&midi_hold,sizeof midi_hold);
    wts_clear(played, sizeof played); wts_clear(played_channel, sizeof played_channel);
    wts_clear(sp_ring, sizeof sp_ring); wts_clear(captured_audio, sizeof captured_audio);
    page = bank = selected = mono = midi_channel = focus = home_view = env_selected = lfo_selected = fx_selected = arp_selected = 0;
    ui_draw_signature = ~0u; ui_touched_field = ui_touched_at = 0;
    ab = sel_used = sel_context = sel_target = previous_keys = sounding = oscillator_mutes = 0; octave = 0;
    event_read = event_write = event_overflow = patch_index = patch_generation = seen_generation = audio_fault = lit = 0;
    view_mailbox = 0; spectrum_span = 2; scope_peak = 512;
    for (i = 0; i < 2u; i++) {
        wt_controls_t c = {0, 0, 0, WT_GAIN_MAX};
        c.increment = wt_pitch_increment(60 * 256);
        if (wt_osc_start(&display.osc[i], &c) != 0) audio_fault = 1;
    }
    boot_message = workflow_init();
    if (mono) ev_push(EV_MONO, mono, 0);
    sp_init(); sp_want = 1; patch_publish();
    palette_set(0); lcd_fill(0, 0, 240, 240, C_BLACK);
    say(boot_message);
    RING_PUBLISH(); ready = 1;
    ui_draw();
}

static void app_main(void)
{
    uint32_t draw_at = 0;
    ui_init();
    for (;;) {
        platform_poll(); ui_poll();
        if (fm1_ms - draw_at >= 40u) { ui_draw(); draw_at = fm1_ms; }
    }
}
