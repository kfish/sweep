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
sample_new_empty(char * directory, char * filename,
		      gint nr_channels, gint sample_rate, gint sample_length);

sw_sample *
sample_new_copy(sw_sample * s);

void
sample_destroy (sw_sample * s);

void
sample_set_pathname (sw_sample * s, char * directory, char * filename);

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
sample_set_playmarker (sw_sample * s, int offset);

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

#endif /* __SWEEP_SAMPLE_H__ */
