/* SPDX-License-Identifier: GPL-3.0-only */
/* Explicit little-endian wire formats; never write a compiler's structure layout to flash. */
#define WT_PATCH_BYTES 172u
#define WT_BANK_VOICES 32u
#define WT_USER_COUNT (BANK_N * WT_BANK_VOICES)
#define WT_FACTORY_COUNT 8u
#define WT_VOICE_COUNT (WT_FACTORY_COUNT + WT_USER_COUNT)
#define WT_BANK_BYTES (16u + WT_BANK_VOICES * WT_PATCH_BYTES)
#define WT_SETTINGS_BYTES (240u+MI_BYTES)
typedef char wt_bank_fits[(WT_BANK_BYTES <= BANK_SECTORS * 4096u - 16u) ? 1 : -1];
static uint8_t user_patches[WT_USER_COUNT * WT_PATCH_BYTES], bank_rejected[BANK_N];
static uint8_t bank_stage[WT_BANK_BYTES] __attribute__((section(".pool")));

static void wt_put32(uint8_t *d, uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) d[i] = (uint8_t)(v >> (8u*i));
}
static uint32_t wt_get32(const uint8_t *s)
{
    return (uint32_t)s[0] | (uint32_t)s[1] << 8 | (uint32_t)s[2] << 16 | (uint32_t)s[3] << 24;
}
static int32_t wt_signed(uint32_t v) { return v <= 0x7FFFFFFFu ? (int32_t)v : -(int32_t)~v - 1; }

static int wt_patch_pack(uint8_t *d, const wt_patch_t *p)
{
    uint32_t i;
    if (!wt_patch_valid(p)) return ST_INVALID;
#define PUT(v) do { wt_put32(d, (uint32_t)(v)); d += 4; } while (0)
    for (i = 0; i < 2u; i++) {
        const wt_osc_patch_t *o = &p->osc[i];
        PUT(o->table); PUT(o->position); PUT(o->gain); PUT(o->semitone); PUT(o->fine); PUT(o->position_env);
    }
    for (i = 0; i < 2u; i++) {
        const wt_adsr_t *e = i ? &p->envelope : &p->amp;
        PUT(e->attack); PUT(e->decay); PUT(e->sustain); PUT(e->release);
    }
    PUT(p->filter_type); PUT(p->cutoff); PUT(p->resonance); PUT(p->keytrack); PUT(p->velocity); PUT(p->filter_env);
    for (i = 0; i < 2u; i++) {
        const wt_lfo_patch_t *l = &p->lfo[i];
        PUT(l->rate); PUT(l->shape); PUT(l->depth); PUT(l->destination); PUT(l->key_sync);
    }
#undef PUT
    memcpy(d, p->fx, 16); memcpy(d + 16, p->name, WT_NAME_LEN);
    return 0;
}

static int wt_patch_unpack(wt_patch_t *out, const uint8_t *s)
{
    wt_patch_t p;
    uint32_t i;
    wts_clear(&p, sizeof p);
#define GET(v) do { (v) = wt_get32(s); s += 4; } while (0)
#define SIGNED(v) do { (v) = wt_signed(wt_get32(s)); s += 4; } while (0)
    for (i = 0; i < 2u; i++) {
        wt_osc_patch_t *o = &p.osc[i];
        GET(o->table); GET(o->position); GET(o->gain); SIGNED(o->semitone); SIGNED(o->fine); SIGNED(o->position_env);
    }
    for (i = 0; i < 2u; i++) {
        wt_adsr_t *e = i ? &p.envelope : &p.amp;
        GET(e->attack); GET(e->decay); GET(e->sustain); GET(e->release);
    }
    GET(p.filter_type); GET(p.cutoff); GET(p.resonance); GET(p.keytrack); GET(p.velocity); SIGNED(p.filter_env);
    for (i = 0; i < 2u; i++) {
        wt_lfo_patch_t *l = &p.lfo[i];
        GET(l->rate); GET(l->shape); GET(l->depth); GET(l->destination); GET(l->key_sync);
    }
#undef GET
#undef SIGNED
    memcpy(p.fx, s, 16); memcpy(p.name, s + 16, WT_NAME_LEN);
    if (!wt_patch_valid(&p)) return ST_INVALID;
    *out = p; return 0;
}

