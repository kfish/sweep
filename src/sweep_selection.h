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

#ifndef __SWEEP_SELECTION_H__
#define __SWEEP_SELECTION_H__

sw_sel *
sel_new (sw_framecount_t start, sw_framecount_t end);

sw_sel *
sel_copy (sw_sel * sel);

/*
 * sel_cmp (s1, s2)
 *
 * Compares two sw_sel's for g_list_insert_sorted() --
 * return > 0 if s1 comes after s2 in the sort order.
 */
gint
sel_cmp (sw_sel * s1, sw_sel * s2);

/*
 * sels_copy (sels)
 *
 * returns a copy of sels
 */
GList *
sels_copy (GList * sels);

sw_op_instance *
sample_register_sel_op (sw_sample * s, char * desc, SweepModify func,
			sw_param_set pset, gpointer custom_data);

#endif /* __SWEEP_SELECTION_H__ */
