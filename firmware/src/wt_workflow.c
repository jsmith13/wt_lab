/* SPDX-License-Identifier: GPL-3.0-only */
/* Main-owned preset/save UI and deferred storage jobs. Audio acknowledges a quiet pause first. */
static uint32_t preset_index, save_slot, saving, save_cursor, save_pending, save_target, pc_bank;
static uint32_t settings_dirty, settings_failed, settings_at;
static char save_name[WT_NAME_LEN];
static wt_patch_t pending_patch;
static wt_settings_t settings_seen;
static const char NAME_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!?";
static uint32_t ui_wrap(uint32_t value, int32_t delta, uint32_t count);

static char *workflow_slot(char *s, uint32_t slot)
{
    s = ui_text(s, "U"); *s++ = (char)('1'+slot/32u); *s++ = '-';
    *s++ = (char)('0'+(slot%32u+1u)/10u); *s++ = (char)('0'+(slot%32u+1u)%10u); *s = 0;
    return s;
}

static int workflow_modified(void)
{
    wt_patch_t p;
    if (wt_voice_get(&p, preset_index)) return 1;
    return memcmp(&p, &edit, sizeof p) != 0;
}

static void workflow_pc_send(void)
{
    uint32_t channel = midi_channel ? midi_channel-1u : 0u;
    uint32_t user = preset_index >= WT_FACTORY_COUNT;
    uint32_t index = user ? preset_index-WT_FACTORY_COUNT : preset_index;
    if (!preferences.pc_send) return;
    midi_send(0xB0u|channel, 0, user ? 1u+index/32u : 0u);
    midi_send(0xC0u|channel, user ? index%32u : index, 0);
}

static int workflow_load(uint32_t index, uint32_t send)
{
    wt_patch_t p;
    if (wt_voice_get(&p, index)) { say("PATCH DATA REJECTED"); return ST_INVALID; }
    edit = other = p; ab = oscillator_mutes = saving = 0; preset_index = index;
    patch_publish();
    if (send) workflow_pc_send();
    return 0;
}

static uint32_t workflow_jump(uint32_t at, int32_t delta, uint32_t voices)
{
    uint32_t group = voices ? (at < WT_FACTORY_COUNT ? 0u : 1u+(at-WT_FACTORY_COUNT)/32u) : at/32u;
    uint32_t first = voices ? (group ? WT_FACTORY_COUNT+(group-1u)*32u : 0u) : group*32u;
    if (delta < 0 && at != first) delta++;
    group = ui_wrap(group, delta, voices ? BANK_N+1u : BANK_N);
    return voices ? (group ? WT_FACTORY_COUNT+(group-1u)*32u : 0u) : group*32u;
}

static void workflow_presets(int32_t delta)
{
    uint32_t fine = (fm1_in.buttons >> BTN_SEL)&1u;
    if (saving) save_slot = fine ? workflow_jump(save_slot, delta, 0) : ui_wrap(save_slot, delta, WT_USER_COUNT);
    else {
        uint32_t index = fine ? workflow_jump(preset_index, delta, 1) : ui_wrap(preset_index, delta, WT_VOICE_COUNT);
        workflow_load(index, 1);
    }
}

static void workflow_name_turn(int32_t delta)
{
    uint32_t i;
    for (i = 0; i < sizeof NAME_CHARS-1u && NAME_CHARS[i] != save_name[save_cursor]; i++) {}
    if (i == sizeof NAME_CHARS-1u) i = 0;
    save_name[save_cursor] = NAME_CHARS[ui_wrap(i, delta, sizeof NAME_CHARS-1u)];
}

static void workflow_save(void)
{
    uint32_t i;
    if (save_pending) { say("SAVE WAITING FOR QUIET"); return; }
    if (!saving) {
        for (i = 0; i < WT_NAME_LEN-1u; i++) save_name[i] = ' ';
        for (i = 0; edit.name[i]; i++) save_name[i] = edit.name[i];
        save_name[WT_NAME_LEN-1u] = 0; save_cursor = 0; saving = 1;
        page = PAGE_HOME; bank = focus = 0;
        if (preset_index >= WT_FACTORY_COUNT) save_slot = preset_index-WT_FACTORY_COUNT;
        say("NAME / SLOT / SAVE TO WRITE");
    } else {
        memcpy(edit.name, save_name, sizeof edit.name);
        if (!wt_patch_valid(&edit)) { say("INVALID PATCH"); return; }
        pending_patch = edit; save_target = save_slot; saving = 0; save_pending = 1;
        patch_publish(); say("SAVE WAITING FOR QUIET");
    }
}