static int wt_factory(wt_patch_t *p, uint32_t index)
{
    static const char *const names[] = {"INIT WAVE", "MORPH SAW", "HOLLOW PAD", "VOX MOTION",
                                        "ODD BASS", "GLASS KEYS", "PULSE LEAD", "DRAWBARS"};
    uint32_t i;
    if (index >= WT_FACTORY_COUNT) return ST_INVALID;
    wt_patch_init(p);
    wts_clear(p->name, sizeof p->name);
    for (i = 0; names[index][i]; i++) p->name[i] = names[index][i];
    if (index == 1u) { p->osc[0].position = 42000; p->osc[0].position_env = 18000; }
    if (index == 2u) {
        p->osc[0].table = 6; p->osc[0].position = 28000;
        p->osc[1].table = 5; p->osc[1].position = 18000; p->osc[1].gain = 14000;
        p->amp.attack = 700; p->amp.release = 1400;
        p->lfo[0].rate = 300; p->lfo[0].depth = 9000; p->lfo[0].destination = WT_MOD_BOTH;
        p->fx[0] = 25; p->fx[12] = 20;
    }
    if (index == 3u) {
        p->osc[0].table = 7; p->osc[0].position = 20000;
        p->filter_type = WT_FILTER_LP; p->cutoff = 42000; p->resonance = 7000;
        p->lfo[0].rate = 450; p->lfo[0].depth = 18000; p->lfo[0].destination = WT_MOD_POS1;
    }
    if (index == 4u) {
        p->osc[0].table = p->osc[1].table = 1; p->osc[0].position = 40000;
        p->osc[1].gain = 10000; p->osc[1].semitone = -12;
        p->filter_type = WT_FILTER_LP; p->cutoff = 25000; p->filter_env = 28000;
        p->amp.decay = 180; p->amp.sustain = 12000; p->amp.release = 90;
    }
    if (index == 5u) {
        p->osc[0].table = 5; p->osc[0].position = 40000;
        p->amp.decay = 900; p->amp.sustain = 0; p->amp.release = 600; p->fx[12] = 20;
    }
    if (index == 6u) {
        p->osc[0].table = p->osc[1].table = 2;
        p->osc[0].position = p->osc[1].position = 20000;
        p->osc[1].gain = 20000; p->osc[1].fine = -7;
        p->filter_type = WT_FILTER_LP; p->cutoff = 50000; p->resonance = 5000;
        p->fx[0] = 20; p->fx[8] = 15;
    }
    if (index == 7u) {
        p->osc[0].table = p->osc[1].table = 4;
        p->osc[0].position = 52000; p->osc[1].gain = 8000; p->osc[1].semitone = 12;
        p->amp.attack = 0; p->amp.decay = 0; p->amp.sustain = WT_GAIN_MAX;
        p->fx[4] = 20; p->fx[12] = 12;
    }
    return wt_patch_valid(p) ? 0 : ST_INVALID;
}

static uint32_t wt_banks_load(void)
{
    wt_patch_t p;
    uint8_t init[WT_PATCH_BYTES];
    uint32_t b, i, bad = 0;
    wt_patch_init(&p); wt_patch_pack(init, &p);
    for (b = 0; b < BANK_N; b++) {
        int rc = bank_read(b, bank_stage, sizeof bank_stage);
        for (i = 0; i < WT_BANK_VOICES; i++)
            memcpy(user_patches + (b*WT_BANK_VOICES+i)*WT_PATCH_BYTES, init, sizeof init);
        if (!rc) {
            if (wt_get32(bank_stage) != 1u || wt_get32(bank_stage+4) != WT_PATCH_BYTES ||
                wt_get32(bank_stage+8) != WT_BANK_VOICES || wt_get32(bank_stage+12)) rc = ST_INVALID;
            for (i = 0; !rc && i < WT_BANK_VOICES; i++)
                rc = wt_patch_unpack(&p, bank_stage+16u+i*WT_PATCH_BYTES);
            if (!rc) memcpy(user_patches+b*WT_BANK_VOICES*WT_PATCH_BYTES, bank_stage+16, WT_BANK_BYTES-16u);
        }
        bank_rejected[b] = (uint8_t)(rc < 0);
        if (rc < 0) bad |= 1u << b;
    }
    return bad;
}

