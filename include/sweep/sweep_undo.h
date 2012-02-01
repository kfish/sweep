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

#ifndef __SWEEP_UNDO_H__
#define __SWEEP_UNDO_H__

#include "sweep_types.h"

gint
update_edit_progress (gpointer data);

sw_op_instance *
sw_op_instance_new (sw_sample * sample, const char * description,
		    sw_operation * operation);

void
schedule_operation (sw_sample * sample, const char * description,
		    sw_operation * operation, void * do_data);

void
register_operation (sw_sample * s, sw_op_instance * inst);

void
trim_registered_ops (sw_sample * s, int length);

void
undo_current (sw_sample * s);

void
redo_current (sw_sample * s);

void
revert_op (sw_sample * sample, GList * op_gl);

void
set_active_op (sw_sample * s, sw_op_instance * inst);

void
cancel_active_op (sw_sample * s);

/* Stock undo functions */

#if 0
typedef struct _replace_data replace_data;

struct _replace_data {
  sw_sample * old_sample;
  sw_sample * new_sample;
};

replace_data *
replace_data_new (sw_sample * old_sample, sw_sample * new_sample);

void
undo_by_replace (replace_data * r);

void
redo_by_replace (replace_data * r);
#endif

typedef struct _sounddata_replace_data sounddata_replace_data;

struct _sounddata_replace_data {
  sw_sample * sample;
  sw_sounddata * old_sounddata;
  sw_sounddata * new_sounddata;
};

sounddata_replace_data *
sounddata_replace_data_new (sw_sample * sample,
			    sw_sounddata * old_sounddata,
			    sw_sounddata * new_sounddata);

void
sounddata_replace_data_destroy (sounddata_replace_data * sr);

void
undo_by_sounddata_replace (sw_sample * s, sounddata_replace_data * sr);

void
redo_by_sounddata_replace (sw_sample * s, sounddata_replace_data * sr);


typedef struct _paste_over_data paste_over_data;

struct _paste_over_data {
  sw_sample * sample;
  sw_edit_buffer * old_eb;
  sw_edit_buffer * new_eb;
};

paste_over_data *
paste_over_data_new (sw_edit_buffer * old_eb, sw_edit_buffer * new_eb);

void
paste_over_data_destroy (paste_over_data * p);

void
undo_by_paste_over (sw_sample * s, paste_over_data * p);

void
redo_by_paste_over (sw_sample * s, paste_over_data * p);


typedef struct _splice_data splice_data;

struct _splice_data {
  sw_sample * sample;
  sw_edit_buffer * eb;
  GList * sels; /* Previous sels of sounddata */
};

splice_data *
splice_data_new (sw_edit_buffer * eb, GList * sels);

void
splice_data_destroy (splice_data * s);

void
undo_by_splice_in (sw_sample * s, splice_data * sp);

void
redo_by_splice_out (sw_sample * s, splice_data * sp);

void
undo_by_splice_out (sw_sample * s, splice_data * sp);

void
redo_by_splice_in (sw_sample * s, splice_data * sp);

void
undo_by_splice_over (sw_sample * s, splice_data * sp);

void
redo_by_splice_over (sw_sample * s, splice_data * sp);

void
undo_by_crop_in (sw_sample * s, splice_data * sp);

void
redo_by_crop_out (sw_sample * s, splice_data * sp);

#endif /* __SWEEP_UNDO_H__ */
