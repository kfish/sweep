/*
 * Sweep, a sound wave editor.
 *
 * Copyright (C) 2000-2002 Conrad Parker
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

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "callbacks.h"

#include <sweep/sweep_i18n.h>

#include <sweep/sweep_types.h>
#include <sweep/sweep_undo.h>
#include <sweep/sweep_sounddata.h>
#include <sweep/sweep_sample.h>
#include <sweep/sweep_typeconvert.h>

#include "sample.h"
#include "interface.h"
#include "edit.h"
#include "head.h"
#include "levelmeter.h"
#include "sample-display.h"
#include "play.h"
#include "driver.h"
#include "undo_dialog.h"
#include "db_slider.h"

#define DEBUG

extern GtkStyle * style_wb;
extern GtkStyle * style_LCD;
extern GtkStyle * style_light_grey;
extern GtkStyle * style_red_grey;
extern GtkStyle * style_green_grey;
extern GtkStyle * style_red;

static GtkWidget * rec_dialog = NULL;

static sw_handle * rec_handle = NULL;

static gboolean rec_prepared = FALSE;

static gint update_tag = -1;

static GtkWidget * rec_ind_ebox;
static GtkWidget * rec_ind_label;
static gboolean rec_ind_state = FALSE;

static GtkWidget * combo;
static GtkWidget * mix_slider, * gain_slider;
static sw_head_controller * head_controller;

static sw_head * rec_head = NULL;

static gint
update_rec_ind (gpointer data)
{
  sw_head * h = (sw_head *)data;

  if (rec_dialog == NULL) {
    return FALSE;
  } else if (!h->going) {
    gtk_label_set_text (GTK_LABEL(rec_ind_label), _("Ready to record"));
    gtk_widget_set_style (rec_ind_ebox, style_light_grey);
    return FALSE;
  } else {
    rec_ind_state = !rec_ind_state;

    if (rec_ind_state) {
      gtk_widget_set_style (rec_ind_ebox, style_red);
    } else {
      gtk_widget_set_style (rec_ind_ebox, style_light_grey);
    }

    return TRUE;
  }
}

static gint
update_recmarker (gpointer data)
{
  sw_head * h = (sw_head *)data;

  if (rec_dialog == NULL || !h->going) {
    update_tag = 0;

    head_set_going (h, FALSE);

    return FALSE;
  } else {
    sample_set_rec_marker (h->sample, h->offset);

    head_set_offset (h, h->offset);
    return TRUE;
  }
}

static void
start_recmarker (sw_head * head)
{
  if (update_tag > 0) {
    gtk_timeout_remove (update_tag);
  }

  update_tag = gtk_timeout_add ((guint32)30,
				(GtkFunction)update_recmarker,
				(gpointer)head);

  gtk_label_set_text (GTK_LABEL(rec_ind_label), "RECORDING");
  gtk_widget_set_style (rec_ind_ebox, style_red);
  rec_ind_state = TRUE;

  gtk_timeout_add ((guint32)500,
		   (GtkFunction)update_rec_ind,
		   (gpointer)head);
}

static void
mix_value_changed_cb (GtkWidget * widget, gfloat value, gpointer data)
{
  sw_head * head = (sw_head *)data;

  head->mix = value;
}

static void
gain_value_changed_cb (GtkWidget * widget, gfloat value, gpointer data)
{
  sw_head * head = (sw_head *)data;

  head->gain = value;
}

void
stop_recording (sw_sample * sample)
{
  head_set_going (sample->rec_head, FALSE);
}

static void
prepare_recording (sw_format * format)
{
  rec_handle = device_open (0, O_RDONLY);

  if (rec_handle == NULL) return;

  device_setup (rec_handle, format);

  rec_prepared = TRUE;
}

static struct timeval tv_instant = {0, 0};

static void
do_record_regions (sw_sample * sample)
{
  sw_head * head = sample->rec_head;
  sw_sounddata * sounddata = sample->sounddata;
  sw_format * f = sounddata->format;
  fd_set fds;
  sw_framecount_t sel_total, run_total;
  sw_framecount_t offset, remaining, n, count;
  gint percent;
  gboolean rec_mixing = FALSE;

  sw_audio_t * rbuf;

  gboolean active = TRUE;

  if (!rec_prepared)
    prepare_recording (f);

  if (rec_handle == NULL) goto done;

  head->restricted = TRUE;

  sel_total = sounddata_selection_nr_frames (sounddata) / 100;
  if (sel_total == 0) sel_total = 1;
  run_total = 0;

  rbuf = alloca (1024 * f->channels * sizeof (sw_audio_t));

  rec_mixing = TRUE;

  while (active) {
      
    FD_ZERO (&fds);
    FD_SET (rec_handle->driver_fd, &fds);
    
    if (select (rec_handle->driver_fd+1, &fds, NULL, NULL, &tv_instant) == 0);

    n = 1024;
    count = n * f->channels;
    
    count = device_read (rec_handle, rbuf, count);
    
    g_mutex_lock (sample->ops_mutex);
    
    if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL || count == -1 ||
	!head->going) {
      active = FALSE;
    } else {
      head_write (head, rbuf, n);
      
      remaining -= n;
      offset += n;
      
      run_total += n;
      percent = run_total / sel_total;
      percent = MIN (100, percent);
      sample_set_progress_percent (sample, percent);
      
    }
    
    g_mutex_unlock (sample->ops_mutex);
  }
  
 done:
  if (rec_handle != NULL) {
    device_close (rec_handle);
  }

  gtk_widget_set_sensitive (combo, TRUE);

  rec_prepared = FALSE;
}

static void
do_record_regions_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;

  sw_edit_buffer * old_eb;
  paste_over_data * p;

  old_eb = edit_buffer_from_sample (sample);

  p = paste_over_data_new (old_eb, old_eb);
  inst->redo_data = inst->undo_data = p;
  set_active_op (sample, inst);

  do_record_regions (sample);

  if (sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    p->new_eb = edit_buffer_from_sample (sample);
    
    register_operation (sample, inst);
  }
}

static sw_operation record_regions_op = {
  SWEEP_EDIT_MODE_FILTER,
  (SweepCallback)do_record_regions_thread,
  (SweepFunction)NULL,
  (SweepCallback)undo_by_paste_over,
  (SweepFunction)paste_over_data_destroy,
  (SweepCallback)redo_by_paste_over,
  (SweepFunction)paste_over_data_destroy
};

void
record_cb (GtkWidget * widget, gpointer data)
{
  sw_head * head = (sw_head *)data;
  sw_sample * sample = head->sample;

  if (head->going) {
    stop_recording (sample);
  } else {
    if (sounddata_selection_nr_frames (sample->sounddata) > 0) {
      head->going = TRUE;
      gtk_widget_set_sensitive (combo, FALSE);
      schedule_operation (sample, "Record", &record_regions_op, NULL);
      start_recmarker (head);
    } else {
      head_set_going (head, FALSE);
      sample_set_tmp_message (sample, _("No selection to record into"));
    }
  }
}

static void
rec_dialog_destroy (GtkWidget * widget, gpointer data)
{
  sw_head * head = (sw_head *)data;

  rec_dialog = NULL;
  rec_head = NULL;

  stop_recording (head->sample);
}

static void
_rec_dialog_set_sample (sw_sample * sample, gboolean select_current)
{
  sw_head * head;

  gtk_entry_set_text (GTK_ENTRY(GTK_COMBO(combo)->entry),
		      g_basename (sample->pathname));

  if (sample->rec_head == NULL) {
    sample->rec_head = head_new (sample, SWEEP_HEAD_RECORD);
  }

  head = sample->rec_head;

  if (rec_head != NULL) {
    gtk_signal_disconnect_by_data (GTK_OBJECT(mix_slider), rec_head);
    gtk_signal_disconnect_by_data (GTK_OBJECT(gain_slider), rec_head);
  }

  db_slider_set_value (DB_SLIDER(mix_slider), head->mix);
  gtk_signal_connect (GTK_OBJECT(mix_slider), "value-changed",
		      GTK_SIGNAL_FUNC(mix_value_changed_cb), head);

  db_slider_set_value (DB_SLIDER(gain_slider), head->gain);
  gtk_signal_connect (GTK_OBJECT(gain_slider), "value-changed",
		      GTK_SIGNAL_FUNC(gain_value_changed_cb), head);

  rec_head = head;

  head_controller_set_head (head_controller, rec_head);
}

void
rec_dialog_refresh_sample_list (void)
{
  GList * cbitems = NULL;

  if (rec_dialog == NULL)
    return;

  if ((cbitems = sample_bank_list_names ()) != NULL)
    gtk_combo_set_popdown_strings (GTK_COMBO(combo), cbitems);

  g_list_free (cbitems);
}

static void
rec_dialog_entry_changed_cb (GtkWidget * widget, gpointer data)
{
  GtkEntry * entry;
  gchar * new_text;
  sw_sample * sample;

  entry = GTK_ENTRY(GTK_COMBO(combo)->entry);
  new_text = gtk_entry_get_text (entry);

  sample = sample_bank_find_byname (new_text);

  if (sample == NULL) return;

  gtk_signal_handler_block_by_data (GTK_OBJECT(entry), NULL);
  _rec_dialog_set_sample (sample, TRUE);
  gtk_signal_handler_unblock_by_data (GTK_OBJECT(entry), NULL);
}

void
rec_dialog_create (sw_sample * sample)
{
  GtkWidget * window;
  GtkWidget * main_vbox;
  GtkWidget * separator;
  GtkWidget * frame;
  GtkWidget * hbox;
  GtkWidget * label;
  GtkWidget * slider;
  GtkWidget * ebox;
  GtkWidget * hctl;
  GtkTooltips * tooltips;

#ifdef DEVEL_CODE
  GtkWidget * levelmeter;
#endif

  /*  GSList * group;*/

  GtkAccelGroup * accel_group;

  sw_head * head;

  if (sample->rec_head == NULL) {
    sample->rec_head = head_new (sample, SWEEP_HEAD_RECORD);
  }

  head = sample->rec_head;

  if (rec_dialog == NULL) {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    rec_dialog = window;
        
    main_vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER(window), main_vbox);
    gtk_widget_show (main_vbox);

    gtk_window_set_wmclass(GTK_WINDOW(rec_dialog), "rec_dialog", "Sweep");
    gtk_window_set_title(GTK_WINDOW(rec_dialog), _("Sweep: Record"));
    gtk_window_position(GTK_WINDOW(rec_dialog), GTK_WIN_POS_MOUSE);

    accel_group = gtk_accel_group_new ();
    gtk_window_add_accel_group (GTK_WINDOW(rec_dialog), accel_group);

    gtk_signal_connect(GTK_OBJECT(rec_dialog), "destroy",
		       (GtkSignalFunc) rec_dialog_destroy, head);

    gtk_accel_group_add (accel_group, GDK_w, GDK_CONTROL_MASK, GDK_NONE,
			 GTK_OBJECT(rec_dialog), "hide");

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_box_pack_start (GTK_BOX(main_vbox), hbox, FALSE, TRUE, 8);
    gtk_widget_show (hbox);

    label = gtk_label_new (_("File:"));
    gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 8);
    gtk_widget_show (label);

    combo = gtk_combo_new ();
    gtk_box_pack_start (GTK_BOX(hbox), combo, TRUE, TRUE, 8);
    gtk_widget_show (combo);

    gtk_entry_set_editable (GTK_ENTRY(GTK_COMBO(combo)->entry), FALSE);

    gtk_signal_connect (GTK_OBJECT(GTK_COMBO(combo)->entry), "changed",
			GTK_SIGNAL_FUNC(rec_dialog_entry_changed_cb), NULL);

    separator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX(main_vbox), separator, FALSE, FALSE, 0);
    gtk_widget_show (separator);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX(main_vbox), hbox, TRUE, TRUE, 8);
    gtk_widget_show (hbox);

    /* Old signal */

    frame = gtk_frame_new (_("Previous sound"));
    gtk_container_set_border_width (GTK_CONTAINER(frame), 8);
    gtk_box_pack_start (GTK_BOX(hbox), frame, TRUE, TRUE, 0);
    gtk_widget_show (frame);

    tooltips = gtk_tooltips_new ();

    slider = db_slider_new (_("Gain"), head->mix, 0.0, 1.0);
    gtk_container_add (GTK_CONTAINER(frame), slider);
    gtk_widget_show (slider);

    mix_slider = slider;

    gtk_tooltips_set_tip (tooltips, slider,
			  _("This slider allows you to mix the new recording "
			    "in with the previous contents of the buffer. "
			    "Set it to -inf dB to overwrite the previous "
			    "sound."), NULL);
    /* New signal */

    frame = gtk_frame_new (_("Recorded sound"));
    gtk_container_set_border_width (GTK_CONTAINER(frame), 8);
    gtk_box_pack_start (GTK_BOX(hbox), frame, TRUE, TRUE, 0);
    gtk_widget_show (frame);

    slider = db_slider_new (_("Gain"), head->gain, 0.0, 1.0);
    gtk_container_add (GTK_CONTAINER(frame), slider);
    gtk_widget_show (slider);

    gain_slider = slider;

    gtk_tooltips_set_tip (tooltips, slider,
			  _("This slider allows you to reduce the level of "
			    "the recorded sound. Set it to 0 dB to record "
			    "without any reduction. "
			    "Note that setting this to -inf dB will record "
			    "silence."), NULL);

    /* Level meters */

