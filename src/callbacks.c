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

#include <gtk/gtk.h>

#include "callbacks.h"

#include "sweep_types.h"
#include "sweep_undo.h"
#include "sample.h"
#include "interface.h"
#include "edit.h"
#include "sample-display.h"
#include "driver.h"

extern gint current_tool;

/*
 * Default zooming parameters.
 *
 * DEFAULT_ZOOM sets the ratio for zooming. Each time zoom_in is
 * called, the view is adjusted to be 1/DEFAULT_ZOOM its current
 * length. Each time zoom_out is called, the view is adjusted to
 * DEFAULT_ZOOM times its current length.
 * 
 * If you alter these, make sure
 * (DEFAULT_ZOOM - 1) * DEFAULT_MIN_ZOOM  >  1
 */
#define DEFAULT_ZOOM 1.2
#define DEFAULT_MIN_ZOOM 8


/* Sample creation */

void
sample_new_empty_cb (GtkWidget * widget, gpointer data)
{
  sw_sample * s;
  char * directory, * filename;
  gint nr_channels;
  gint sample_rate;
  gint sample_length;
  sw_view * v;

  directory = NULL;  /* XXX: Why is this code so poxy? */
  filename = NULL;
  nr_channels = 2;
  sample_rate = 44100;
  sample_length = 2048;

  s = sample_new_empty(directory,
		       filename,
		       nr_channels,
		       sample_rate,
		       sample_length);
  
  v = view_new_all (s, 1.0);
  sample_add_view (s, v);

  sample_bank_add(s);
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
  v = view_new_all (ns, 1.0);
  sample_add_view (ns, v);

  sample_bank_add(ns);
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
  stop_playback();
  gtk_main_quit();
}

/* Tools */
void
set_tool_cb (GtkWidget * widget, gpointer data)
{
  gint tool = (gint)data;

  current_tool = tool;
}

/* Zooming */

void
zoom_in_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_view * v = sd->view;
  sw_framecount_t nstart, nlength, olength;

  olength = v->end - v->start;
  nlength = (sw_framecount_t)((double)olength / DEFAULT_ZOOM);

  if (nlength < olength) {
    nstart = v->start + (olength - nlength) / 2UL;
  } else {
    nstart = v->start - (nlength - olength) / 2UL;
  }

  if(nlength <= DEFAULT_MIN_ZOOM) return;

  view_set_ends(sd->view, nstart, nstart+nlength);
}

void
zoom_out_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_view * v = sd->view;
  sw_framecount_t nstart, nlength, olength;

  olength = v->end - v->start;
  nlength = (sw_framecount_t)((double)olength * DEFAULT_ZOOM);

  if (nlength < 0) return; /* sw_framecount_t multiplication overflow */

  if (nlength > olength) {
    nstart = v->start - (nlength - olength) / 2UL;
  } else {
    nstart = v->start + (olength - nlength) / 2UL;
  }

  if (nstart == v->start && (nstart+nlength) == v->end)
    return;

  view_set_ends(sd->view, nstart, nstart+nlength);
}

void
zoom_to_sel_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  GList * gl;
  sw_sel * sel;
  gint sel_min, sel_max;

  if(!sd) return;

  if(!sd->view->sample->sounddata->sels) return;

  gl = sd->view->sample->sounddata->sels;

  sel = (sw_sel *)gl->data;
  sel_min = sel->sel_start;

  if (gl->next)
    for (gl = gl->next; gl->next; gl = gl->next);

  sel = (sw_sel *)gl->data;
  sel_max = sel->sel_end;

  view_set_ends(sd->view, sel_min, sel_max);
}

void
zoom_left_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  GtkAdjustment * adj = GTK_ADJUSTMENT(sd->view->adj);

  adj->value -= adj->page_size;
  if(adj->value < adj->lower) {
    adj->value = adj->lower;
  }

  gtk_adjustment_value_changed (GTK_ADJUSTMENT(adj));
}

void
zoom_right_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  GtkAdjustment * adj = GTK_ADJUSTMENT(sd->view->adj);

  adj->value += adj->page_size;
  if(adj->value > adj->upper) {
    adj->value = adj->upper;
  }

  gtk_adjustment_value_changed (GTK_ADJUSTMENT(adj));
}

void
zoom_1_1_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_sample * s;

  s = sd->view->sample;

  view_set_ends(sd->view, 0, s->sounddata->nr_frames);
}

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


/* Playback */

void
play_view_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_view * v;

  v = sd->view;

  play_view_all (v);
}

void
play_view_all_loop_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_view * v;

  v = sd->view;

  play_view_all_loop (v);
}

void
play_view_sel_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_view * v;

  v = sd->view;

  play_view_sel (v);
}

void
play_view_sel_loop_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_view * v;

  v = sd->view;

  play_view_sel_loop (v);
}

void
stop_playback_cb (GtkWidget * widget, gpointer data)
{
  stop_playback ();
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

  gtk_signal_handler_block_by_data (GTK_OBJECT(v->adj), v);
  view_fix_adjustment (sd->view);
  gtk_signal_handler_unblock_by_data (GTK_OBJECT(v->adj), v);

  view_refresh_hruler (v);
}

void
adj_changed_cb (GtkWidget * widget, gpointer data)
{
  sw_view * v = (sw_view *)data;
  SampleDisplay * sd = SAMPLE_DISPLAY(v->display);
  GtkAdjustment * adj = GTK_ADJUSTMENT(v->adj);

  gtk_signal_handler_block_by_data (GTK_OBJECT(sd), v);
  sample_display_set_window(sd,
			    (gint)adj->value,
			    (gint)(adj->value + adj->page_size));
  gtk_signal_handler_unblock_by_data (GTK_OBJECT(sd), v);

  view_refresh_hruler (v);
}


/* Selection */

void
select_invert_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);

  sample_selection_invert (sd->view->sample);

  sample_refresh_views (sd->view->sample);
}

void
select_all_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);

  sample_selection_select_all (sd->view->sample);

  sample_refresh_views (sd->view->sample);
}

void
select_none_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);

  sample_selection_select_none (sd->view->sample);

  sample_refresh_views (sd->view->sample);
}


/* Undo / Redo */

void
undo_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);

  undo_current (sd->view->sample);
}

void
redo_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);

  redo_current (sd->view->sample);
}


/* Edit */

void
copy_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_sample * s = sd->view->sample;

  do_copy (s);
}

void
cut_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_sample * s = sd->view->sample;

  do_cut (s);
}

void
clear_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_sample * s = sd->view->sample;

  do_clear (s);
}

void
delete_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_sample * s = sd->view->sample;

  do_delete (s);
}

void
paste_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_sample * s = sd->view->sample;

  do_paste_at (s);
}

void
paste_as_new_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_sample * s;
  sw_view * v;

  do_paste_as_new (sd->view->sample, &s);

  if (s) {
    v = view_new_all (s, 1.0);
    sample_add_view (s, v);
  }
}

