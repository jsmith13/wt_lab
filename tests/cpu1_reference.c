/* SPDX-License-Identifier: GPL-3.0-only */
/* Independent original serial algorithms, with the expanded shared factory data. */
#define wt_table_info ref_wt_table_info
#define WT_WAVES REF_WT_WAVES
#define wt_patch_init ref_wt_patch_init
#define wt_patch_valid ref_wt_patch_valid
#define wt_filter_hz ref_wt_filter_hz
#define wt_pitch_increment ref_wt_pitch_increment
#define wt_osc_init ref_wt_osc_init
#define wt_osc_start ref_wt_osc_start
#define wt_osc_set ref_wt_osc_set
#define wt_osc_render ref_wt_osc_render
#define wt_osc_peek ref_wt_osc_peek
#define wt_synth_init ref_wt_synth_init
#define wt_synth_patch ref_wt_synth_patch
#define wt_synth_note_on ref_wt_synth_note_on
#define wt_synth_note_off ref_wt_synth_note_off
#define wt_synth_mono ref_wt_synth_mono
#define wt_synth_tune ref_wt_synth_tune
#define wt_synth_panic ref_wt_synth_panic
#define wt_synth_sustain ref_wt_synth_sustain
#define wt_synth_all_notes_off ref_wt_synth_all_notes_off
#define wt_synth_silence ref_wt_synth_silence
#define wt_synth_render wt_reference_render
#include "../firmware/src/wt_synth.h"
#include "../firmware/src/wt_data.h"
#include "reference/wt_synth.c"