#ifdef DEVEL_CODE
    levelmeter = levelmeter_new (7);
    gtk_box_pack_start (GTK_BOX(hbox), levelmeter, FALSE, TRUE, 3);
    gtk_widget_show (levelmeter);

    levelmeter = levelmeter_new (7);
    gtk_box_pack_start (GTK_BOX(hbox), levelmeter, FALSE, TRUE, 3);
    gtk_widget_show (levelmeter);
#endif

    hctl = head_controller_create (sample->rec_head, rec_dialog,
				   &head_controller);
    gtk_box_pack_start (GTK_BOX (main_vbox), hctl, FALSE, TRUE, 0);
    gtk_widget_show (hctl);

    ebox = gtk_event_box_new ();
    gtk_widget_set_style (ebox, style_light_grey);
    gtk_box_pack_start (GTK_BOX(main_vbox), ebox, FALSE, TRUE, 0);
    gtk_widget_show (ebox);

    rec_ind_ebox = ebox;

    label = gtk_label_new (_("Ready to record"));
    gtk_container_add (GTK_CONTAINER(ebox), label);
    gtk_widget_show (label);

    rec_ind_label = label;
  }

  rec_dialog_refresh_sample_list ();
  _rec_dialog_set_sample (sample, TRUE);

  if (!GTK_WIDGET_VISIBLE(rec_dialog)) {
    gtk_widget_show (rec_dialog);
  } else {
    gdk_window_raise (rec_dialog->window);
  }

}
