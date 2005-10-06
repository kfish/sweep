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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "callbacks.h"

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_types.h>
#include <sweep/sweep_undo.h>
#include <sweep/sweep_sample.h>
#include <sweep/sweep_typeconvert.h>

#include "sample.h"
#include "interface.h"
#include "edit.h"
#include "head.h"
#include "sample-display.h"
#include "play.h"
#include "driver.h"
#include "undo_dialog.h"
#include "paste_dialogs.h"
#include "print.h"
#include "record.h"

/*
 * Default zooming parameters.
 *
 * DEFAULT_ZOOM sets the ratio for zooming. Each time zoom_in is
 * called, the view is adjusted to be 1/DEFAULT_ZOOM its current
 * length. Each time zoom_out is called, the view is adjusted to
 * DEFAULT_ZOOM times its current length.
 * 
 * If you alter this, make sure
 * (DEFAULT_ZOOM - 1) * DEFAULT_MIN_ZOOM  >  1
 *
 * where DEFAULT_MIN_ZOOM is defined in view.c
 */
#define DEFAULT_ZOOM 2.0

#define ZOOM_FRAMERATE 10

/* Sample creation */

void
sample_new_empty_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;

  if (view == NULL) {
    create_sample_new_dialog_defaults (NULL);
  } else {
    create_sample_new_dialog_like (view->sample);
  }
}

void
sample_new_copy_cb (GtkWidget * widget, gpointer data)
{
  sw_sample * ns;
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_sample * s;
  sw_view * v;

  s = sd->view->sample;

  if(!s) return;
  
  ns = sample_new_copy(s);

  while (ns->color == s->color) {
    ns->color = (random()) % VIEW_COLOR_MAX;
  }

  v = view_new_all (ns, 1.0);
  sample_add_view (ns, v);

  sample_bank_add(ns);
}

/* Generic repeater */

static void
view_init_repeater (sw_view * view, GtkFunction function, gpointer data)
{
  function (data);
  if (view->repeater_tag <= 0) {
    view->repeater_tag = g_timeout_add ((guint32)1000/ZOOM_FRAMERATE,
					  (GSourceFunc) function, data);
  }
}

void
repeater_released_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;

  if (view->repeater_tag > 0) {
    g_source_remove(view->repeater_tag);
    view->repeater_tag = 0;
  }
}

/* View */

void
view_new_all_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_sample * s;
  sw_view * v;

  s = sd->view->sample;
  v = view_new_all(s, 1.0);

  sample_add_view (s, v);
}

void
view_new_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_sample * s;
  sw_view * view, * v;

  v = sd->view;
  s = v->sample;

  view = view_new (s, v->start, v->end, s->play_head->gain);

  sample_add_view (s, view);
}

void
view_close_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_view * v;

  v = sd->view;

  view_close(v);
}

void
exit_cb (GtkWidget * widget, gpointer data)
{
  sweep_quit ();
}

/* Tools */
void
set_tool_cb (GtkWidget * widget, gpointer data)
{
#if 0
  gint tool = (gint)data;

  /*  current_tool = tool;*/
#endif
  g_print ("NOOOOOOOOOOOOOO global current_tool\n");
}

void
view_set_tool_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_tool_t tool;

  tool = (sw_tool_t)
    GPOINTER_TO_INT(g_object_get_data (G_OBJECT(widget), "default"));

  view->current_tool = tool;

  view_refresh_tool_buttons (view);
}

/* Zooming */

static gboolean
zoom_in_step (gpointer data)
{
  sw_view * view = (sw_view *)data;

  view_zoom_in (view, DEFAULT_ZOOM);

  return TRUE;
}

void
zoom_in_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  view_zoom_in (view, DEFAULT_ZOOM);
}

static gboolean
zoom_out_step (gpointer data)
{
  sw_view * view = (sw_view *)data;

  view_zoom_out (view, DEFAULT_ZOOM);

  return TRUE;
}

void
zoom_out_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  view_zoom_out (view, DEFAULT_ZOOM);
}


void
zoom_in_pressed_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;

  view_zoom_in (view, DEFAULT_ZOOM);
  view_init_repeater (view, (GtkFunction)zoom_in_step, data);
}

