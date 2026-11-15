/* SPDX-License-Identifier: GPL-3.0-only */
/* Native voice adapters around FM Lab's sample-clocked arp and quantized phrase looper. */
enum { UI_TEMPO, UI_APAT, UI_AOCT, UI_LATCH, UI_BARS, UI_DIV, UI_CLICK, UI_AGATE, UI_ARATE, UI_SWING, UI_AVEL, PL_N };
static uint8_t play_controls[PL_N];
#define SQ_MAX 64u                              /* notes in a phrase */
#define ARP_MAX 16u                             /* keys in a chord */
enum { AP_UP = 0, AP_DOWN, AP_UPDOWN, AP_RANDOM, AP_PLAYED };
typedef struct { uint8_t step, note, len; } sqn_t;   /* a note of the phrase, in sixteenths */
typedef struct {
    uint32_t step_q8;                           /* a sixteenth: Q8 samples */
    uint32_t a_len[2], a_gate[2];               /* an arp step on the beat and off it -- SWING is the
                                                 * difference -- and how much of each sounds */
    uint8_t run, gen, cin, click;               /* gen: which PLAY; cin: steps of count-in before the loop */
    uint8_t arp, pat, oct, an, vel, sq_n, sq_len;
    uint8_t chord[ARP_MAX];                     /* ascending */
    uint8_t ord[ARP_MAX];                       /* ..and as the keys went down */
    sqn_t sq[SQ_MAX];
} play_t;
static play_t plbuf[2];
static volatile uint32_t plcur;
/* where the clock is, for the main loop to record against: the step since PLAY (count-in and all),
 * how far into it in Q8 samples, and which PLAY -- or 0xFFFF, stopped */
static volatile uint32_t pl_at, pl_atph, pl_atgen = 0xFFFFu, pl_clock_version;

static uint32_t pl_ph, pl_step, arp_ph, arp_rng = 1u;
static uint32_t clk_ph, clk_inc;                /* the click: a square, dying away */
static int32_t clk_a;                           /* its level, Q8 */
#define CLK_LEN 900                             /* 20 ms */
#define CLK_A0 (5000 * 256)
static uint8_t pl_run, pl_gen, pl_an, arp_odd, arp_ni, arp_oi, arp_up = 1, arp_new, arp_note = 0xFFu;
static uint8_t sq_note[SQ_MAX], sq_left[SQ_MAX];  /* what each note of the phrase is sounding, and for
                                                   * how many more steps: the note is kept, so a phrase
                                                   * replaced under it can still let it go */

static void play_audio_panic(void)
{
    wts_clear(sq_left, sizeof sq_left); arp_note = 0xFFu; clk_a = 0;
}

static void pl_on(uint32_t n, uint32_t vel) { audio_generated_on(n, vel); }
static void pl_off(uint32_t n) { audio_generated_off(n); }

static void arp_upstep(uint32_t n, uint32_t o)
{
    if (++arp_ni >= n) { arp_ni = 0; if (++arp_oi >= o) arp_oi = 0; }
}

static void arp_downstep(uint32_t n, uint32_t o)
{
    if (arp_ni) { arp_ni--; return; }
    arp_ni = (uint8_t)(n - 1u);
    arp_oi = (uint8_t)(arp_oi ? arp_oi - 1u : o - 1u);
}

static uint32_t arp_rand(uint32_t n)            /* 0..n-1, without a division */
{
    arp_rng = arp_rng * 1664525u + 1013904223u;
    return ((arp_rng >> 16) * n) >> 16;
}

static void arp_next(const play_t *p)
{
    uint32_t n = p->an, o = p->oct + 1u;
    if (arp_ni >= n) arp_ni = (uint8_t)(n - 1u);    /* a key let go under it */
    if (arp_oi >= o) arp_oi = (uint8_t)(o - 1u);
    if (p->pat == AP_RANDOM) {
        arp_ni = (uint8_t)arp_rand(n);
        arp_oi = (uint8_t)arp_rand(o);
    } else if (arp_new) {                       /* a fresh chord: from its own end */
        arp_up = p->pat != AP_DOWN;
        arp_ni = (uint8_t)(arp_up ? 0u : n - 1u);
        arp_oi = (uint8_t)(arp_up ? 0u : o - 1u);
    } else if (p->pat == AP_UP || p->pat == AP_PLAYED) {
        arp_upstep(n, o);
    } else if (p->pat == AP_DOWN) {
        arp_downstep(n, o);
    } else if (n * o > 1u) {                    /* up and down, without playing either end twice */
        if (arp_up && arp_ni + 1u >= n && arp_oi + 1u >= o) arp_up = 0;
        else if (!arp_up && !arp_ni && !arp_oi) arp_up = 1;
        if (arp_up) arp_upstep(n, o); else arp_downstep(n, o);
    }
    arp_new = 0;
}