static int wt_voice_get(wt_patch_t *p, uint32_t index)
{
    if (index >= WT_VOICE_COUNT) return ST_INVALID;
    return index < WT_FACTORY_COUNT ? wt_factory(p, index) :
           wt_patch_unpack(p, user_patches + (index-WT_FACTORY_COUNT)*WT_PATCH_BYTES);
}

static int wt_voice_store(uint32_t slot, const wt_patch_t *p)
{
    uint32_t b, expected_crc;
    int rc;
    if (slot >= WT_USER_COUNT || !wt_patch_valid(p)) return ST_INVALID;
    b = slot / WT_BANK_VOICES;
    wt_put32(bank_stage, 1); wt_put32(bank_stage+4, WT_PATCH_BYTES);
    wt_put32(bank_stage+8, WT_BANK_VOICES); wt_put32(bank_stage+12, 0);
    memcpy(bank_stage+16, user_patches+b*WT_BANK_VOICES*WT_PATCH_BYTES, WT_BANK_BYTES-16u);
    rc = wt_patch_pack(bank_stage+16+(slot%WT_BANK_VOICES)*WT_PATCH_BYTES, p);
    expected_crc = crc32(bank_stage, sizeof bank_stage);
    if (!rc) rc = bank_write(b, bank_stage, sizeof bank_stage);
    if (!rc) rc = bank_read(b, bank_stage, sizeof bank_stage); /* verify programmed data before committing RAM */
    if (!rc && crc32(bank_stage, sizeof bank_stage) != expected_crc) rc = ST_INVALID;
    if (rc) return rc;
    memcpy(user_patches+b*WT_BANK_VOICES*WT_PATCH_BYTES, bank_stage+16, WT_BANK_BYTES-16u);
    bank_rejected[b] = 0; return 0;
}

typedef struct {
    wt_preferences_t preferences;
    int32_t octave;
    uint32_t mono, midi, span, voice, slot, count;
    uint8_t controls[PL_N];
    sqn_t notes[SQ_MAX];
    mi_config_t midi_config;
} wt_settings_t;

static int wt_preferences_valid(const wt_preferences_t *p)
{
    return p->transpose >= -24 && p->transpose <= 24 && p->fine >= -256 && p->fine <= 256 &&
           p->bits <= 3u && p->output_mono <= 1u && p->pc_send <= 1u &&
           p->sample_rate < WT_SAMPLE_RATE_COUNT;
}

static int wt_settings_valid(const wt_settings_t *s)
{
    static const uint8_t maxima[PL_N] = {99,4,3,1,2,2,1,99,5,25,99};
    uint32_t i;
    if (!mi_valid(&s->midi_config) || !wt_preferences_valid(&s->preferences) ||
        s->octave < -2 || s->octave > 2 || s->mono > 1u ||
        s->midi > 16u || s->span > 4u || s->voice >= WT_VOICE_COUNT || s->slot >= WT_USER_COUNT ||
        s->count > SQ_MAX) return 0;
    for (i = 0; i < PL_N; i++) if (s->controls[i] > maxima[i]) return 0;
    for (i = 0; i < s->count; i++)
        if (s->notes[i].step >= SQ_MAX || s->notes[i].note > 127u || !s->notes[i].len ||
            s->notes[i].len > SQ_MAX) return 0;
    return 1;
}

