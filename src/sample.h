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

#ifndef __SAMPLE_H__
#define __SAMPLE_H__

#include <sweep/sweep_types.h>
#include "sweep_app.h"

void
create_sample_new_dialog_defaults (char * pathname);

void
create_sample_new_dialog_like (sw_sample * s);

void
sample_add_view (sw_sample * s, sw_view * v);

void
sample_remove_view (sw_sample * s, sw_view * v);

gint
sample_clear_tmp_message (gpointer data);

void
sample_set_monitor (sw_sample * s, gboolean monitor);

void
sample_set_offset_next_bound_left (sw_sample * s);

void
sample_set_offset_next_bound_right (sw_sample * s);

void
sample_refresh_rec_marker (sw_sample * s);

/*
 * Functions to handle the temporary selection.
 */

void
sample_clear_tmp_sel (sw_sample * s);

/* sample_set_tmp_sel (s, tsel)
 *
 * sets the tmp_sel of sample s to a list containing only tsel.
 * If tsel was part of the actual selection of s, it is first
 * removed from the selection.
 */
void
sample_set_tmp_sel (sw_sample * s, sw_view * tview, sw_sel * tsel);

void
sample_set_tmp_sel_1 (sw_sample * s, sw_view * tview,
			   sw_framecount_t start, sw_framecount_t end);

void
sample_selection_insert_tmp_sel (sw_sample * s);

void
sample_selection_subtract_tmp_sel (sw_sample * s);

void
sample_selection_replace_with_tmp_sel (sw_sample * s);

#endif /* __SAMPLE_H__ */