static void clk_go(uint32_t bar)                /* a tick; higher on the first beat of a bar */
{
    clk_ph = 0;
    clk_inc = bar ? 194783098u : 97391549u;     /* 2 kHz, 1 kHz: 2^32 f / FS */
    clk_a = CLK_A0;
}

static void pl_tick(const play_t *p)            /* once a control block, before it is rendered */
{
    uint32_t i, an = p->arp ? p->an : 0u;
    if (p->run != pl_run || (p->run && p->gen != pl_gen)) {   /* started, stopped or started over:
                                                               * from the top, now */
        for (i = 0; i < SQ_MAX; i++)
            if (sq_left[i]) { pl_off(sq_note[i]); sq_left[i] = 0; }
        pl_run = p->run;
        pl_gen = p->gen;
        if (pl_run) {
            pl_step = 0xFFFFFFFFu;
            pl_ph = p->step_q8;
            arp_ph = p->a_len[1];               /* the arpeggiator on the same downbeat */
            arp_odd = 1;
        }
    }
    if ((an != 0u) != (pl_an != 0u)) {         /* the arpeggiator started or stopped */
        if (arp_note != 0xFFu) { pl_off(arp_note); arp_note = 0xFFu; }
        if (an) {
            arp_new = 1;
            if (!pl_run) {                      /* on its own it starts with the key, not the beat */
                arp_ph = p->a_len[1];
                arp_odd = 1;
            }
        }
    }
    pl_an = (uint8_t)an;
    if (pl_ph >= p->step_q8) {
        pl_ph -= p->step_q8;
        if (pl_ph >= p->step_q8)
            pl_ph = 0;                          /* the tempo went up past where we were */
        pl_step++;
        if (pl_run && pl_step < p->cin) {       /* the count-in: only the click */
            if (!(pl_step & 3u))
                clk_go(!(pl_step & 15u));
        } else if (pl_run && p->sq_len) {
            uint32_t pos = (pl_step - p->cin) & (p->sq_len - 1u);   /* 16, 32 or 64 */
            if (p->click && !(pos & 3u))
                clk_go(!(pos & 15u));
            for (i = 0; i < SQ_MAX; i++)
                if (sq_left[i] && --sq_left[i] == 0u)
                    pl_off(sq_note[i]);
            for (i = 0; i < p->sq_n; i++)
                if (p->sq[i].step == pos) {
                    if (sq_left[i]) pl_off(sq_note[i]);
                    sq_note[i] = p->sq[i].note;
                    sq_left[i] = p->sq[i].len;
                    pl_on(sq_note[i], 100u);
                }
        }
    }
    pl_ph += (uint32_t)WT_CONTROL_SAMPLES << 8;
    /* the arpeggiator's own steps, at RATE and swung: they run on whether or not there is a chord,
     * so one started under a playing phrase lands on its grid */
    if (arp_note != 0xFFu && arp_ph >= p->a_gate[arp_odd]) { pl_off(arp_note); arp_note = 0xFFu; }
    if (arp_ph >= p->a_len[arp_odd]) {
        arp_ph -= p->a_len[arp_odd];
        arp_odd ^= 1u;
        if (arp_ph >= p->a_len[arp_odd])
            arp_ph = 0;                         /* the tempo went up past where we were */
        if (an) {
            uint32_t n;
            if (arp_note != 0xFFu) pl_off(arp_note);
            arp_next(p);
            n = (uint32_t)(p->pat == AP_PLAYED ? p->ord : p->chord)[arp_ni] + 12u * arp_oi;
            arp_note = (uint8_t)(n > 127u ? 127u : n);
            pl_on(arp_note, p->vel);
        }
    }
    arp_ph += (uint32_t)WT_CONTROL_SAMPLES << 8;
    pl_clock_version++; RING_PUBLISH();
    pl_at = pl_step;
    pl_atph = pl_ph;
    pl_atgen = pl_run ? pl_gen : 0xFFFFu;
    RING_PUBLISH(); pl_clock_version++;
}

