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

#ifndef __SWEEP_SOUNDDATA_H__
#define __SWEEP_SOUNDDATA_H__

sw_sounddata *
sounddata_new_empty(gint nr_channels, gint sample_rate, gint sample_length);

void
sounddata_destroy (sw_sounddata * sounddata);

void
sounddata_lock_selection (sw_sounddata * sounddata);

void
sounddata_unlock_selection (sw_sounddata * sounddata);

void
sounddata_clear_selection (sw_sounddata * sounddata);

/*
 * sounddata_normalise_selection(sounddata)
 *
 * normalise the selection of sounddata, ie. make sure there's
 * no overlaps and merge adjoining sections.
 */

void
sounddata_normalise_selection (sw_sounddata * sounddata);

void
sounddata_add_selection (sw_sounddata * sounddata, sw_sel * sel);

sw_sel *
sounddata_add_selection_1 (sw_sounddata * sounddata,
			   sw_framecount_t start, sw_framecount_t end);

sw_sel *
sounddata_set_selection_1 (sw_sounddata * sounddata,
			   sw_framecount_t start, sw_framecount_t end);

guint
sounddata_selection_nr_regions (sw_sounddata * sounddata);

gint
sounddata_selection_nr_frames (sw_sounddata * sounddata);

gint
sounddata_selection_width (sw_sounddata * sounddata);

void
sounddata_selection_translate (sw_sounddata * sounddata, gint delta);

void
sounddata_selection_scale (sw_sounddata * sounddata, gfloat scale);

/*
 * sounddata_copyin_selection (sounddata, sounddata2)
 *
 * copies the selection of sounddata1 into sounddata2. If sounddata2 previously
 * had a selection, the two are merged.
 */
void
sounddata_copyin_selection (sw_sounddata * sounddata1,
			    sw_sounddata * sounddata2);


#endif /* __SWEEP_SOUNDDATA_H__ */
