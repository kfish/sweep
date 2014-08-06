/*
 * Sweep, a sound wave editor.
 *
 * Copyright (C) 2000 Conrad Parker
 * Copyright (C) 2002 CSIRO Australia
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

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_types.h>
#include <sweep/sweep_typeconvert.h>
#include <sweep/sweep_undo.h>
#include <sweep/sweep_filter.h>
#include <sweep/sweep_selection.h>
#include <sweep/sweep_sounddata.h>
#include <sweep/sweep_sample.h>

#include "sweep_app.h"
#include "edit.h"
#include "sw_chooser.h"

#define BUFFER_LEN 4096

/* Mono dup channels */

static void
do_dup_channels_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  int new_channels = GPOINTER_TO_INT(inst->do_data);
  int min_channels;
  sw_format * old_format = sample->sounddata->format;
  sw_sounddata * old_sounddata, * new_sounddata;
  sw_framecount_t nr_frames;

  float * old_d, * new_d;

  sw_framecount_t remaining, n, run_total, ctotal;
  int i, j, k;
  int percent;

  gboolean active = TRUE;

  old_sounddata = sample->sounddata;
  nr_frames = old_sounddata->nr_frames;

  new_sounddata = sounddata_new_empty (new_channels,
				       old_format->rate, nr_frames);

  min_channels = MIN (old_format->channels, new_channels);

  remaining = nr_frames;
  ctotal = remaining / 100;
  if (ctotal == 0) ctotal = 1;
  run_total = 0;

  old_d = (float *)old_sounddata->data;
  new_d = (float *)new_sounddata->data;

  /* Create selections */
  g_mutex_lock (&sample->ops_mutex);
  new_sounddata->sels = sels_copy (old_sounddata->sels);
  g_mutex_unlock (&sample->ops_mutex);

  /* Mix down */
  while (active && remaining > 0) {
    g_mutex_lock (&sample->ops_mutex);

    if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
      active = FALSE;
    } else {

      n = MIN (remaining, 4096);

      for (i = 0; i < n; i++) {
	k = 0;
	while (k < new_channels) {
	  for (j = 0; j < min_channels; j++) {
	    new_d[k++] = old_d[j];
	  }
	}
	old_d += old_format->channels;
	new_d += new_channels;
      }

      remaining -= n;
      run_total += n;

      percent = run_total / ctotal;
      sample_set_progress_percent (sample, percent);
    }

    g_mutex_unlock (&sample->ops_mutex);
  }

  if (remaining > 0) { /* cancelled or failed */
    sounddata_destroy (new_sounddata);
  } else if (sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    sample->sounddata = new_sounddata;

    inst->redo_data = inst->undo_data =
      sounddata_replace_data_new (sample, old_sounddata, new_sounddata);

    register_operation (sample, inst);
  }
}

static sw_operation dup_channels_op = {
  SWEEP_EDIT_MODE_ALLOC,
  (SweepCallback)do_dup_channels_thread,
  (SweepFunction)NULL,
  (SweepCallback)undo_by_sounddata_replace,
  (SweepFunction)sounddata_replace_data_destroy,
  (SweepCallback)redo_by_sounddata_replace,
  (SweepFunction)sounddata_replace_data_destroy
};

void
dup_channels (sw_sample * sample, int new_channels)
{
  char buf[128];

  if (sample->sounddata->format->channels == 1) {
    g_snprintf (buf, sizeof (buf), _("Duplicate to %d channels"), new_channels);
  } else {
    g_snprintf (buf, sizeof (buf), _("Duplicate from %d to %d channels"),
		sample->sounddata->format->channels, new_channels);
  }

  schedule_operation (sample, buf, &dup_channels_op,
		      GINT_TO_POINTER(new_channels));
}

void
dup_stereo_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;

  dup_channels (sample, 2);
}

static void
dup_channels_dialog_ok_cb (GtkWidget * widget, gpointer data)
{
  sw_sample * sample = (sw_sample *)data;
  GtkWidget * dialog;
  GtkWidget * chooser;

  int new_channels;

  dialog = gtk_widget_get_toplevel (widget);

  chooser = g_object_get_data (G_OBJECT(dialog), "default");
  new_channels = channelcount_chooser_get_count (chooser);

  gtk_widget_destroy (dialog);

  dup_channels (sample, new_channels);
}

static void
dup_channels_dialog_cancel_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;

  dialog = gtk_widget_get_toplevel (widget);
  gtk_widget_destroy (dialog);
}