static uint8_t seq_run, seq_gen, seq_cin, arp_on, rec, pl_dirty;   /* rec: 1 to begin where the loop is,
                                                                   * 2 taking notes */
static uint8_t chord[ARP_MAX], chord_n;
static uint8_t chord_ord[ARP_MAX];              /* the same keys, in the order they went down */
static uint8_t input_owner[155], input_note[155]; /* 27 panel keys plus 128 incoming MIDI pitches */
static sqn_t sq[SQ_MAX];                        /* the phrase */
static uint8_t sq_n;
static uint32_t rec_from, rec_to;               /* the recording, in steps since PLAY */
static uint8_t rkeys[155], rk_note[155];
static uint32_t rk_st[155];
static uint32_t stc;
static uint8_t audition_notes[WT_VOICES], audition_count;
static uint32_t audition_at;

static uint32_t bpm(void)
{
    return 40u + 2u * play_controls[UI_TEMPO];
}

static uint32_t sq_len(void) { return 16u << play_controls[UI_BARS]; }
static uint32_t sq_div(void) { return 1u << play_controls[UI_DIV]; }  /* in sixteenths */

/* Where the loop is, in Q8 sixteenths since PLAY -- or 0 when it is not running, or the ISR has not
 * yet taken this PLAY. The ISR can land between two reads, so they are read until they agree. */
static uint32_t clock_at(uint32_t *q8)
{
    uint32_t st, ph, g, version;
    do {
        version = pl_clock_version; RING_PUBLISH();
        st = pl_at;
        ph = pl_atph;
        g = pl_atgen;
        RING_PUBLISH();
    } while ((version & 1u) || version != pl_clock_version);
    if (!seq_run || g != seq_gen)
        return 0;
    *q8 = st * 256u + ph * 256u / (FS * 15u * 256u / bpm());
    return 1;
}

static uint32_t q_round(uint32_t q8)            /* the nearest DIV, in sixteenths */
{
    uint32_t d = sq_div();
    return (q8 + d * 128u) / (d * 256u) * d;
}

static void chord_add(uint8_t n)
{
    uint32_t i, j;
    for (i = 0; i < chord_n && chord[i] < n; i++)
        ;
    if ((i < chord_n && chord[i] == n) || chord_n >= ARP_MAX)
        return;
    for (j = chord_n; j > i; j--)
        chord[j] = chord[j - 1u];
    chord[i] = n;
    chord_ord[chord_n] = n;
    chord_n++;
}

static void chord_del(uint8_t n)
{
    uint32_t i;
    for (i = 0; i < chord_n && chord[i] != n; i++)
        ;
    if (i >= chord_n)
        return;
    for (; i + 1u < chord_n; i++)
        chord[i] = chord[i + 1u];
    for (i = 0; i < chord_n && chord_ord[i] != n; i++)
        ;
    for (; i + 1u < chord_n; i++)
        chord_ord[i] = chord_ord[i + 1u];
    chord_n--;
}

static void sq_put(uint8_t note, uint32_t st, uint32_t len)   /* st in steps since PLAY */
{
    uint32_t i, n = sq_len(), pos = (st - seq_cin) & (n - 1u);
    if (len > n)
        len = n;
    for (i = 0; i < sq_n; i++)                  /* played again where it already was: replaced */
        if (sq[i].step == pos && sq[i].note == note)
            break;
    if (i == sq_n) {
        if (sq_n >= SQ_MAX) {
            say("SEQ FULL");
            return;
        }
        sq_n++;
    }
    sq[i].step = (uint8_t)pos;
    sq[i].note = note;
    sq[i].len = (uint8_t)(len ? len : 1u);
    pl_dirty = 1;
}

