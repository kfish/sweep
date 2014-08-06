/*
 * Sweep, a sound wave editor.
 *
 * Copyright (C) 2000 Conrad Parker
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __SWEEP_SAMPLE_H__
#define __SWEEP_SAMPLE_H__

#include <sweep/sweep_types.h>

sw_sample *
sample_new_empty(char * pathname, gint nr_channels, gint sample_rate,
		 sw_framecount_t sample_length);

sw_sample *
sample_new_copy(sw_sample * s);

void
sample_destroy (sw_sample * s);

sw_sounddata *
sample_get_sounddata (sw_sample * s);

void
sample_set_file_format (sw_sample * s, sw_file_format_t file_format);

void
sample_set_pathname (sw_sample * s, char * pathname);

GList *
sample_bank_list_names (void);

sw_sample *
sample_bank_find_byname (const gchar * name);

gboolean
sample_bank_contains (sw_sample *s);

void
sample_bank_add (sw_sample * s);

void
sample_bank_remove (sw_sample * s);

void
sample_refresh_views (sw_sample * s);

void
sample_start_marching_ants (sw_sample * s);

void
sample_stop_marching_ants (sw_sample * s);

void
sample_set_edit_state (sw_sample * s, sw_edit_state edit_state);

void
sample_set_edit_mode (sw_sample * s, sw_edit_mode edit_mode);

void
sample_refresh_playmode (sw_sample * s);

void
sample_set_previewing (sw_sample * s, gboolean previewing);

void
sample_set_stop_offset (sw_sample * s);

void
sample_set_playmarker (sw_sample * s, sw_framecount_t offset,
		       gboolean by_user);

void
sample_set_rec_marker (sw_sample * s, sw_framecount_t offset);

void
sample_set_scrubbing (sw_sample * s, gboolean scrubbing);

void
sample_set_looping (sw_sample * s, gboolean looping);

void
sample_set_playrev (sw_sample * s, gboolean reverse);

void
sample_set_mute (sw_sample * s, gboolean mute);

void
sample_set_color (sw_sample * s, gint color);

void
sample_set_progress_text (sw_sample * s, gchar * text);

void
sample_set_progress_percent (sw_sample * s, gint percent);

void
sample_refresh_progress_percent (sw_sample * s);

int
sample_set_progress_ready (sw_sample * s);

void
sample_set_tmp_message (sw_sample * s, const char * fmt, ...);

/*
 * sample_replace_throughout (os, s)
 *
 * Replaces os with s throughout the program, ie:
 *   - in the sample bank
 *   - in all os's views
 *
 * Destroys os.
 *
 */
void
sample_replace_throughout (sw_sample * os, sw_sample * s);


/* Selection handling */

gboolean
sample_offset_in_sel (sw_sample * s, sw_framecount_t offset);

guint
sample_sel_nr_regions (sw_sample * s);

void
sample_clear_selection (sw_sample * s);

void
sample_add_selection (sw_sample * s, sw_sel * sel);

sw_sel *
sample_add_selection_1 (sw_sample * s,
			sw_framecount_t start, sw_framecount_t end);

void
sample_set_selection (sw_sample * s, GList * gl);

sw_sel *
sample_set_selection_1 (sw_sample * s,
			sw_framecount_t start, sw_framecount_t end);


/* Modify a given existing selection */
void
sample_selection_modify (sw_sample * s, sw_sel * sel,
			 sw_framecount_t new_start, sw_framecount_t new_end);

void
sample_selection_invert (sw_sample * s);

void
sample_selection_select_all (sw_sample * s);

void
sample_selection_select_none (sw_sample * s);

void
sample_selection_halve (sw_sample * s);

void
sample_selection_double (sw_sample * s);

void
sample_selection_shift_left (sw_sample * s);

void
sample_selection_shift_right (sw_sample * s);

/* info dialog */
void
sample_show_info_dialog (sw_sample * sample);


#endif /* __SWEEP_SAMPLE_H__ */