void
dup_channels_dialog_new_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;
  GtkWidget * dialog;
  GtkWidget * main_vbox;
  GtkWidget * chooser;
  GtkWidget * button, * ok_button;

  dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW(dialog), _("Sweep: Duplicate channel"));
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  gtk_container_set_border_width (GTK_CONTAINER(dialog), 8);

  main_vbox = GTK_DIALOG(dialog)->vbox;

  chooser = channelcount_chooser_new (_("Output channels"));
  gtk_box_pack_start (GTK_BOX(main_vbox), chooser, TRUE, TRUE, 0);
  channelcount_chooser_set_count (chooser,
				  sample->sounddata->format->channels);
  gtk_widget_show (chooser);

  g_object_set_data (G_OBJECT(dialog), "default", chooser);

  /* OK */

  ok_button = gtk_button_new_with_label (_("OK"));
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (ok_button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), ok_button,
		      TRUE, TRUE, 0);
  gtk_widget_show (ok_button);
  g_signal_connect (G_OBJECT(ok_button), "clicked",
		      G_CALLBACK (dup_channels_dialog_ok_cb),
		      sample);

  /* Cancel */

  button = gtk_button_new_with_label (_("Cancel"));
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), button,
		      TRUE, TRUE, 0);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK (dup_channels_dialog_cancel_cb),
		      NULL);

  gtk_widget_grab_default (ok_button);

  gtk_widget_show (dialog);
}

static void
do_mono_mixdown_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;

  sw_format * old_format = sample->sounddata->format;
  sw_sounddata * old_sounddata, * new_sounddata;
  sw_framecount_t nr_frames;

  float * old_d, * new_d;

  sw_framecount_t remaining, n, run_total, ctotal;
  int i, j;
  int percent;

  gboolean active = TRUE;

  old_sounddata = sample->sounddata;
  nr_frames = old_sounddata->nr_frames;

  new_sounddata = sounddata_new_empty (1, old_format->rate, nr_frames);

  remaining = nr_frames;
  ctotal = remaining / 100;
  if (ctotal == 0) ctotal = 1;
  run_total = 0;

  old_d = (float *)old_sounddata->data;
  new_d = (float *)new_sounddata->data;

  /* Create selections */
  g_mutex_lock (&sample->ops_mutex);
  new_sounddata->sels = sels_copy (old_sounddata->sels);
  g_mutex_unlock (&sample->ops_mutex);

  /* Mix down */
  while (active && remaining > 0) {
    g_mutex_lock (&sample->ops_mutex);

    if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
      active = FALSE;
    } else {

      n = MIN (remaining, 4096);

      for (i = 0; i < n; i++) {
	for (j = 0; j < old_format->channels; j++) {
	  *new_d += *old_d;
	  old_d++;
	}
	new_d++;
      }

      remaining -= n;
      run_total += n;

      percent = run_total / ctotal;
      sample_set_progress_percent (sample, percent);
    }

    g_mutex_unlock (&sample->ops_mutex);
  }

  if (remaining > 0) { /* cancelled or failed */
    sounddata_destroy (new_sounddata);
  } else if (sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    sample->sounddata = new_sounddata;

    inst->redo_data = inst->undo_data =
      sounddata_replace_data_new (sample, old_sounddata, new_sounddata);

    register_operation (sample, inst);
  }
}

static sw_operation mono_mixdown_op = {
  SWEEP_EDIT_MODE_ALLOC,
  (SweepCallback)do_mono_mixdown_thread,
  (SweepFunction)NULL,
  (SweepCallback)undo_by_sounddata_replace,
  (SweepFunction)sounddata_replace_data_destroy,
  (SweepCallback)redo_by_sounddata_replace,
  (SweepFunction)sounddata_replace_data_destroy
};

void
mono_mixdown_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;

  schedule_operation (sample, _("Mix down to mono"), &mono_mixdown_op, NULL);
}

