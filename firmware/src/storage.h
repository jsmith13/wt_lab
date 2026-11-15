/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared flash record implementation; hardware and host supply the same read/erase/program API. */
/* ---- settings: [magic][length][crc32] then the bytes; settings_save erases the sector (~50 ms with
 * the interrupts off: the sound drops out), so save when nothing plays, not on every knob step */
#define SETTINGS_MAGIC 0x32535457u              /* WTS2: native WT settings, never FM/baseline layouts */
static uint32_t crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu, k;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8u; k++)
            c = c & 1u ? (c >> 1) ^ 0xEDB88320u : c >> 1;
    }
    return ~c;
}
static int settings_load(void *p, uint32_t n)   /* 0: loaded; else p is untouched */
{
    static uint8_t buf[512];
    uint32_t h[3];
    if (!flash_ok || st_read(SETTINGS_AT, h, sizeof h)) return ST_IO;
    if (h[0] == 0xFFFFFFFFu && h[1] == 0xFFFFFFFFu && h[2] == 0xFFFFFFFFu) return ST_EMPTY;
    if (n > sizeof buf || !n || h[0] != SETTINGS_MAGIC ||
        (h[1] != n && !(n == 312u && h[1] == 240u))) return ST_INVALID;
    if (st_read(SETTINGS_AT + sizeof h, buf, h[1])) return ST_IO;
    if (crc32(buf, h[1]) != h[2]) return ST_INVALID;
    memset(p,0,n);
    memcpy(p, buf, h[1]);
    return 0;
}
static int settings_save(const void *p, uint32_t n)   /* p in RAM, n <= 512 */
{
    uint32_t h[3] = {SETTINGS_MAGIC, n, 0}, took;
    int rc;
    if (!n || n > 512u) return ST_INVALID;
    if (!flash_ok) return ST_IO;
    h[2] = crc32(p, n);
    if ((rc = fl_erase4k(SETTINGS_AT, &took)) != 0)
        return rc;
    if ((rc = fl_write(SETTINGS_AT + sizeof h, p, n)) != 0)
        return rc;
    return fl_write(SETTINGS_AT, (const uint8_t *)h, sizeof h);   /* the header last: a cut save stays invalid */
}

/* ---- native voice banks: the same header, each in its own two sectors.
 * The application versions and validates the payload. Two erases, about 100 ms without sound:
 * write one when nothing is playing. A cut write leaves no valid header and that bank reads as empty,
 * which is the right way round -- a half-written bank must never look like a good one. */
#define BANK_MAGIC 0x32425457u                  /* WTB2: native WT banks */
static int bank_read(uint32_t b, void *p, uint32_t n)   /* 0: read and checked; else p holds nothing worth having */
{
    uint32_t h[3], at = BANK_ADDR(b);
    if (b >= BANK_N || n > BANK_SECTORS * 0x1000u - 16u) return ST_INVALID;
    if (!flash_ok || st_read(at, h, sizeof h)) return ST_IO;
    if (h[0] == 0xFFFFFFFFu && h[1] == 0xFFFFFFFFu && h[2] == 0xFFFFFFFFu) return ST_EMPTY;
    if (h[0] != BANK_MAGIC || h[1] != n) return ST_INVALID;
    if (st_read(at + 16u, p, n)) return ST_IO;
    if (crc32(p, n) != h[2]) return ST_INVALID;
    return 0;
}
static int bank_write(uint32_t b, const void *p, uint32_t n)
{
    uint32_t h[3] = {BANK_MAGIC, n, 0}, took, s, at = BANK_ADDR(b);
    int rc;
    if (b >= BANK_N || !n || n > BANK_SECTORS * 0x1000u - 16u) return ST_INVALID;
    if (!flash_ok) return ST_IO;
    h[2] = crc32(p, n);
    for (s = 0; s < BANK_SECTORS; s++)
        if ((rc = fl_erase4k(at + s * 0x1000u, &took)) != 0)
            return rc;
    if ((rc = fl_write(at + 16u, p, n)) != 0)
        return rc;
    return fl_write(at, (const uint8_t *)h, sizeof h);
}