static void rec_down(uint32_t k, uint8_t note)  /* a key played while recording */
{
    uint32_t q, r;
    if (rec != 2u || !clock_at(&q))
        return;
    r = q_round(q);
    if (r < rec_from)                           /* the count-in, and not close enough to its end */
        return;
    rkeys[k] = 1;
    rk_st[k] = r;
    rk_note[k] = note;
}

static void rec_close(uint32_t k, uint32_t e)   /* key k's note ends at step e */
{
    rkeys[k] = 0;
    sq_put(rk_note[k], rk_st[k], e > rk_st[k] ? e - rk_st[k] : sq_div());
}

static void rec_up(uint32_t k)
{
    uint32_t q;
    if (rkeys[k])
        rec_close(k, clock_at(&q) ? q_round(q) : 0u);
}

static void rec_stop(void)                      /* what is still held ends here */
{
    uint32_t k, q, e = clock_at(&q) ? q_round(q) : 0u;
    if (rec == 2u && e > rec_to)
        e = rec_to;
    for (k = 0; k < 155u; k++)
        if (rkeys[k])
            rec_close(k, e);
    rec = 0;
}

static void seq_start(uint32_t cin)             /* from the top, after cin steps of count-in */
{
    seq_run = 1;
    seq_gen++;
    seq_cin = (uint8_t)cin;
}

static void rec_poll(void)                      /* every pass: begin an overdub, end a recording */
{
    uint32_t q;
    if (!rec || !clock_at(&q))
        return;
    if (rec == 1u) {
        rec_from = q >> 8;
        rec_to = rec_from + sq_len();
        rec = 2;
    } else if ((q >> 8) >= rec_to) {
        rec_stop();
        say(sq_n ? "REC DONE" : "NOTHING RECORDED");
    }
}

static uint32_t counting_in(void)               /* REC is waiting for the downbeat */
{
    uint32_t q;
    return rec == 2u && (!clock_at(&q) || (q >> 8) < rec_from);
}

static void pl_sync(void)                       /* hand the ISR what it plays: every pass, as the
                                                 * effects are, because it is cheaper than tracking */
{
    static const uint8_t ARP_DIV[6] = {1, 2, 3, 4, 6, 8};   /* RATE: steps to the beat */
    play_t *d = &plbuf[plcur ^ 1u];
    uint32_t a, i, g = play_controls[UI_AGATE] + 1u, v = play_controls[UI_AVEL];
    d->step_q8 = FS * 15u * 256u / bpm();       /* 44100 * 15 * 256 still fits */
    a = FS * 60u * 256u / bpm() / ARP_DIV[play_controls[UI_ARATE] < 6u ? play_controls[UI_ARATE] : 3u];   /* 52 s Q8 at most */
    d->a_len[0] = a / 50u * (50u + play_controls[UI_SWING]);   /* the on-beat step the longer; the pair the same */
    d->a_len[1] = 2u * a - d->a_len[0];
    for (i = 0; i < 2u; i++)
        d->a_gate[i] = d->a_len[i] / 100u * g;
    d->vel = (uint8_t)(v ? (v * 127u + 49u) / 99u : 1u);
    d->run = seq_run;
    d->gen = seq_gen;
    d->cin = seq_cin;
    d->click = play_controls[UI_CLICK];
    d->arp = arp_on;
    d->pat = play_controls[UI_APAT];
    d->oct = play_controls[UI_AOCT];
    d->an = chord_n;
    memcpy(d->chord, chord, chord_n);
    memcpy(d->ord, chord_ord, chord_n);
    d->sq_n = sq_n;
    d->sq_len = (uint8_t)sq_len();
    memcpy(d->sq, sq, sq_n * sizeof sq[0]);
    RING_PUBLISH();
    plcur ^= 1u;
}

static void performance_up(uint32_t source)
{
    uint32_t i, note = input_note[source], owner = input_owner[source];
    rec_up(source); input_owner[source] = 0;
    if (owner == 1u) ev_push_source(EV_OFF, note, 0, source);
    if (owner == 2u && !play_controls[UI_LATCH]) {
        for (i = 0; i < 155u; i++)
            if (input_owner[i] == 2u && input_note[i] == note) break;
        if (i == 155u) chord_del((uint8_t)note);
    }
}