void
zoom_out_pressed_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;

  view_zoom_out (view, DEFAULT_ZOOM);
  view_init_repeater (view, (GtkFunction)zoom_out_step, data);
}

void
zoom_to_sel_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_view * v = sd->view;

  view_zoom_to_sel (v);
}

void
zoom_left_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_view * v = sd->view;

  view_zoom_left (v);
}

void
zoom_right_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_view * v = sd->view;

  view_zoom_right (v);
}

void
zoom_all_cb (GtkWidget * widget, gpointer data)
{
  sw_view * v = (sw_view *)data;

  view_zoom_all (v);
}

/* XXX: dump this ?? */
void
zoom_2_1_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_sample * s;

  s = sd->view->sample;

  view_set_ends(sd->view,
		s->sounddata->nr_frames / 4,
		3 * s->sounddata->nr_frames / 4);
}

void
zoom_norm_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_view * view = sd->view;

  view_zoom_normal (view);
}

void
zoom_1to1_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);

  view_zoom_length (sd->view, sd->width);
  view_center_on (sd->view, sd->view->sample->user_offset);
}

void
zoom_center_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);

  view_center_on (sd->view, sd->view->sample->user_offset);
}

void
zoom_combo_changed_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  gchar * text;
  sw_time_t zoom_time;
  sw_framecount_t zoom_length;

  text = (gchar *) gtk_entry_get_text (GTK_ENTRY(widget));

  if (!strcmp (text, "All")) {
    view_zoom_all (view);
  } else {
    zoom_time = (sw_time_t)strtime_to_seconds (text);
    if (zoom_time != -1.0) {
      zoom_length =
	time_to_frames (view->sample->sounddata->format, zoom_time);

      view_zoom_length (view, zoom_length);
      
      /* Work around a bug, probably in gtkcombo, whereby all the other
       * widgets sometimes lost input after choosing a zoom */
      gtk_widget_grab_focus (view->display);
    }
  }
}

/* Device config */

void
device_config_cb (GtkWidget * widget, gpointer data)
{
  device_config ();
}

/* Sample */

void
sample_set_color_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *) data;
  gint color;

  color = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(widget), "default"));
  sample_set_color (view->sample, color);
}

/* Playback */

void
follow_toggled_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *) data;
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
  view_set_following (view, active);
}

void
follow_toggle_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  view_set_following (view, !view->following);
}

void
loop_toggled_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *) data;
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
  sample_set_looping (view->sample, active);
}

void
loop_toggle_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sample_set_looping (view->sample, !view->sample->play_head->looping);
}

void
playrev_toggled_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *) data;
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
  sample_set_playrev (view->sample, active);
}

void
playrev_toggle_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sample_set_playrev (view->sample, !view->sample->play_head->reverse);
}

void
mute_toggled_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *) data;
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
  sample_set_mute (view->sample, active);
}

void
mute_toggle_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sample_set_mute (view->sample, !view->sample->play_head->mute);
}

void
monitor_toggled_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *) data;
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
  sample_set_monitor (view->sample, active);
}

void
monitor_toggle_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sample_set_monitor (view->sample, !view->sample->play_head->monitor);
}

void
play_view_button_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *) data;
  sw_head * head = view->sample->play_head;
  gboolean was_restricted = head->restricted;

  head_set_rate (head, 1.0);
  /*  head_set_restricted (head, FALSE);*/

  if (head->going && (head->scrubbing || !was_restricted)) {
    pause_playback_cb (widget, data);
  } else {
    play_view_all (view);
  }
}

void
play_view_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *) data;
  sw_head * head = view->sample->play_head;

  head_set_rate (head, 1.0);
  /*  head_set_restricted (head, FALSE);*/

  if (head->going) {
    pause_playback_cb (widget, data);
  } else {
    play_view_all (view);
  }
}

void
play_view_sel_button_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *) data;
  sw_head * head = view->sample->play_head;
  gboolean was_restricted = head->restricted;

  if (view->sample->sounddata->sels == NULL) {
    play_view_cb (widget, data);
    return;
  }

  head_set_rate (head, 1.0);
  /*  head_set_restricted (head, TRUE);*/

  if (head->going && (head->scrubbing || was_restricted)) {
    pause_playback_cb (widget, data);
  } else {
    play_view_sel (view);
  }
}

