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
#include "sweep-scheme.h"

void
sample_new_empty_cb (GtkWidget * widget, gpointer data);

void
sample_new_copy_cb (GtkWidget * widget, gpointer data);

void
view_new_all_cb (GtkWidget * widget, gpointer data);

void
view_new_cb (GtkWidget * widget, gpointer data);

void
view_close_cb (GtkWidget * widget, gpointer data);

void
exit_cb (GtkWidget * widget, gpointer data);

void
set_tool_cb (GtkWidget * widget, gpointer data);

void
view_set_tool_cb (GtkWidget * widget, gpointer data);

void
repeater_released_cb (GtkWidget * widget, gpointer data);

void
zoom_in_cb (GtkWidget * widget, gpointer data);

void
zoom_out_cb (GtkWidget * widget, gpointer data);

void
zoom_in_pressed_cb (GtkWidget * widget, gpointer data);

void
zoom_out_pressed_cb (GtkWidget * widget, gpointer data);

void
zoom_to_sel_cb (GtkWidget * widget, gpointer data);

void
zoom_left_cb (GtkWidget * widget, gpointer data);

void
zoom_right_cb (GtkWidget * widget, gpointer data);

void
zoom_all_cb (GtkWidget * widget, gpointer data);

void
zoom_2_1_cb (GtkWidget * widget, gpointer data);

void
zoom_norm_cb (GtkWidget * widget, gpointer data);

void
zoom_1to1_cb (GtkWidget * widget, gpointer data);

void
zoom_center_cb (GtkWidget * widget, gpointer data);

void
zoom_combo_changed_cb (GtkWidget * widget, gpointer data);

void
sample_set_color_cb (GtkWidget * widget, gpointer data);

void
device_config_cb (GtkWidget * widget, gpointer data);

void
follow_toggled_cb (GtkWidget * widget, gpointer data);

void
follow_toggle_cb (GtkWidget * widget, gpointer data);

void
loop_toggled_cb (GtkWidget * widget, gpointer data);

void
loop_toggle_cb (GtkWidget * widget, gpointer data);

void
playrev_toggled_cb (GtkWidget * widget, gpointer data);

void
playrev_toggle_cb (GtkWidget * widget, gpointer data);

void
mute_toggled_cb (GtkWidget * widget, gpointer data);

void
mute_toggle_cb (GtkWidget * widget, gpointer data);

void
monitor_toggled_cb (GtkWidget * widget, gpointer data);

void
monitor_toggle_cb (GtkWidget * widget, gpointer data);

void
play_view_button_cb (GtkWidget * widget, gpointer data);

void
play_view_sel_button_cb (GtkWidget * widget, gpointer data);

void
play_view_cb (GtkWidget * widget, gpointer data);

void
play_view_all_once_cb (GtkWidget * widget, gpointer data);

void
play_view_all_loop_cb (GtkWidget * widget, gpointer data);

void
play_view_sel_cb (GtkWidget * widget, gpointer data);

void
play_view_sel_once_cb (GtkWidget * widget, gpointer data);

void
play_view_sel_loop_cb (GtkWidget * widget, gpointer data);

void
pause_playback_cb (GtkWidget * widget, gpointer data);

void
stop_playback_cb (GtkWidget * widget, gpointer data);

void
preview_cut_cb (GtkWidget * widget, gpointer data);

void
preroll_cb (GtkWidget * widget, gpointer data);

void
show_rec_dialog_cb (GtkWidget * widget, gpointer data);

void
page_back_cb (GtkWidget * widget, gpointer data);

void
page_fwd_cb (GtkWidget * widget, gpointer data);

void
rewind_pressed_cb (GtkWidget * widget, gpointer data);

void
ffwd_pressed_cb (GtkWidget * widget, gpointer data);

void
goto_start_cb (GtkWidget * widget, gpointer data);

void
goto_start_of_view_cb (GtkWidget * widget, gpointer data);

void
goto_end_of_view_cb (GtkWidget * widget, gpointer data);

void
goto_end_cb (GtkWidget * widget, gpointer data);

void
sd_sel_changed_cb (GtkWidget * widget);

void
sd_win_changed_cb (GtkWidget * widget);

void
adj_changed_cb (GtkWidget * widget, gpointer data);

void
adj_value_changed_cb (GtkWidget * widget, gpointer data);

void
select_invert_cb (GtkWidget * widget, gpointer data);

void
select_all_cb (GtkWidget * widget, gpointer data);

void
select_none_cb (GtkWidget * widget, gpointer data);

void
selection_halve_cb (GtkWidget * widget, gpointer data);

void
selection_double_cb (GtkWidget * widget, gpointer data);

void
select_shift_left_cb (GtkWidget * widget, gpointer data);

void
select_shift_right_cb (GtkWidget * widget, gpointer data);

void
show_undo_dialog_cb (GtkWidget * widget, gpointer data);

void
undo_cb (GtkWidget * widget, gpointer data);

void
redo_cb (GtkWidget * widget, gpointer data);

void
cancel_cb (GtkWidget * widget, gpointer data);

void
copy_cb (GtkWidget * widget, gpointer data);

void
cut_cb (GtkWidget * widget, gpointer data);

void
clear_cb (GtkWidget * widget, gpointer data);

void
delete_cb (GtkWidget * widget, gpointer data);

void
crop_cb (GtkWidget * widget, gpointer data);

void
paste_cb (GtkWidget * widget, gpointer data);

void
paste_mix_cb (GtkWidget * widget, gpointer data);

void
paste_xfade_cb (GtkWidget * widget, gpointer data);

void
paste_as_new_cb (GtkWidget * widget, gpointer data);

void
reverse_cb (GtkWidget * widget, gpointer data);

void
normalise_cb (GtkWidget * widget, gpointer data);

void
show_info_dialog_cb (GtkWidget * widget, gpointer data);

void 
close_window_cb(GtkAccelGroup *accel_group,
                                             GObject *acceleratable,
                                             guint keyval,
                                             GdkModifierType modifier);
											 
void 
hide_window_cb(GtkAccelGroup *accel_group,
                                             GObject *acceleratable,
                                             guint keyval,
                                             GdkModifierType modifier);
											 
void  
hack_max_label_width_cb (GtkWidget *widget,
							GtkStyle *previous_style,
                            gpointer user_data);
							
void  
hack_max_combo_width_cb (GtkWidget *widget,
							GtkStyle *previous_style,
                            gpointer user_data);
#if GTK_CHECK_VERSION(2, 10, 0)
void
recent_chooser_menu_activated_cb(GtkRecentChooser *chooser,
                                 gpointer          user_data);
#endif

void
scheme_ed_new_clicked_cb (GtkButton *button, gpointer user_data);
void
scheme_ed_copy_clicked_cb (GtkButton *button, gpointer user_data);
void
scheme_ed_delete_clicked_cb (GtkButton *button, gpointer user_data);
void
scheme_ed_ok_clicked_cb (GtkButton *button, gpointer user_data);
void
scheme_ed_combo_changed_cb (GtkComboBox *widget, gpointer user_data);
gboolean 
schemes_ed_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data);
void
schemes_ed_destroy_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data);
void
schemes_ed_radio_toggled_cb (GtkToggleButton *togglebutton, gpointer user_data);

void
schemes_ed_color_changed_cb  (GtkColorSelection *colorselection,
                              gpointer user_data);

void
schemes_ed_treeview_selection_changed_cb (GtkTreeSelection *treeselection,
                                         gpointer user_data);


#endif /* __CALLBACKS_H__ */