static void performance_down(uint32_t source, uint32_t note, uint32_t velocity, uint32_t audition)
{
    uint32_t i;
    if (input_owner[source]) performance_up(source);
    input_note[source] = (uint8_t)note;
    if (arp_on && !audition) {
        for (i = 0; i < 155u && input_owner[i] != 2u; i++) {}
        if (i == 155u && play_controls[UI_LATCH]) chord_n = 0;
        input_owner[source] = 2;
        if (chord_n == ARP_MAX) say("ARP CHORD FULL");
        chord_add((uint8_t)note);
    } else {
        if (ev_push_source(EV_ON, note, velocity, source) == 0) input_owner[source] = 1;
    }
    if (!audition) rec_down(source, (uint8_t)note);
}

static void performance_stop(void)
{
    rec_stop(); seq_run = arp_on = chord_n = 0;
    pl_dirty = 1; pl_sync();
}

static void performance_latch(void)
{
    uint32_t i;
    chord_n = 0;
    for (i = 0; i < 155u; i++)
        if (input_owner[i] == 2u) chord_add(input_note[i]);
}

static void performance_clear(void)
{
    wts_clear(rkeys, sizeof rkeys); rec = seq_run = sq_n = 0; stc = 0; pl_dirty = 1;
}

static int st_toggle(uint8_t note)
{
    uint32_t i;
    for (i = 0; i < sq_n; i++) if (sq[i].step == stc && sq[i].note == note) break;
    if (i < sq_n) {
        for (; i+1u < sq_n; i++) sq[i] = sq[i+1u];
        sq_n--; pl_dirty = 1; return 0;
    }
    if (sq_n == SQ_MAX) { say("SEQ FULL"); return 0; }
    sq[sq_n].step = (uint8_t)stc; sq[sq_n].note = note; sq[sq_n].len = (uint8_t)sq_div();
    sq_n++; pl_dirty = 1; return 1;
}

static void st_len(int32_t delta)
{
    uint32_t i, found = 0;
    for (i = 0; i < sq_n; i++) if (sq[i].step == stc) {
        int64_t value = sq[i].len + (int64_t)delta;
        sq[i].len = (uint8_t)(value < 1 ? 1 : value > (int32_t)sq_len() ? sq_len() : value);
        found = 1;
    }
    if (!found) say("EMPTY STEP");
    else pl_dirty = 1;
}

static void audition_stop(void)
{
    uint32_t i;
    for (i = 0; i < audition_count; i++) ev_push(EV_OFF, audition_notes[i], 0);
    audition_count = 0;
}

static void audition_cursor(void)
{
    uint32_t i;
    audition_stop();
    if (seq_run) return;
    for (i = 0; i < sq_n && audition_count < WT_VOICES; i++) if (sq[i].step == stc) {
        audition_notes[audition_count++] = sq[i].note; ev_push(EV_ON, sq[i].note, 100);
    }
    audition_at = fm1_ms;
}

static void performance_init(void)
{
    static const uint8_t defaults[PL_N] = {40,0,0,0,0,0,0,74,3,0,78};
    memcpy(play_controls, defaults, sizeof defaults);
    wts_clear(plbuf, sizeof plbuf); wts_clear(sq, sizeof sq);
    wts_clear(input_owner, sizeof input_owner); wts_clear(input_note, sizeof input_note);
    wts_clear(rkeys, sizeof rkeys); wts_clear(rk_st, sizeof rk_st); wts_clear(rk_note, sizeof rk_note);
    wts_clear(sq_left, sizeof sq_left); wts_clear(sq_note, sizeof sq_note);
    seq_run = seq_gen = seq_cin = arp_on = rec = pl_dirty = chord_n = sq_n = 0;
    plcur = pl_ph = pl_step = arp_ph = clk_ph = clk_inc = pl_run = pl_gen = pl_an = arp_odd = arp_ni = arp_oi = arp_new = 0;
    rec_from = rec_to = stc = pl_at = pl_atph = pl_clock_version = audition_at = audition_count = 0;
    pl_atgen = 0xFFFFu; arp_rng = 1; arp_up = 1; arp_note = 0xFFu; clk_a = 0;
    pl_sync();
}