void
play_view_sel_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *) data;
  sw_head * head = view->sample->play_head;

  if (view->sample->sounddata->sels == NULL) {
    play_view_cb (widget, data);
    return;
  }

  head_set_rate (head, 1.0);
  /*  head_set_restricted (head, TRUE);*/

  if (head->going) {
    pause_playback_cb (widget, data);
  } else {
    play_view_sel (view);
  }
}

void
pause_playback_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;

  pause_playback (view->sample);
}

void
stop_playback_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;

  stop_playback (view->sample);
}

void
preview_cut_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *) data;

  sample_refresh_playmode (view->sample);

  if (view->sample->sounddata->sels != NULL) {
    play_preview_cut (view);
  } else {
    play_preroll (view);
  }
}

void
preroll_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *) data;

  sample_refresh_playmode (view->sample);
  
  play_preroll (view);
}

/* Record */

void
show_rec_dialog_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;

  rec_dialog_create (view->sample);
}

/* Transport */

static gboolean
rewind_step (gpointer data)
{
  sw_sample * sample = (sw_sample *)data;
  gint step;

  step = sample->sounddata->format->rate; /* 1 second */
  sample_set_playmarker (sample, sample->user_offset - step, TRUE);

  return TRUE;
}

static gboolean
ffwd_step (gpointer data)
{
  sw_sample * sample = (sw_sample *)data;
  gint step;

  step = sample->sounddata->format->rate; /* 1 second */
  sample_set_playmarker (sample, sample->user_offset + step, TRUE);

  return TRUE;
}

void
page_back_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;
  gint step;

  step = MIN(sample->sounddata->format->rate, (view->end - view->start));
  sample_set_playmarker (sample, sample->user_offset - step, TRUE);
}

void
page_fwd_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;
  gint step;

  step = MIN(sample->sounddata->format->rate, (view->end - view->start));
  sample_set_playmarker (sample, sample->user_offset + step, TRUE);
}

void
rewind_pressed_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;

  view_init_repeater (view, (GtkFunction)rewind_step, sample);
}

void
ffwd_pressed_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;

  view_init_repeater (view, (GtkFunction)ffwd_step, sample);
}

void
goto_start_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;

  sample_set_scrubbing (sample, FALSE);
  sample_set_playmarker (sample, 0, TRUE);
}

void
goto_start_of_view_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;

  sample_set_scrubbing (sample, FALSE);
  sample_set_playmarker (sample, view->start, TRUE);
}

void
goto_end_of_view_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;

  sample_set_scrubbing (sample, FALSE);
  sample_set_playmarker (sample, view->end, TRUE);
}

void
goto_end_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;

  sample_set_scrubbing (sample, FALSE);
  sample_set_playmarker (sample, sample->sounddata->nr_frames, TRUE);
}

/* Interface elements: sample display, scrollbars etc. */

void
sd_sel_changed_cb (GtkWidget * widget)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(widget);

  sample_refresh_views (sd->view->sample);
}

void
sd_win_changed_cb (GtkWidget * widget)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(widget);
  sw_view * v = sd->view;

  g_signal_handlers_block_matched (GTK_OBJECT(v->adj), G_SIGNAL_MATCH_DATA, 0,
									0, 0, 0, v);
  view_fix_adjustment (sd->view);
	
  g_signal_handlers_unblock_matched (GTK_OBJECT(v->adj), G_SIGNAL_MATCH_DATA, 0,
								0, 0, 0, v); 
  view_refresh_hruler (v);
}

void
adj_changed_cb (GtkWidget * widget, gpointer data)
{
  sw_view * v = (sw_view *)data;
  SampleDisplay * sd = SAMPLE_DISPLAY(v->display);
  GtkAdjustment * adj = GTK_ADJUSTMENT(v->adj);

  g_signal_handlers_block_matched (GTK_OBJECT(GTK_OBJECT(sd)), G_SIGNAL_MATCH_DATA, 0,
								0, 0, 0, v);
  sample_display_set_window(sd,
			    (gint)adj->value,
			    (gint)(adj->value + adj->page_size));
  
  g_signal_handlers_unblock_matched (GTK_OBJECT(GTK_OBJECT(sd)), G_SIGNAL_MATCH_DATA, 0,
								0, 0, 0, v);
  view_refresh_hruler (v);
}

