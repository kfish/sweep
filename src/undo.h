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

#ifndef __UNDO_H__
#define __UNDO_H__

#include "sweep.h"

sw_op_instance *
sw_op_instance_new (char * desc, sw_operation * op);

void
register_operation (sw_sample * s, sw_op_instance * inst);

void
undo_current (sw_sample * s);

void
redo_current (sw_sample * s);


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

typedef struct _paste_over_data paste_over_data;

struct _paste_over_data {
  sw_sample * sample;
  edit_buffer * old_eb;
  edit_buffer * new_eb;
};

paste_over_data *
paste_over_data_new (edit_buffer * old_eb, edit_buffer * new_eb);

void
paste_over_data_destroy (paste_over_data * p);

void
undo_by_paste_over (sw_sample * s, paste_over_data * p);

void
redo_by_paste_over (sw_sample * s, paste_over_data * p);


typedef struct _splice_data splice_data;

struct _splice_data {
  sw_sample * sample;
  edit_buffer * eb;
};

splice_data *
splice_data_new (edit_buffer * eb);

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


#endif /* __UNDO_H__ */
