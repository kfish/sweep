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

#ifndef __EDIT_H__
#define __EDIT_H__

#include "sweep_types.h"

edit_buffer *
edit_buffer_from_sample (sw_sample * sample);

void
edit_buffer_destroy (edit_buffer * eb);

sw_sdata *
splice_out_sel (sw_sdata * sdata);

sw_sdata *
splice_in_eb (sw_sdata * sdata, edit_buffer * eb);

sw_sample *
paste_over (sw_sample * sample, edit_buffer * eb);

void
do_copy (sw_sample * sample);

sw_op_instance *
do_cut (sw_sample * sample);

sw_op_instance *
do_clear (sw_sample * sample);

sw_op_instance *
do_delete (sw_sample * sample);

sw_op_instance *
do_paste_in (sw_sample * in, sw_sample ** out);

sw_op_instance *
do_paste_at (sw_sample * sample);

sw_op_instance *
do_paste_over (sw_sample * in, sw_sample ** out);

void
do_paste_as_new (sw_sample * in, sw_sample ** out);

#endif /* __EDIT_H__ */