void
adj_value_changed_cb (GtkWidget * widget, gpointer data)
{
  sw_view * v = (sw_view *)data;
  SampleDisplay * sd = SAMPLE_DISPLAY(v->display);
  GtkAdjustment * adj = GTK_ADJUSTMENT(v->adj);

  if (!v->sample->play_head->going || !v->following) {

    g_signal_handlers_block_matched (GTK_OBJECT(GTK_OBJECT(sd)), G_SIGNAL_MATCH_DATA, 0,
								0, 0, 0, v);
    sample_display_set_window(sd,
			      (gint)adj->value,
			      (gint)(adj->value + adj->page_size));
    g_signal_handlers_unblock_matched (GTK_OBJECT(GTK_OBJECT(sd)), G_SIGNAL_MATCH_DATA, 0,
								0, 0, 0, v);
  }

  if (v->following) {
    if (v->sample->user_offset < adj->value ||
	v->sample->user_offset > adj->value + adj->page_size)
      sample_set_playmarker (v->sample, adj->value + adj->page_size/2, TRUE);
  }

  view_refresh_hruler (v);  
}

/* Selection */

void
select_invert_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);

  sample_selection_invert (sd->view->sample);
}

void
select_all_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);

  sample_selection_select_all (sd->view->sample);
}

void
select_none_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);

  sample_selection_select_none (sd->view->sample);
}

void
selection_halve_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);

  sample_selection_halve (sd->view->sample);
}

void
selection_double_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);

  sample_selection_double (sd->view->sample);
}

void
select_shift_left_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);

  sample_selection_shift_left (sd->view->sample);
}

void
select_shift_right_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);

  sample_selection_shift_right (sd->view->sample);
}

/* Undo / Redo */

void
show_undo_dialog_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;

  undo_dialog_create (view->sample);
}

void
undo_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;

  undo_current (view->sample);
}

void
redo_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;

  redo_current (view->sample);
}

void
cancel_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;

  cancel_active_op (view->sample);
}

/* Edit */

void
copy_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * s = view->sample;

  do_copy (s);
}

void
cut_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * s = view->sample;

  do_cut (s);
}

void
clear_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * s = view->sample;

  do_clear (s);
}

void
delete_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * s = view->sample;

  do_delete (s);
}

void
crop_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * s = view->sample;

  do_crop (s);
}

void
paste_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * s = view->sample;

  do_paste_insert (s);
}

void
paste_mix_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * s = view->sample;

  if (clipboard_width() > 0) {
    create_paste_mix_dialog (s);
  } else {
    sample_set_tmp_message (s, _("Clipboard empty"));
  }
}

void
paste_xfade_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * s = view->sample;

  if (clipboard_width() > 0) {
    create_paste_xfade_dialog (s);
  } else {
    sample_set_tmp_message (s, _("Clipboard empty"));
  }
}

void
paste_as_new_cb (GtkWidget * widget, gpointer data)
{
  sw_sample * s;
  sw_view * v;

  s = do_paste_as_new ();
  
  if (s) {
    v = view_new_all (s, 1.0);
    sample_add_view (s, v);
    sample_bank_add (s);
  }
}

void
show_info_dialog_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;

  sample_show_info_dialog (view->sample);
}

void
hide_window_cb (GtkAccelGroup *accel_group,
                                             GObject *acceleratable,
                                             guint keyval,
                                             GdkModifierType modifier)
{
	if (GTK_IS_WINDOW(acceleratable))
  		g_signal_emit_by_name(G_OBJECT(acceleratable), "hide");
}

void 
close_window_cb (GtkAccelGroup *accel_group,
                                             GObject *acceleratable,
                                             guint keyval,
                                             GdkModifierType modifier)
{
	if (GTK_IS_WINDOW(acceleratable))
  		g_signal_emit_by_name(G_OBJECT(acceleratable), "destroy");
}
