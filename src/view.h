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

#include <sweep/sweep_types.h>
#include "sweep_app.h"

sw_view *
view_new(sw_sample * sample, sw_framecount_t start,
	 sw_framecount_t end, gfloat gain);

sw_view *
view_new_all(sw_sample * sample, gfloat gain);

void
view_popup_context_menu (sw_view * view, guint button, guint32 activate_time);

void
view_set_ends (sw_view * view, sw_framecount_t start, sw_framecount_t end);

void
view_center_on (sw_view * view, sw_framecount_t offset);

void
view_zoom_length (sw_view * view, sw_framecount_t length);

void
view_zoom_normal (sw_view * view);

void
view_zoom_in (sw_view * view, double ratio);

void
view_zoom_out (sw_view * view, double ratio);

void
view_zoom_to_sel (sw_view * view);

void
view_zoom_left (sw_view * view);

void
view_zoom_right (sw_view * view);

void
view_zoom_all (sw_view * view);

void
view_vzoom_in (sw_view * view, double ratio);

void
view_vzoom_out (sw_view * view, double ratio);

void
view_refresh_edit_mode (sw_view * view);

#if 0
void
view_set_play_marker (sw_view * view, int play_offset);

void
view_set_user_marker (sw_view * view, int user_offset);
#endif

void
view_refresh_playmode (sw_view * view);

void
view_set_following (sw_view * view, gboolean following);

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
view_refresh_tool_buttons (sw_view * v);

void
view_refresh_offset_indicators (sw_view * v);

void
view_refresh_rec_offset_indicators (sw_view * view);

void
view_set_progress_text (sw_view * view, gchar * text);

void
view_set_progress_percent (sw_view * view, gint percent);

void
view_set_progress_ready (sw_view * view);

void
view_set_tmp_message (sw_view * view, gchar * message);

void
view_refresh_hruler (sw_view * v);

void
view_refresh_display (sw_view * v);

void
view_refresh_adjustment (sw_view * v);

void
view_refresh (sw_view * v);

void
view_refresh_looping (sw_view * v);

void
view_refresh_playrev (sw_view * view);

void
view_refresh_mute (sw_view * view);

void
view_refresh_monitor (sw_view * view);

void
view_fix_adjustment (sw_view * v);

void
view_sink_last_tmp_view (void);

void
view_clear_last_tmp_view (void);

#endif /* __VIEW_H__ */
