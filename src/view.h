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

#ifndef __VIEW_H__
#define __VIEW_H__

#include <gtk/gtk.h>

#include "sweep_types.h"
#include "sweep_app.h"

sw_view *
view_new(sw_sample * sample, sw_framecount_t start,
	 sw_framecount_t end,gfloat vol);

sw_view *
view_new_all(sw_sample * sample, gfloat vol);

void
view_set_ends (sw_view * view, sw_framecount_t start, sw_framecount_t end);

void
view_set_playmarker (sw_view * view, int offset);

void
view_close(sw_view * view);

void
view_volume_increase (sw_view * view);

void
view_volume_decrease (sw_view * view);

void
view_refresh_title(sw_view * view);

void
view_default_status (sw_view * view);

void
view_refresh_hruler (sw_view * v);

void
view_refresh_display (sw_view * v);

void
view_refresh_adjustment (sw_view * v);

void
view_refresh (sw_view * v);

void
view_fix_adjustment (sw_view * v);

void
view_sink_last_tmp_view (void);

void
view_clear_last_tmp_view (void);

#endif /* __VIEW_H__ */