static void
do_remove_channel_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  int channel = GPOINTER_TO_INT(inst->do_data);

  sw_format * old_format = sample->sounddata->format;
  sw_sounddata * old_sounddata, * new_sounddata;
  sw_framecount_t nr_frames;

  float * old_d, * new_d;

  sw_framecount_t remaining, n, run_total, ctotal;
  int i, j;
  int percent;

  gboolean active = TRUE;

  old_sounddata = sample->sounddata;
  nr_frames = old_sounddata->nr_frames;

  new_sounddata = sounddata_new_empty (old_format->channels - 1,
				       old_format->rate, nr_frames);

  remaining = nr_frames;
  ctotal = remaining / 100;
  if (ctotal == 0) ctotal = 1;
  run_total = 0;

  old_d = (float *)old_sounddata->data;
  new_d = (float *)new_sounddata->data;

  /* Create selections */
  g_mutex_lock (&sample->ops_mutex);
  new_sounddata->sels = sels_copy (old_sounddata->sels);
  g_mutex_unlock (&sample->ops_mutex);

  /* Mix down */
  while (active && remaining > 0) {
    g_mutex_lock (&sample->ops_mutex);

    if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
      active = FALSE;
    } else {

      n = MIN (remaining, 4096);

      for (i = 0; i < n; i++) {
	for (j = 0; j < old_format->channels; j++) {
	  if (j != channel) {
	    *new_d = *old_d;
	    new_d++;
	  }
	  old_d++;
	}
      }

      remaining -= n;
      run_total += n;

      percent = run_total / ctotal;
      sample_set_progress_percent (sample, percent);
    }

    g_mutex_unlock (&sample->ops_mutex);
  }

  if (remaining > 0) { /* cancelled or failed */
    sounddata_destroy (new_sounddata);
  } else if (sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    sample->sounddata = new_sounddata;

    inst->redo_data = inst->undo_data =
      sounddata_replace_data_new (sample, old_sounddata, new_sounddata);

    register_operation (sample, inst);
  }
}

static sw_operation remove_channel_op = {
  SWEEP_EDIT_MODE_ALLOC,
  (SweepCallback)do_remove_channel_thread,
  (SweepFunction)NULL,
  (SweepCallback)undo_by_sounddata_replace,
  (SweepFunction)sounddata_replace_data_destroy,
  (SweepCallback)redo_by_sounddata_replace,
  (SweepFunction)sounddata_replace_data_destroy
};

void
remove_left_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;

  schedule_operation (sample, _("Remove left channel"), &remove_channel_op,
		      GINT_TO_POINTER(0));
}

void
remove_right_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;

  schedule_operation (sample, _("Remove right channel"), &remove_channel_op,
		      GINT_TO_POINTER(1));
}

static void
do_stereo_swap (sw_sample * sample, gpointer data)
{
  sw_framecount_t nr_frames;
  float * dl, * dr, t;

  sw_framecount_t remaining, n, run_total, ctotal;
  int i;
  int percent;

  gboolean active = TRUE;

  nr_frames = sample->sounddata->nr_frames;

  remaining = nr_frames;
  ctotal = remaining / 100;
  if (ctotal == 0) ctotal = 1;
  run_total = 0;

  dl = (float *)sample->sounddata->data;
  dr = dl; dr++;

  /* Swap channels */
  while (active && remaining > 0) {
    g_mutex_lock (&sample->ops_mutex);

    if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
      active = FALSE;
    } else {

      n = MIN (remaining, 4096);

      for (i = 0; i < n; i++) {
	t = *dl;
	*dl = *dr;
	*dr = t;
	dl = ++dr;
	dr++;
      }

      remaining -= n;
      run_total += n;

      percent = run_total / ctotal;
      sample_set_progress_percent (sample, percent);
    }

    g_mutex_unlock (&sample->ops_mutex);
  }
}

static void
do_stereo_swap_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;

  do_stereo_swap (sample, NULL);

  if (sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    register_operation (sample, inst);
  }
}

static sw_operation stereo_swap_op = {
  SWEEP_EDIT_MODE_ALLOC,
  (SweepCallback)do_stereo_swap_thread,
  (SweepFunction)NULL,
  (SweepCallback)do_stereo_swap,
  (SweepFunction)NULL,
  (SweepCallback)do_stereo_swap,
  (SweepFunction)NULL
};

void
stereo_swap_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;

  if (sample->sounddata->format->channels == 2)
    schedule_operation (sample, _("Swap channels"), &stereo_swap_op, NULL);
  else
    sample_set_tmp_message (sample, _("Not stereo"));
}


/* Add / Remove channels */