static int wt_settings_pack(uint8_t d[WT_SETTINGS_BYTES], const wt_settings_t *s)
{
    uint32_t i;
    if (!wt_settings_valid(s)) return ST_INVALID;
    wts_clear(d, WT_SETTINGS_BYTES);
    wt_put32(d, mi_default_config(&s->midi_config) ? (s->preferences.sample_rate ? 2u : 1u) : 3u);
    wt_put32(d+4, (uint32_t)s->preferences.transpose);
    wt_put32(d+8, (uint32_t)s->preferences.fine); wt_put32(d+12, s->preferences.bits);
    wt_put32(d+16, s->preferences.output_mono); wt_put32(d+20, s->preferences.pc_send);
    d[24] = (uint8_t)s->octave; d[25] = (uint8_t)s->mono; d[26] = (uint8_t)s->midi; d[27] = (uint8_t)s->span;
    d[28] = (uint8_t)s->voice; d[29] = (uint8_t)(s->voice >> 8);
    d[30] = (uint8_t)s->slot; d[31] = (uint8_t)(s->slot >> 8);
    memcpy(d+32, s->controls, PL_N); d[43] = (uint8_t)s->count;
    for (i = 0; i < s->count; i++) {
        d[44+3*i] = s->notes[i].step; d[45+3*i] = s->notes[i].note; d[46+3*i] = s->notes[i].len;
    }
    wt_put32(d+236,s->preferences.sample_rate);
    if (wt_get32(d) == 3u) memcpy(d+240,&s->midi_config,MI_BYTES);
    return 0;
}

static int wt_settings_unpack(wt_settings_t *out, const uint8_t d[WT_SETTINGS_BYTES])
{
    wt_settings_t s;
    uint32_t i, version = wt_get32(d);
    if (version != 1u && version != 2u && version != 3u) return ST_INVALID;
    if (version == 1u && wt_get32(d+236)) return ST_INVALID;
    if (version < 3u)
        for (i = 240u; i < WT_SETTINGS_BYTES; i++) if (d[i]) return ST_INVALID;
    wts_clear(&s, sizeof s);
    mi_defaults(&s.midi_config);
    if (version == 3u) memcpy(&s.midi_config,d+240,MI_BYTES);
    s.preferences.transpose = wt_signed(wt_get32(d+4)); s.preferences.fine = wt_signed(wt_get32(d+8));
    s.preferences.bits = wt_get32(d+12); s.preferences.output_mono = wt_get32(d+16);
    s.preferences.pc_send = wt_get32(d+20);
    s.preferences.sample_rate = version >= 2u ? wt_get32(d+236) : 0u;
    s.octave = d[24] < 128u ? d[24] : (int32_t)d[24]-256;
    s.mono = d[25]; s.midi = d[26]; s.span = d[27];
    s.voice = d[28] + 256u*d[29]; s.slot = d[30] + 256u*d[31];
    memcpy(s.controls, d+32, PL_N); s.count = d[43];
    if (s.count > SQ_MAX) return ST_INVALID;
    for (i = 0; i < s.count; i++) {
        s.notes[i].step = d[44+3*i]; s.notes[i].note = d[45+3*i]; s.notes[i].len = d[46+3*i];
    }
    if (!wt_settings_valid(&s)) return ST_INVALID;
    *out = s; return 0;
}

static int wt_settings_read(wt_settings_t *s)
{
    uint8_t bytes[WT_SETTINGS_BYTES];
    int rc = settings_load(bytes, sizeof bytes);
    return rc ? rc : wt_settings_unpack(s, bytes);
}

static int wt_settings_write(const wt_settings_t *s)
{
    uint8_t bytes[WT_SETTINGS_BYTES], verify[WT_SETTINGS_BYTES];
    int rc = wt_settings_pack(bytes, s);
    if (!rc) rc = settings_save(bytes,wt_get32(bytes) == 3u ? sizeof bytes : 240u);
    if (!rc) rc = settings_load(verify, sizeof verify);
    if (!rc && memcmp(bytes, verify, sizeof bytes)) rc = ST_INVALID;
    return rc;
}
