/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
enum { ST_EMPTY = 1, ST_INVALID = -1, ST_IO = -2 };
#define SETTINGS_AT 0x000FC000u                 /* one 4 KiB sector of Felucca's globals area */
#define BANK_AT     0x000FD000u                 /* bank 0: the other two */
#define BANK_HI_AT  0x000F6000u                 /* banks 1, 2, 3: two sectors each, up to SETTINGS_AT */
#define BANK_LO_AT  0x000EE000u                 /* banks 4, 5, 6, 7: likewise, up to BANK_HI_AT */
#define BANK_SECTORS 2u
#define BANK_N      8u
#define BANK_ADDR(b) ((b) == 0u ? BANK_AT : (b) < 4u ? BANK_HI_AT + ((b) - 1u) * BANK_SECTORS * 0x1000u : \
                      BANK_LO_AT + ((b) - 4u) * BANK_SECTORS * 0x1000u)
