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


#ifndef __SWEEP_FILTER_H__
#define __SWEEP_FILTER_H__

typedef void (*SweepFilterRegion) (gpointer data,
				   sw_format * format, int nr_frames,
				   sw_param_set pset, gpointer custom_data);

typedef sw_soundfile * (*SweepFilter) (sw_soundfile * soundfile,
				       sw_param_set pset,
				       gpointer custom_data);

sw_op_instance *
register_filter_region_op (sw_sample * sample, char * desc,
			   SweepFilterRegion func, sw_param_set pset,
			   gpointer custom_data);

sw_op_instance *
register_filter_op (sw_sample * sample, char * desc, SweepFilter func,
		    sw_param_set pset, gpointer custom_data);


#endif /* __SWEEP_FILTER_H__ */