static void workflow_cancel(void)
{
    if (saving || save_pending) {
        saving = save_pending = 0;
        if (flash_hold) { RING_PUBLISH(); flash_hold = 0; }
        say("SAVE CANCELLED");
    }
}

static void workflow_capture(wt_settings_t *s)
{
    wts_clear(s, sizeof *s);
    s->preferences = preferences; s->octave = octave; s->mono = mono; s->midi = midi_channel;
    s->span = spectrum_span; s->voice = preset_index; s->slot = save_slot; s->count = sq_n;
    memcpy(s->controls, play_controls, PL_N); memcpy(s->notes, sq, sq_n*sizeof sq[0]);
    s->midi_config = mi_config;
}

static const char *workflow_init(void)
{
    wt_settings_t s;
    uint32_t bad;
    int rc;
    preset_index = save_slot = saving = save_cursor = save_pending = save_target = pc_bank = 0;
    settings_dirty = settings_failed = 0; settings_at = fm1_ms;
    bad = wt_banks_load();
    workflow_capture(&s);
    rc = wt_settings_read(&s);
    if (!rc) {
        preferences = s.preferences; octave = s.octave; mono = s.mono; midi_channel = s.midi;
        mi_config = s.midi_config; mi_publish();
        spectrum_span = s.span; preset_index = s.voice; save_slot = s.slot;
        memcpy(play_controls, s.controls, PL_N); sq_n = (uint8_t)s.count; memcpy(sq, s.notes, sizeof sq);
    }
    if (workflow_load(preset_index, 0)) { wt_patch_init(&edit); other = edit; bad = 1; }
    pl_sync(); workflow_capture(&settings_seen);
    return bad || rc < 0 ? "STORAGE DATA REJECTED" : "WT LAB READY";
}

static void workflow_poll(void)
{
    wt_settings_t now;
    uint32_t i, busy = event_read != event_write || seq_run || (arp_on && chord_n);
    workflow_capture(&now);
    if (memcmp(&now, &settings_seen, sizeof now)) {
        settings_seen = now; settings_dirty = 1; settings_failed = 0; settings_at = fm1_ms;
    }
    for (i = 0; i < 155u; i++) if (input_owner[i]) busy = 1;
    if (busy) {
        if (flash_hold) { RING_PUBLISH(); flash_hold = 0; }
        return;
    }
    if (flash_hold == 2u) {
        int rc = 0, saved = 0;
        if (save_pending) {
            rc = wt_voice_store(save_target, &pending_patch);
            if (!rc) {
                saved = 1; save_slot = save_target;
                if (!memcmp(&edit, &pending_patch, sizeof edit)) preset_index = WT_FACTORY_COUNT+save_target;
                workflow_capture(&now); settings_seen = now; settings_dirty = 1;
            } else say("PATCH SAVE FAILED");
            save_pending = 0;
        }
        if (!rc && settings_dirty) {
            rc = wt_settings_write(&now);
            if (!rc) {
                settings_dirty = settings_failed = mi_dirty = 0; mi_drawn = ~0u;
                if (saved) say("PATCH SAVED");
            } else {
                settings_failed = 1; mi_feedback = 5; mi_drawn = ~0u;
                say(saved ? "PATCH SAVED / PREFS FAILED" : "SETTINGS SAVE FAILED");
            }
        }
        RING_PUBLISH(); flash_hold = 0;
    } else if (!flash_hold && (save_pending || (settings_dirty && !settings_failed && fm1_ms-settings_at >= 2000u))) {
        RING_PUBLISH(); flash_hold = 1;
    }
}

static void workflow_program(uint32_t program)
{
    uint32_t index;
    if (pc_bank > BANK_N || program >= (pc_bank ? WT_BANK_VOICES : WT_FACTORY_COUNT)) {
        say("INVALID BANK / PROGRAM"); return;
    }
    index = pc_bank ? WT_FACTORY_COUNT+(pc_bank-1u)*WT_BANK_VOICES+program : program;
    if (index >= WT_VOICE_COUNT) { say("INVALID BANK / PROGRAM"); return; }
    workflow_load(index, 0);                    /* never echo an incoming program change */
}