static void
do_change_channels_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  int new_channels = GPOINTER_TO_INT(inst->do_data);
  int min_channels;
  sw_format * old_format = sample->sounddata->format;
  sw_sounddata * old_sounddata, * new_sounddata;
  sw_framecount_t nr_frames;

  float * old_d, * new_d;

  sw_framecount_t remaining, n, run_total, ctotal;
  int i, j;
  int percent;

  gboolean active = TRUE;

  old_sounddata = sample->sounddata;
  nr_frames = old_sounddata->nr_frames;

  new_sounddata = sounddata_new_empty (new_channels,
				       old_format->rate, nr_frames);

  min_channels = MIN (old_format->channels, new_channels);

  remaining = nr_frames;
  ctotal = remaining / 100;
  if (ctotal == 0) ctotal = 1;
  run_total = 0;

  old_d = (float *)old_sounddata->data;
  new_d = (float *)new_sounddata->data;

  /* Create selections */
  g_mutex_lock (&sample->ops_mutex);
  new_sounddata->sels = sels_copy (old_sounddata->sels);
  g_mutex_unlock (&sample->ops_mutex);

  /* Mix down */
  while (active && remaining > 0) {
    g_mutex_lock (&sample->ops_mutex);

    if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
      active = FALSE;
    } else {

      n = MIN (remaining, 4096);

      for (i = 0; i < n; i++) {
	for (j = 0; j < min_channels; j++) {
	  new_d[j] = old_d[j];
	}
	old_d += old_format->channels;
	new_d += new_channels;
      }

      remaining -= n;
      run_total += n;

      percent = run_total / ctotal;
      sample_set_progress_percent (sample, percent);
    }

    g_mutex_unlock (&sample->ops_mutex);
  }

  if (remaining > 0) { /* cancelled or failed */
    sounddata_destroy (new_sounddata);
  } else if (sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    sample->sounddata = new_sounddata;

    inst->redo_data = inst->undo_data =
      sounddata_replace_data_new (sample, old_sounddata, new_sounddata);

    register_operation (sample, inst);
  }
}

static sw_operation change_channels_op = {
  SWEEP_EDIT_MODE_ALLOC,
  (SweepCallback)do_change_channels_thread,
  (SweepFunction)NULL,
  (SweepCallback)undo_by_sounddata_replace,
  (SweepFunction)sounddata_replace_data_destroy,
  (SweepCallback)redo_by_sounddata_replace,
  (SweepFunction)sounddata_replace_data_destroy
};

void
change_channels (sw_sample * sample, int new_channels)
{
  char buf[128];

  g_snprintf (buf, sizeof (buf), _("Convert from %d to %d channels"),
	      sample->sounddata->format->channels, new_channels);

  schedule_operation (sample, buf, &change_channels_op,
		      GINT_TO_POINTER(new_channels));
}

static void
channels_dialog_ok_cb (GtkWidget * widget, gpointer data)
{
  sw_sample * sample = (sw_sample *)data;
  GtkWidget * dialog;
  GtkWidget * chooser;

  int new_channels;

  dialog = gtk_widget_get_toplevel (widget);

  chooser = g_object_get_data (G_OBJECT(dialog), "default");
  new_channels = channelcount_chooser_get_count (chooser);

  gtk_widget_destroy (dialog);

  change_channels (sample, new_channels);
}

static void
channels_dialog_cancel_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;

  dialog = gtk_widget_get_toplevel (widget);
  gtk_widget_destroy (dialog);
}

void
channels_dialog_new_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;
  GtkWidget * dialog;
  GtkWidget * main_vbox;
  GtkWidget * label;
  GtkWidget * chooser;
  GtkWidget * button, * ok_button;

  gchar current [128];

  dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW(dialog), _("Sweep: Add/Remove channels"));
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  gtk_container_set_border_width (GTK_CONTAINER(dialog), 8);

  main_vbox = GTK_DIALOG(dialog)->vbox;

  snprintf (current, sizeof (current), _("Currently: %d channels"),
			     sample->sounddata->format->channels);
  label = gtk_label_new (current);
  gtk_box_pack_start (GTK_BOX(main_vbox), label, TRUE, TRUE, 8);
  gtk_widget_show (label);

  chooser = channelcount_chooser_new (_("Output channels"));
  gtk_box_pack_start (GTK_BOX(main_vbox), chooser, TRUE, TRUE, 0);
  channelcount_chooser_set_count (chooser,
				  sample->sounddata->format->channels);
  gtk_widget_show (chooser);

  g_object_set_data (G_OBJECT(dialog), "default", chooser);

  /* OK */

  ok_button = gtk_button_new_with_label (_("OK"));
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (ok_button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), ok_button,
		      TRUE, TRUE, 0);
  gtk_widget_show (ok_button);
  g_signal_connect (G_OBJECT(ok_button), "clicked",
		      G_CALLBACK (channels_dialog_ok_cb),
		      sample);

  /* Cancel */

  button = gtk_button_new_with_label (_("Cancel"));
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), button,
		      TRUE, TRUE, 0);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK (channels_dialog_cancel_cb),
		      NULL);

  gtk_widget_grab_default (ok_button);

  gtk_widget_show (dialog);
}
