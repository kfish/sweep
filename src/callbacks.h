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

#ifndef __CALLBACKS_H__
#define __CALLBACKS_H__

#include <gtk/gtk.h>

void
sample_new_empty_cb (GtkWidget * widget, gpointer data);

void
sample_new_copy_cb (GtkWidget * widget, gpointer data);

void
view_new_all_cb (GtkWidget * widget, gpointer data);

void
view_close_cb (GtkWidget * widget, gpointer data);

void
exit_cb (GtkWidget * widget, gpointer data);

void
set_tool_cb (GtkWidget * widget, gpointer data);

void
zoom_in_cb (GtkWidget * widget, gpointer data);

void
zoom_out_cb (GtkWidget * widget, gpointer data);

void
zoom_to_sel_cb (GtkWidget * widget, gpointer data);

void
zoom_left_cb (GtkWidget * widget, gpointer data);

void
zoom_right_cb (GtkWidget * widget, gpointer data);

void
zoom_1_1_cb (GtkWidget * widget, gpointer data);

void
zoom_2_1_cb (GtkWidget * widget, gpointer data);

void
zoom_norm_cb (GtkWidget * widget, gpointer data);

void
zoom_1to1_cb (GtkWidget * widget, gpointer data);

void
play_view_cb (GtkWidget * widget, gpointer data);

void
play_view_all_loop_cb (GtkWidget * widget, gpointer data);

void
play_view_sel_cb (GtkWidget * widget, gpointer data);

void
play_view_sel_loop_cb (GtkWidget * widget, gpointer data);

void
stop_playback_cb (GtkWidget * widget, gpointer data);

void
sd_sel_changed_cb (GtkWidget * widget);

void
sd_win_changed_cb (GtkWidget * widget);

void
adj_changed_cb (GtkWidget * widget, gpointer data);

void
select_invert_cb (GtkWidget * widget, gpointer data);

void
select_all_cb (GtkWidget * widget, gpointer data);

void
select_none_cb (GtkWidget * widget, gpointer data);

void
undo_cb (GtkWidget * widget, gpointer data);

void
redo_cb (GtkWidget * widget, gpointer data);

void
copy_cb (GtkWidget * widget, gpointer data);

void
cut_cb (GtkWidget * widget, gpointer data);

void
clear_cb (GtkWidget * widget, gpointer data);

void
delete_cb (GtkWidget * widget, gpointer data);

void
paste_cb (GtkWidget * widget, gpointer data);

void
paste_as_new_cb (GtkWidget * widget, gpointer data);

void
reverse_cb (GtkWidget * widget, gpointer data);

void
normalise_cb (GtkWidget * widget, gpointer data);

#endif /* __CALLBACKS_H__ */
