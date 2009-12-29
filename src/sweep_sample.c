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

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_sample.h>
#include <sweep/sweep_typeconvert.h>
#include <sweep/sweep_sounddata.h>
#include <sweep/sweep_selection.h>
#include <sweep/sweep_undo.h>

#include "sample.h" /* app internal functions for samples */

#include "format.h"
#include "print.h"
#include "view.h"
#include "sample-display.h"
#include "play.h"
#include "undo_dialog.h"
#include "head.h"
#include "interface.h"
#include "preferences.h"
#include "record.h"
#include "question_dialogs.h"
#include "sw_chooser.h"

#include "../pixmaps/new.xpm"

/* Defaults for new files */
#define DEFAULT_DURATION "00:01:00.000"
#define DEFAULT_SAMPLERATE 44100
#define DEFAULT_CHANNELS 2

/* Preferences keys for "new file" remembered values */
#define SAMPLERATE_KEY "NewFile_Samplerate"
#define CHANNELS_KEY "NewFile_Channels"

/*#define DEBUG*/

extern sw_view * last_tmp_view;

static GList * sample_bank = NULL;

static int untitled_count = 0;

static void
sample_info_update (sw_sample * sample);

/* Sample functions */

void
sample_add_view (sw_sample * s, sw_view * v)
{
 s->views = g_list_append(s->views, v);
}

void
sample_remove_view (sw_sample * s, sw_view * v)
{
 s->views = g_list_remove(s->views, v);

 if(!s->views) {
   sample_bank_remove(s);
 }
}

/*
 * filename_color_hash
 *
 * Choose colour for this file based on the filename, such that
 * a file is loaded with the same colour each time it is edited.
 *
 * Idea due to Erik de Castro Lopo
 */
static int
filename_color_hash (char * filename)
{
  char *p;
  int i = 0;

  if (filename == NULL) return 0;

  for (p = filename; *p; p++) i += (int)*p;

  return (i % VIEW_COLOR_DEFAULT_MAX);
}

static gchar *
filename_generate (void)
{
  return g_strdup_printf ("%s-%d.aiff", _("Untitled"), ++untitled_count);
}

sw_sample *
sample_new_empty(gchar * pathname,
		 gint nr_channels, gint sample_rate,
		 sw_framecount_t sample_length)
{
  sw_sample *s;

  s = g_malloc0 (sizeof(sw_sample));
  if (!s)
    return NULL;

  s->sounddata = sounddata_new_empty (nr_channels, sample_rate, sample_length);

  s->views = NULL;

  if (pathname != NULL)
    s->pathname = strdup (pathname);
  else
    s->pathname = filename_generate ();

  s->ops_thread = (pthread_t) -1;

  s->ops_mutex = g_mutex_new ();
  s->registered_ops = NULL;
  s->current_undo = NULL;
  s->current_redo = NULL;
  s->active_op = NULL;
  s->op_progress_tag = -1;

  s->tmp_sel = NULL;

  s->playmarker_tag = 0;

  s->edit_mutex = g_mutex_new ();
  s->edit_mode = SWEEP_EDIT_MODE_READY;
  s->edit_state = SWEEP_EDIT_STATE_IDLE;
  s->modified = FALSE;

  s->pending_cond = g_cond_new ();
  s->pending_ops = NULL;

  s->play_mutex = g_mutex_new ();
  s->user_offset = 0;
  s->by_user = 0;

  s->play_head = head_new (s, SWEEP_HEAD_PLAY);

  s->rate = 1.0;

  s->color = filename_color_hash ((gchar *)g_basename (s->pathname));

  s->info_clist = NULL;

  /*  s->recording = FALSE;*/
  /*  s->rec_offset = 0;*/
  s->rec_head = NULL;

  s->tmp_message_active = FALSE;
  s->last_tmp_message = NULL;
  s->tmp_message_tag = -1;

  return s;
}

sw_sample *
sample_new_copy(sw_sample * s)
{
  sw_sample * sn;

  sn = sample_new_empty(NULL,
			s->sounddata->format->channels,
			s->sounddata->format->rate,
			s->sounddata->nr_frames);

  if(!sn) {
    fprintf(stderr, "Unable to allocate new sample.\n");
    return NULL;
  }

  memcpy(sn->sounddata->data, s->sounddata->data,
	 frames_to_bytes(s->sounddata->format, s->sounddata->nr_frames));

  sounddata_copyin_selection (s->sounddata, sn->sounddata);

  /*  sn->last_mtime = s->last_mtime;*/

  return sn;
}

static void
sample_new_dialog_ok_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;
  GtkWidget * entry;
  GtkWidget * checkbutton;
  gchar * filename, * text;
  gdouble seconds;
  sw_framecount_t nr_frames;
  gint nr_channels;
  gint sample_rate;
  gboolean rem_format;

  sw_sample * s;
  sw_view * v;

  dialog = gtk_widget_get_toplevel (widget);

  entry = g_object_get_data (G_OBJECT(dialog), "name_entry");
  filename = (gchar *) gtk_entry_get_text (GTK_ENTRY(entry));

  entry = g_object_get_data (G_OBJECT(dialog), "duration_entry");
  text = (gchar *) gtk_entry_get_text (GTK_ENTRY(entry));
  seconds = strtime_to_seconds (text);
  if (seconds == -1) goto out; /* XXX: invalid time spec */

  entry = g_object_get_data (G_OBJECT(dialog), "rate_chooser");
  sample_rate = samplerate_chooser_get_rate (entry);

  entry = g_object_get_data (G_OBJECT(dialog), "channelcount_chooser");
  nr_channels = channelcount_chooser_get_count (entry);

  nr_frames = (sw_framecount_t) (sample_rate * seconds);

  checkbutton =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "rem_format_chb"));
  rem_format =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton));

  if (rem_format) {
    prefs_set_int (SAMPLERATE_KEY, sample_rate);
    prefs_set_int (CHANNELS_KEY, nr_channels);
  }

  s = sample_new_empty ((gchar *)filename, nr_channels, sample_rate, nr_frames);
  v = view_new_all (s, 1.0);
  sample_add_view (s, v);
  sample_bank_add (s);

 out:
  gtk_widget_destroy (dialog);
}

static void
sample_new_dialog_cancel_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;

  dialog = gtk_widget_get_toplevel (widget);
  gtk_widget_destroy (dialog);

  if (sample_bank == NULL) {
    sweep_quit ();
  }
}


static void
sample_new_dialog_update (GtkWidget * widget)
{
  GtkWidget * dialog;
  GtkWidget * entry;
  GtkWidget * memsize_label;
  GtkWidget * ok_button;
  gchar * text;
  gdouble seconds;
  gint sample_rate, nr_channels;
  glong bytes;

  char buf[16];

  dialog = gtk_widget_get_toplevel (widget);

  entry = g_object_get_data (G_OBJECT(dialog), "duration_entry");
  text = (gchar *)gtk_entry_get_text (GTK_ENTRY(entry));
  seconds = strtime_to_seconds (text);

  entry = g_object_get_data (G_OBJECT(dialog), "rate_chooser");
  sample_rate = samplerate_chooser_get_rate (entry);

  entry = g_object_get_data (G_OBJECT(dialog), "channelcount_chooser");
  nr_channels = channelcount_chooser_get_count (entry);

  memsize_label = g_object_get_data (G_OBJECT(dialog), "memsize_label");

  bytes = (glong) (seconds * sample_rate * nr_channels * sizeof(sw_audio_t));
  if (bytes < 0) {
    gtk_label_set_text (GTK_LABEL(memsize_label), _("Overflow"));
  } else {
    snprint_bytes (buf, sizeof (buf), bytes);
    gtk_label_set_text (GTK_LABEL(memsize_label), buf);
  }

  ok_button = g_object_get_data (G_OBJECT(dialog), "ok_button");

  if (seconds <= 0.0 || sample_rate <= 0 || nr_channels <= 0 || bytes < 0) {
    gtk_widget_set_sensitive (ok_button, FALSE);
  } else {
    gtk_widget_set_sensitive (ok_button, TRUE);
  }
}

static void
sample_new_dialog_reset_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;
  GtkWidget * entry;

  int * i, sample_rate, nr_channels;

  i = prefs_get_int (SAMPLERATE_KEY);
  sample_rate = i ? *i : DEFAULT_SAMPLERATE;

  i = prefs_get_int (CHANNELS_KEY);
  nr_channels = i ? *i : DEFAULT_CHANNELS;

  dialog = gtk_widget_get_toplevel (widget);

  entry = g_object_get_data (G_OBJECT(dialog), "rate_chooser");
  samplerate_chooser_set_rate (entry, sample_rate);

  entry = g_object_get_data (G_OBJECT(dialog), "channelcount_chooser");
  channelcount_chooser_set_count (entry, nr_channels);

  sample_new_dialog_update (dialog);
}

static void
sample_new_dialog_defaults_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;
  GtkWidget * entry;

  dialog = gtk_widget_get_toplevel (widget);

  entry = g_object_get_data (G_OBJECT(dialog), "duration_entry");
  gtk_entry_set_text (GTK_ENTRY (entry), DEFAULT_DURATION);

  entry = g_object_get_data (G_OBJECT(dialog), "rate_chooser");
  samplerate_chooser_set_rate (entry, DEFAULT_SAMPLERATE);

  entry = g_object_get_data (G_OBJECT(dialog), "channelcount_chooser");
  channelcount_chooser_set_count (entry, DEFAULT_CHANNELS);

  sample_new_dialog_update (dialog);
}

static void
create_sample_new_dialog ( gchar * pathname, gint nr_channels, gint sample_rate,
			  sw_time_t duration, gboolean do_reset)
{
  GtkWidget * dialog;
  GtkWidget * main_vbox, * vbox;
  GtkWidget * main_hbox, * hbox, * hbox2;
  GtkWidget * pixmap;
  GtkWidget * frame;
  GtkWidget * ebox;
  GtkWidget * label;
  GtkWidget * entry;
  GtkWidget * button;
  GtkWidget * checkbutton;
  GtkWidget * ok_button;
  GtkTooltips * tooltips;

  gchar buf[16];

  dialog = gtk_dialog_new ();
  sweep_set_window_icon (GTK_WINDOW(dialog));
  gtk_window_set_position (GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_title (GTK_WINDOW(dialog), _("Sweep: New file"));
  /*gtk_container_border_width (GTK_CONTAINER(dialog), 8);*/

  g_signal_connect (G_OBJECT(dialog), "destroy",
		      G_CALLBACK(sample_new_dialog_cancel_cb), dialog);

  attach_window_close_accel(GTK_WINDOW(dialog));

  main_vbox = GTK_DIALOG(dialog)->vbox;

  main_hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(main_vbox), main_hbox, FALSE, FALSE, 0);
  gtk_widget_show (main_hbox);

  /* Left side */

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(main_hbox), vbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER(vbox), 4);
  gtk_widget_show (vbox);

  /* Name */

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER(hbox), 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Name:"));
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 4);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX(hbox), entry, TRUE, TRUE, 4);
  gtk_widget_show (entry);

  gtk_entry_set_text (GTK_ENTRY (entry), 
		      pathname ? pathname : filename_generate ()); 

  g_object_set_data (G_OBJECT (dialog), "name_entry", entry);

  /* Duration */
  
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER(hbox), 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Duration:"));
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 4);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX(hbox), entry, TRUE, TRUE, 4);
  gtk_widget_show (entry);

  snprint_time (buf, sizeof (buf), duration);
  gtk_entry_set_text (GTK_ENTRY (entry), buf); 

  g_signal_connect (G_OBJECT(entry), "changed",
		      G_CALLBACK(sample_new_dialog_update), NULL);

  g_object_set_data (G_OBJECT (dialog), "duration_entry", entry);

  label = gtk_label_new (_("hh:mm:ss.xxx"));
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 4);
  gtk_widget_show (label);

  /* Right side */

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(main_hbox), vbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER(vbox), 4);
  gtk_widget_show (vbox);

  /* Sampling rate */

  entry = samplerate_chooser_new (NULL);
  samplerate_chooser_set_rate (entry, sample_rate);
  gtk_box_pack_start (GTK_BOX(vbox), entry, FALSE, FALSE, 4);
  gtk_widget_show (entry);

  g_signal_connect (G_OBJECT(entry), "number-changed",
		      G_CALLBACK(sample_new_dialog_update), NULL);

  g_object_set_data (G_OBJECT (dialog), "rate_chooser", entry);

  /* Channels */

  entry = channelcount_chooser_new (NULL);
  channelcount_chooser_set_count (entry, nr_channels);
  gtk_box_pack_start (GTK_BOX(vbox), entry, FALSE, FALSE, 4);
  gtk_widget_show (entry);

  g_signal_connect (G_OBJECT(entry), "number-changed",
		      G_CALLBACK(sample_new_dialog_update), NULL);

  g_object_set_data (G_OBJECT (dialog), "channelcount_chooser", entry);

  /* Defaults */

  hbox = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 4);
  gtk_container_set_border_width (GTK_CONTAINER(hbox), 12);
  gtk_widget_show (hbox);

  checkbutton =
    gtk_check_button_new_with_label (_("Remember this format"));
  gtk_box_pack_start (GTK_BOX (hbox), checkbutton, TRUE, TRUE, 0);
  gtk_widget_show (checkbutton);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, checkbutton,
			_("Remember this sampling rate and channel "
			  "configuration for creating new files."),
			NULL);

  g_object_set_data (G_OBJECT (dialog), "rem_format_chb", checkbutton);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkbutton), do_reset);

  hbox2 = gtk_hbox_new (TRUE, 4);
  gtk_box_pack_start (GTK_BOX (hbox), hbox2, FALSE, TRUE, 0);
  gtk_widget_show (hbox2);

  button = gtk_button_new_with_label (_("Reset"));
  gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, TRUE, 4);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK(sample_new_dialog_reset_cb), NULL);
  gtk_widget_show (button);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, button,
			_("Reset to the last remembered format for new files."),
			NULL);

  button = gtk_button_new_with_label (_("Defaults"));
  gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, TRUE, 4);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK(sample_new_dialog_defaults_cb), NULL);
  gtk_widget_show (button);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, button,
			_("Set to the default format for new files."),
			NULL);

  /* Data memory */

  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, TRUE, TRUE, 4);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_widget_show (frame);

  ebox = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER(frame), ebox);
  gtk_widget_show (ebox);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, ebox,
			_("Indicates the amount of data memory which "
			  "will be allocated for the selected duration and "
			  "format. All audio data is processed "
			  "internally in 32 bit floating point format."),
			NULL);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(ebox), hbox);
  gtk_container_set_border_width (GTK_CONTAINER(hbox), 2);
  gtk_widget_show (hbox);

  pixmap = create_widget_from_xpm (dialog, new_xpm);
  gtk_box_pack_start (GTK_BOX(hbox), pixmap, FALSE, FALSE, 4);
  gtk_widget_show (pixmap);

  label = gtk_label_new (_("Data memory:"));
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 4);
  gtk_widget_show (label);

  label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 4);
  gtk_widget_show (label);

  g_object_set_data (G_OBJECT (dialog), "memsize_label", label);

  /* OK */

  ok_button = gtk_button_new_with_label (_("Create"));
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET(ok_button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), ok_button,
		      TRUE, TRUE, 0);
  gtk_widget_show (ok_button);
  g_signal_connect (G_OBJECT(ok_button), "clicked",
		      G_CALLBACK (sample_new_dialog_ok_cb), NULL);

  g_object_set_data (G_OBJECT (dialog), "ok_button", ok_button);

  /* Cancel */

  button = gtk_button_new_with_label (_("Don't create"));
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET(button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), button,
		      TRUE, TRUE, 0);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK (sample_new_dialog_cancel_cb), NULL);

  gtk_widget_grab_default (ok_button);

  if (do_reset) {
    /* Call the reset callback now to set remembered options */
    sample_new_dialog_reset_cb (dialog, NULL);
  } else {
    sample_new_dialog_update (dialog);
  }

  gtk_widget_show (dialog);
}

void
create_sample_new_dialog_defaults ( gchar * pathname)
{
  create_sample_new_dialog (pathname, DEFAULT_CHANNELS, DEFAULT_SAMPLERATE,
			    60, TRUE);
}

void
create_sample_new_dialog_like (sw_sample * s)
{
  sw_format * f = s->sounddata->format;

  create_sample_new_dialog (NULL, f->channels, f->rate,
			    frames_to_time (f, s->sounddata->nr_frames),
			    FALSE);
}

void
sample_destroy (sw_sample * s)
{
  stop_playback (s);

  sounddata_destroy (s->sounddata);

  /* XXX: Should do this: */
  /* trim_registered_ops (s, 0); */

  g_free (s);
}

sw_sounddata *
sample_get_sounddata (sw_sample * s)
{
  return s->sounddata;
}

void
sample_set_pathname (sw_sample * s, gchar * pathname)
{
  sw_view * v;
  GList * gl;

  if (pathname == s->pathname) return;

  if (pathname) {
    s->pathname = strdup (pathname);
  } else {
    if (s->pathname) g_free (s->pathname);
    s->pathname = NULL;
  }

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    view_refresh_title (v);
  }
}

GList *
sample_bank_list_names (void)
{
  GList * ret = NULL;
  GList * gl = NULL;
  sw_sample * s;

  for (gl = sample_bank; gl; gl = gl->next) {
    s = (sw_sample *)gl->data;

    ret = g_list_append (ret, (gpointer *)g_basename (s->pathname));
  }

  return ret;
}

sw_sample *
sample_bank_find_byname (const gchar * name)
{
  GList * gl;
  sw_sample * sample;

  if (name == NULL) return NULL;

  for (gl = sample_bank; gl; gl = gl->next) {
    sample = (sw_sample *)gl->data;

    if ((sample->pathname != NULL) &&
	(!strcmp (name, g_basename (sample->pathname))))
      return sample;
  }

  return NULL;
}

gboolean
sample_bank_contains (sw_sample *s)
{
  return (g_list_find (sample_bank, s) != 0);
}

void
sample_bank_add (sw_sample * s)
{
  /* Check that sample is not already in sample_bank */
  if (g_list_find (sample_bank, s)) return;

  sample_bank = g_list_append (sample_bank, s);

  undo_dialog_refresh_sample_list ();
  rec_dialog_refresh_sample_list ();
}

/*
 * sample_bank_remove (s)
 *
 * Takes a sample out of the sample list.
 */
void
sample_bank_remove (sw_sample * s)
{
  if (s) {
    sample_destroy(s);
    sample_bank = g_list_remove(sample_bank, s);
    
    undo_dialog_refresh_sample_list ();
    rec_dialog_refresh_sample_list ();
  }

  if (sample_bank == NULL) {
    sweep_quit ();
  }
}

void
sweep_quit_ok_cb (GtkWidget * widget, gpointer data)
{
  stop_all_playback ();
  gtk_main_quit ();
}

void
sweep_quit_cancel_cb (GtkWidget * widget, gpointer data)
{
  GList * gl;
  sw_sample * s;

  for (gl = sample_bank; gl; gl = gl->next) {
    s = (sw_sample *)gl->data;
    sample_set_tmp_message (s, _("Excellent!!!"));
    sample_set_progress_ready (s);
  }

}

void
sweep_quit (void)
{
  GList * gl;
  sw_sample * s;
  gboolean any_modified = FALSE;

  for (gl = sample_bank; gl; gl = gl->next) {
    s = (sw_sample *)gl->data;
    if (s->modified) {
      any_modified = TRUE;
      break;
    }
  }

  if (any_modified) {
    question_dialog_new (NULL, _("Files unsaved"),
			 _("Some files are unsaved. If you quit, all "
			   "changes will be lost.\n\n"
			   "Are you sure you want to quit?"),
			 _("Quit"), _("Don't quit"),
			 G_CALLBACK (sweep_quit_ok_cb), NULL, G_CALLBACK (sweep_quit_cancel_cb), NULL,
			 SWEEP_EDIT_MODE_READY);
  } else if (any_playing()) {
    question_dialog_new (NULL, _("Files playing"),
			 _("No files are unsaved, but some files are "
			   "currently playing.\n\n"
			   "Are you sure you want to quit?"),
			 _("Quit"), _("Don't quit"),
			 G_CALLBACK (sweep_quit_ok_cb), NULL, G_CALLBACK (sweep_quit_cancel_cb), NULL,
			 SWEEP_EDIT_MODE_READY);
  } else {
    sweep_quit_ok_cb (NULL, NULL);
  }
}

/* info dialog */


void
sample_refresh_views (sw_sample * s)
{
  sw_view * v;
  GList * gl;

  g_mutex_lock (s->ops_mutex);

  sample_info_update (s);

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    view_refresh (v);
  }

  g_mutex_unlock (s->ops_mutex);
}

void
sample_start_marching_ants (sw_sample * s)
{
  sw_view * v;
  GList * gl;

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    sample_display_start_marching_ants (SAMPLE_DISPLAY(v->display));
  }
}

void
sample_stop_marching_ants (sw_sample * s)
{
  sw_view * v;
  GList * gl;

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    sample_display_stop_marching_ants (SAMPLE_DISPLAY(v->display));
  }
}

/* Edit state */


static void
_sample_set_edit_mode (sw_sample * s, sw_edit_mode edit_mode)
{
  GList * gl;
  sw_view * v;

  s->edit_mode = edit_mode;

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    view_refresh_edit_mode (v);
  }

  undo_dialog_refresh_edit_mode (s);
}

void
sample_set_edit_mode (sw_sample * s, sw_edit_mode edit_mode)
{
  g_mutex_lock (s->edit_mutex);

  _sample_set_edit_mode (s, edit_mode);

#ifdef DEBUG
  g_print ("set_edit_mode %d\n", edit_mode);
#endif

  g_mutex_unlock (s->edit_mutex);
}

void
sample_set_edit_state (sw_sample * s, sw_edit_state edit_state)
{
  g_mutex_lock (s->edit_mutex);

  s->edit_state = edit_state;

#ifdef DEBUG
  g_print ("set_edit_state %d\n", edit_state);
#endif

  if (edit_state == SWEEP_EDIT_STATE_IDLE) {
    _sample_set_edit_mode (s, SWEEP_EDIT_MODE_READY);
  }

  g_mutex_unlock (s->edit_mutex);

  if (edit_state == SWEEP_EDIT_STATE_PENDING) {
    g_cond_signal (s->pending_cond);
  }
}

/* Playback state */

void
sample_refresh_playmode (sw_sample * s)
{
  GList * gl;
  sw_view * v;
  sw_head * head = s->play_head;

  if (!head->going) {
    if (s->playmarker_tag > 0) {
      g_source_remove (s->playmarker_tag);
    }
  }

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    view_refresh_playmode (v);
  }
}

void
sample_set_stop_offset (sw_sample * s)
{
  sw_framecount_t offset;

  g_mutex_lock (s->play_mutex);

  offset = s->user_offset;
  if (offset == s->sounddata->nr_frames)
    offset = 0;

  head_set_stop_offset (s->play_head, offset);
  /*  s->play_head->stop_offset = offset;*/

  g_mutex_unlock (s->play_mutex);
}

void
sample_set_playmarker (sw_sample * s, sw_framecount_t offset,
		       gboolean by_user)
{
  GList * gl;
  sw_view * v;
  sw_head * head = s->play_head;

#ifdef DEBUG
  g_print ("sample_set_playmarker (%p, %d, %s)\n", s, offset,
	   by_user ? "TRUE" : "FALSE");
#endif

  g_mutex_lock (s->play_mutex);

  if (offset < 0) offset = 0;
  if (offset > s->sounddata->nr_frames) offset = s->sounddata->nr_frames;

  if (by_user) {
    s->by_user = by_user;
    s->user_offset = offset;
    if (!head->going || head->scrubbing == FALSE) {
      head_set_stop_offset (head, offset);
      head_set_offset (head, offset);
    }
  } else {
    /*
    if (head->scrubbing == FALSE)
      head_set_offset (head, offset);
    */
  }

  g_mutex_unlock (s->play_mutex);

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    view_refresh_offset_indicators (v);
  }
}

void
sample_set_offset_next_bound_left (sw_sample * s)
{
  GList * gl;
  sw_sel * sel;

  for (gl = g_list_last (s->sounddata->sels); gl; gl = gl->prev) {
    sel = (sw_sel *)gl->data;
    if (sel->sel_end < s->user_offset) {
      sample_set_playmarker (s, sel->sel_end, TRUE);
      return;
    }
    if (sel->sel_start < s->user_offset) {
      sample_set_playmarker (s, sel->sel_start, TRUE);
      return;
    }
  }
}

void
sample_set_offset_next_bound_right (sw_sample * s)
{
  GList * gl;
  sw_sel * sel;

  for (gl = s->sounddata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;
    if (sel->sel_start > s->user_offset) {
      sample_set_playmarker (s, sel->sel_start, TRUE);
      return;
    }
    if (sel->sel_end > s->user_offset) {
      sample_set_playmarker (s, sel->sel_end, TRUE);
      return;
    }
  }
}

void
sample_set_rec_marker (sw_sample * s, sw_framecount_t offset)
{
  GList * gl;
  sw_view * v;

  g_assert (s->rec_head != NULL);

  head_set_offset (s->rec_head, (gdouble)offset);

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    view_refresh_rec_offset_indicators (v);
  }

}

void
sample_refresh_rec_marker (sw_sample * s)
{
  GList * gl;
  sw_view * v;

  g_assert (s->rec_head != NULL);

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    view_refresh_rec_offset_indicators (v);
  }
}

void
sample_set_scrubbing (sw_sample * s, gboolean scrubbing)
{
  sw_head * head = s->play_head;

  head_set_scrubbing (head, scrubbing);

  sample_set_progress_ready (s);
}

void
sample_set_looping (sw_sample * s, gboolean looping)
{
  GList * gl;
  sw_view * v;
  sw_head * head = s->play_head;

  head_set_looping (head, looping);
  
  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    view_refresh_looping (v);
  }
}

void
sample_set_playrev (sw_sample * s, gboolean reverse)
{
  GList * gl;
  sw_view * v;
  sw_head * head = s->play_head;

  head_set_reverse (head, reverse);

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    view_refresh_playrev (v);
  }
}

void
sample_set_mute (sw_sample * s, gboolean mute)
{
  GList * gl;
  sw_view * v;
  sw_head * head = s->play_head;

  head_set_mute (head, mute);

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    view_refresh_mute (v);
  }
}

void
sample_set_monitor (sw_sample * s, gboolean monitor)
{
  GList * gl;
  sw_view * v;
  sw_head * head = s->play_head;

  head_set_monitor (head, monitor);

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    view_refresh_monitor (v);
  }
}

void
sample_set_previewing (sw_sample * s, gboolean previewing)
{
  head_set_previewing (s->play_head, previewing);
}

void
sample_set_color (sw_sample * s, gint color)
{
  GList * gl;
  sw_view * v;

  if (color < 0 || color > VIEW_COLOR_MAX) return;

  s->color = color;

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    view_refresh_display (v);
  }
}

void
sample_set_progress_text (sw_sample * s, gchar * text)
{
  GList * gl;
  sw_view * v;

  s->tmp_message_active = FALSE;

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    view_set_progress_text (v, text);
  }
}

void
sample_set_progress_percent (sw_sample * s, gint percent)
{
  s->progress_percent = CLAMP (percent, 0, 100);
}

void
sample_refresh_progress_percent (sw_sample * s)
{
  GList * gl;
  sw_view * v;

  if (s->edit_state == SWEEP_EDIT_STATE_IDLE) return;

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    view_set_progress_percent (v, s->progress_percent);
  }
}

int
sample_set_progress_ready (sw_sample * s)
{
  GList * gl;
  sw_view * v;

  if (s->edit_state != SWEEP_EDIT_STATE_IDLE) return FALSE;

  if (s->tmp_message_active) {
    for(gl = s->views; gl; gl = gl->next) {
      v = (sw_view *)gl->data;
      view_set_tmp_message (v, s->last_tmp_message);
    }
  } else {
    for(gl = s->views; gl; gl = gl->next) {
      v = (sw_view *)gl->data;
      view_set_progress_ready (v);
    }
  }

  return FALSE; /* for use as a one-shot timeout function */
}

gint
sample_clear_tmp_message (gpointer data)
{
  sw_sample * sample = (sw_sample *)data;

  sample->tmp_message_active = FALSE;

  if (sample->edit_state == SWEEP_EDIT_STATE_IDLE)
    sample_set_progress_ready (sample);

  g_free (sample->last_tmp_message);
  sample->last_tmp_message = NULL;

  sample->tmp_message_tag = -1;

  return FALSE;
}

void
sample_set_tmp_message (sw_sample * s, const char * fmt, ...)
{
  va_list ap;
  char buf[512];

  va_start (ap, fmt);
  vsnprintf (buf, sizeof (buf), fmt, ap);
  va_end (ap);

  s->tmp_message_active = TRUE;
  s->last_tmp_message = g_strdup (buf);

  if (s->tmp_message_tag != -1) {
    sweep_timeout_remove (s->tmp_message_tag);
  }

  s->tmp_message_tag =
    sweep_timeout_add ((guint32)5000,
		       (GtkFunction)sample_clear_tmp_message, s);

  sweep_timeout_add ((guint32)0, (GtkFunction)sample_set_progress_ready, s);
}

/*
 * sample_replace_throughout (os, s)
 *
 * Replaces os with s throughout the program, ie:
 *   - in the sample bank
 *   - in all os's views
 *
 * Destroys os.
 *
 * This function is not needed in general filters because sw_sample
 * pointers are persistent across sounddata modifications.
 * However, this function is still required where the entire sample
 * changes but the view must stay the same, eg. for File->Revert.
 */
void
sample_replace_throughout (sw_sample * os, sw_sample * s)
{
  sw_view * v;
  GList * gl;

  if (os == s) return;

  if ((!os) || (!s)) return;

  s->views = os->views;

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    v->sample = s;
    /* view_refresh (v); */
  }

  sample_bank_remove (os);
  sample_bank_add (s);
}

gboolean
sample_offset_in_sel (sw_sample * s, sw_framecount_t offset)
{
  GList * gl;
  sw_sel * sel;

  for (gl = s->sounddata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    if (sel->sel_start <= offset && sel->sel_end >= offset)
      return TRUE;
  }

  return FALSE;
}

guint
sample_sel_nr_regions (sw_sample * s)
{
  return g_list_length (s->sounddata->sels);
}

void
sample_clear_selection (sw_sample * s)
{
  sounddata_clear_selection (s->sounddata);

  sample_stop_marching_ants (s);
}

static void
sample_normalise_selection (sw_sample * s)
{
  sounddata_normalise_selection (s->sounddata);
}

void
sample_add_selection (sw_sample * s, sw_sel * sel)
{
  if (!s->sounddata->sels)
    sample_start_marching_ants (s);

  sounddata_add_selection (s->sounddata, sel);
}

sw_sel *
sample_add_selection_1 (sw_sample * s, sw_framecount_t start, sw_framecount_t end)
{
  return sounddata_add_selection_1 (s->sounddata, start, end);
}

void
sample_set_selection (sw_sample * s, GList * gl)
{
  sample_clear_selection(s);

  s->sounddata->sels = sels_copy (gl);
}

sw_sel *
sample_set_selection_1 (sw_sample * s, sw_framecount_t start, sw_framecount_t end)
{
  return sounddata_set_selection_1 (s->sounddata, start, end);
}

void
sample_selection_modify (sw_sample * s, sw_sel * sel,
			 sw_framecount_t new_start, sw_framecount_t new_end)
{
  sel->sel_start = new_start;
  sel->sel_end = new_end;

  sample_normalise_selection (s);
}

static sw_sample *
ss_invert (sw_sample * s, sw_param_set unused, gpointer unused2)
{
  sw_sounddata * sounddata = s->sounddata;
  GList * gl;
  GList * osels;
  sw_sel * osel, * sel;

  g_mutex_lock (sounddata->sels_mutex);

  if (!sounddata->sels) {
    sounddata_set_selection_1 (sounddata, 0, sounddata->nr_frames);
    goto out;
  }

  gl = osels = sounddata->sels;
  sounddata->sels = NULL;

  sel = osel = (sw_sel *)gl->data;
  if (osel->sel_start > 0) {
    sounddata_add_selection_1 (sounddata, 0, osel->sel_start - 1);
  }

  gl = gl->next;

  for (; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;
    sounddata_add_selection_1 (sounddata, osel->sel_end, sel->sel_start - 1);
    osel = sel;
  }

  if (sel->sel_end != sounddata->nr_frames) {
    sounddata_add_selection_1 (sounddata, sel->sel_end, sounddata->nr_frames);
  }

  g_list_free (osels);

out:

  g_mutex_unlock (sounddata->sels_mutex);

  return s;
}

void
sample_selection_invert (sw_sample * s)
{
  perform_selection_op (s, _("Invert selection"), ss_invert, NULL, NULL);
}

static sw_sample *
ss_select_all (sw_sample * s, sw_param_set unused, gpointer unused2)
{
  sw_sounddata * sounddata = s->sounddata;

  g_mutex_lock (sounddata->sels_mutex);

  sounddata_set_selection_1 (sounddata, 0, sounddata->nr_frames);

  g_mutex_unlock (sounddata->sels_mutex);

  return s;
}

void
sample_selection_select_all (sw_sample * s)
{
  perform_selection_op (s, _("Select all"), ss_select_all, NULL, NULL);
}

static sw_sample *
ss_select_none (sw_sample * s, sw_param_set unused, gpointer unused2)
{
  sw_sounddata * sounddata = s->sounddata;

  g_mutex_lock (sounddata->sels_mutex);

  sounddata_clear_selection (sounddata);

  g_mutex_unlock (sounddata->sels_mutex);

  return s;
}

void
sample_selection_select_none (sw_sample * s)
{
  perform_selection_op (s, _("Select none"), ss_select_none, NULL, NULL);
}

static sw_sample *
ss_halve (sw_sample * s, sw_param_set unused, gpointer unused2)
{
  sw_sounddata * sounddata = s->sounddata;

  g_mutex_lock (sounddata->sels_mutex);

  sounddata_selection_scale (sounddata, 0.5);

  g_mutex_unlock (sounddata->sels_mutex);

  return s;
}

void
sample_selection_halve (sw_sample * s)
{
  perform_selection_op (s, _("Halve selection"), ss_halve, NULL, NULL);
}

static sw_sample *
ss_double (sw_sample * s, sw_param_set unused, gpointer unused2)
{
  sw_sounddata * sounddata = s->sounddata;

  g_mutex_lock (sounddata->sels_mutex);

  sounddata_selection_scale (sounddata, 2.0);

  g_mutex_unlock (sounddata->sels_mutex);

  return s;
}

void
sample_selection_double (sw_sample * s)
{
  perform_selection_op (s, _("Double selection"), ss_double, NULL, NULL);
}

static sw_sample *
ss_shift_left (sw_sample * s, sw_param_set unused, gpointer unused2)
{
  sw_sounddata * sounddata = s->sounddata;

  sw_framecount_t delta;

  g_mutex_lock (sounddata->sels_mutex);

  delta = - (sounddata_selection_width (sounddata));
  sounddata_selection_translate (sounddata, delta);

  g_mutex_unlock (sounddata->sels_mutex);

  return s;
}

void
sample_selection_shift_left (sw_sample * s)
{
  perform_selection_op (s, _("Selection left"), ss_shift_left, NULL, NULL);
}

static sw_sample *
ss_shift_right (sw_sample * s, sw_param_set unused, gpointer unused2)
{
  sw_sounddata * sounddata = s->sounddata;
  sw_framecount_t delta;

  g_mutex_lock (sounddata->sels_mutex);

  delta = (sounddata_selection_width (sounddata));
  sounddata_selection_translate (sounddata, delta);

  g_mutex_unlock (sounddata->sels_mutex);

  return s;
}

void
sample_selection_shift_right (sw_sample * s)
{
  perform_selection_op (s, _("Selection right"), ss_shift_right, NULL, NULL);
}

/*
 * Functions to handle the temporary selection
 */

void
sample_clear_tmp_sel (sw_sample * s)
{
 if (s->tmp_sel) g_free (s->tmp_sel);
 s->tmp_sel = NULL;
 last_tmp_view = NULL;
}


/* 
 * sample_set_tmp_sel (s, tsel)
 *
 * sets the tmp_sel of sample s to a list containing only tsel.
 * If tsel was part of the actual selection of s, it is first
 * removed from the selection.
 */
void
sample_set_tmp_sel (sw_sample * s, sw_view * tview, sw_sel * tsel)
{
  GList * gl;
  sw_sel * sel;

  /* XXX: Dump old tmp_sel */
  sample_clear_tmp_sel (s);

  s->tmp_sel =
    sel_copy (tsel); /* XXX: try to do this without copying? */
  last_tmp_view = tview;

  g_mutex_lock (s->sounddata->sels_mutex);

  for(gl = s->sounddata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    if(sel == tsel) {
      s->sounddata->sels = g_list_remove(s->sounddata->sels, sel);
    }
  }

  g_mutex_unlock (s->sounddata->sels_mutex);
}

void
sample_set_tmp_sel_1 (sw_sample * s, sw_view * tview, sw_framecount_t start, sw_framecount_t end)
{
  sw_sel * tsel;

  tsel = sel_new (start, end);

  sample_set_tmp_sel (s, tview, tsel);
}

/* For convenience, as the tmp_sel handling is internal to the application,
 * we pass the actual sw_sample pointer to the tmp_sel handling ops.
 */

static sw_sample *
ssits (sw_sample * s, sw_param_set unused, gpointer data)
{
  sw_sel * sel = (sw_sel *)data;

  g_mutex_lock (s->sounddata->sels_mutex);

  sample_add_selection (s, sel);
  sample_normalise_selection (s);

  g_mutex_unlock (s->sounddata->sels_mutex);

  return s;
}

void
sample_selection_insert_tmp_sel (sw_sample * s)
{
  int n;
  gchar buf[64];
  sw_format * format = s->sounddata->format;
  sw_sel * sel;

  n = snprintf (buf, sizeof (buf), _("Insert selection ["));
  n += snprint_time (buf+n, sizeof (buf)-n,
		     frames_to_time (format, s->tmp_sel->sel_start));
  n += snprintf (buf+n, sizeof (buf)-n, " - ");
  n += snprint_time (buf+n, sizeof (buf)-n,
		     frames_to_time (format, s->tmp_sel->sel_end));
  n += snprintf (buf+n, sizeof (buf)-n, "]");

  g_mutex_lock (s->sounddata->sels_mutex);

  sel = sel_copy (s->tmp_sel);

  s->tmp_sel = NULL;
  last_tmp_view = NULL;

  g_mutex_unlock (s->sounddata->sels_mutex);
  
  perform_selection_op (s, buf, ssits, NULL, sel);
}

static sw_sample *
sssts (sw_sample * s, sw_param_set unused, gpointer data)
{
  GList * sels;
  sw_sel * sel = (sw_sel *)data;

  g_mutex_lock (s->sounddata->sels_mutex);

  sels = s->sounddata->sels;

  sels = sels_invert (sels, s->sounddata->nr_frames);
  s->sounddata->sels = sels_add_selection (sels, sel);

  sample_normalise_selection (s);
  sels = s->sounddata->sels;

  s->sounddata->sels = sels_invert (sels, s->sounddata->nr_frames);

  g_mutex_unlock (s->sounddata->sels_mutex);

  return s;
}

void
sample_selection_subtract_tmp_sel (sw_sample * s)
{
  int n;
  gchar buf[64];
  sw_format * format = s->sounddata->format;
  sw_sel * sel;

  n = snprintf (buf, sizeof (buf), _("Subtract selection ["));
  n += snprint_time (buf+n, sizeof (buf)-n,
		     frames_to_time (format, s->tmp_sel->sel_start));
  n += snprintf (buf+n, sizeof (buf)-n, " - ");
  n += snprint_time (buf+n, sizeof (buf)-n,
		     frames_to_time (format, s->tmp_sel->sel_end));
  n += snprintf (buf+n, sizeof (buf)-n, "]");

  g_mutex_lock (s->sounddata->sels_mutex);

  sel = sel_copy (s->tmp_sel);

  s->tmp_sel = NULL;
  last_tmp_view = NULL;

  g_mutex_unlock (s->sounddata->sels_mutex);
  
  perform_selection_op (s, buf, sssts, NULL, sel);
}

static sw_sample *
ssrwts (sw_sample * s, sw_param_set unused, gpointer data)
{
  sw_sel * sel = (sw_sel *)data;

  g_mutex_lock (s->sounddata->sels_mutex);

  sample_clear_selection (s);

  sample_add_selection (s, sel);

  g_mutex_unlock (s->sounddata->sels_mutex);

  return s;
}

void
sample_selection_replace_with_tmp_sel (sw_sample * s)
{
  int n;
  gchar buf[64];
  sw_format * format = s->sounddata->format;
  sw_sel * sel;

  n = snprintf (buf, sizeof (buf), _("Set selection ["));
  n += snprint_time (buf+n, sizeof (buf)-n,
		     frames_to_time (format, s->tmp_sel->sel_start));
  n += snprintf (buf+n, sizeof (buf)-n, " - ");
  n += snprint_time (buf+n, sizeof (buf)-n,
		     frames_to_time (format, s->tmp_sel->sel_end));
  n += snprintf (buf+n, sizeof (buf)-n, "]");

  g_mutex_lock (s->sounddata->sels_mutex);

  sel = sel_copy (s->tmp_sel);

  s->tmp_sel = NULL;
  last_tmp_view = NULL;

  g_mutex_unlock (s->sounddata->sels_mutex);

  perform_selection_op (s, buf, ssrwts, NULL, sel);
}


/* Sample info dialog */

static void
sample_info_update (sw_sample * sample)
{
  sw_sounddata * sounddata = sample->sounddata;
  GtkWidget * clist = sample->info_clist;

  char rate_buf[16];
  char chan_buf[16];
  char byte_buf[16];
  char time_buf[16];

  if (clist == NULL) return;

  snprintf (rate_buf, sizeof (rate_buf), "%d Hz", sounddata->format->rate);

  snprintf (chan_buf, sizeof (chan_buf), "%d", sounddata->format->channels);

  snprint_bytes (byte_buf, sizeof (byte_buf),
		 frames_to_bytes (sounddata->format, sounddata->nr_frames));
  
  snprint_time (time_buf, sizeof (time_buf),
		frames_to_time (sounddata->format, sounddata->nr_frames));


  gtk_clist_set_text (GTK_CLIST(clist), 0, 1, g_basename(sample->pathname));
  gtk_clist_set_text (GTK_CLIST(clist), 1, 1, rate_buf);
  gtk_clist_set_text (GTK_CLIST(clist), 2, 1, chan_buf);
  gtk_clist_set_text (GTK_CLIST(clist), 3, 1, byte_buf);
  gtk_clist_set_text (GTK_CLIST(clist), 4, 1, time_buf);

}

static void
sample_info_dialog_destroy_cb (GtkWidget * widget, gpointer data)
{
  sw_sample * sample = (sw_sample *)data;

  sample->info_clist = NULL;
}

static void
sample_info_dialog_ok_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;

  dialog = gtk_widget_get_toplevel (widget);
  gtk_widget_hide (dialog);
}

/*
static gchar * filename_info[] = { N_("Filename:"), "" };
static gchar * rate_info[] = { N_("Sampling rate:"), "" };
static gchar * channels_info[] = { N_("Channels:"), "" };
static gchar * size_info[] = { N_("Data memory:"), "" };
static gchar * duration_info[] = { N_("Duration:"), "" };
*/

void
sample_show_info_dialog (sw_sample * sample)
{
  GtkWidget * dialog;
  GtkWidget * clist;
  GtkWidget * ok_button;
  gchar * list_item[] = { "" };
  gint i=0;

  if (sample->info_clist == NULL) {
    dialog = gtk_dialog_new ();
    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_title (GTK_WINDOW(dialog), _("Sweep: File properties"));
    gtk_container_set_border_width (GTK_CONTAINER(dialog), 8);

    g_signal_connect (G_OBJECT(dialog), "destroy",
			G_CALLBACK(sample_info_dialog_destroy_cb),
			sample);
    
    clist = gtk_clist_new (2);
    gtk_clist_set_selection_mode (GTK_CLIST(clist), GTK_SELECTION_BROWSE);
    gtk_clist_set_column_justification (GTK_CLIST(clist), 0,
					GTK_JUSTIFY_RIGHT);
    gtk_clist_set_column_justification (GTK_CLIST(clist), 1,
					GTK_JUSTIFY_LEFT);
    gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), clist,
			FALSE, FALSE, 0);
    /*    
    gtk_clist_append (GTK_CLIST(clist), filename_info);
    gtk_clist_append (GTK_CLIST(clist), rate_info);
    gtk_clist_append (GTK_CLIST(clist), channels_info);
    gtk_clist_append (GTK_CLIST(clist), size_info);
    gtk_clist_append (GTK_CLIST(clist), duration_info);
    */
     
    gtk_clist_append (GTK_CLIST(clist), list_item);
    gtk_clist_set_text (GTK_CLIST(clist), i++, 0, _("Filename: "));
    gtk_clist_append (GTK_CLIST(clist), list_item);
    gtk_clist_set_text (GTK_CLIST(clist), i++, 0, _("Sampling rate: "));
    gtk_clist_append (GTK_CLIST(clist), list_item);
    gtk_clist_set_text (GTK_CLIST(clist), i++, 0, _("Channels: "));
    gtk_clist_append (GTK_CLIST(clist), list_item);
    gtk_clist_set_text (GTK_CLIST(clist), i++, 0, _("Data memory: "));
    gtk_clist_append (GTK_CLIST(clist), list_item);
    gtk_clist_set_text (GTK_CLIST(clist), i++, 0, _("Duration: "));
    
    gtk_clist_set_column_min_width (GTK_CLIST(clist), 0, 120);
    gtk_clist_set_column_min_width (GTK_CLIST(clist), 1, 160);
    
    gtk_widget_show (clist);

    sample->info_clist = clist;

    /* OK */
    
    ok_button = gtk_button_new_with_label (_("OK"));
    GTK_WIDGET_SET_FLAGS (GTK_WIDGET (ok_button), GTK_CAN_DEFAULT);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), ok_button,
			TRUE, TRUE, 0);
    gtk_widget_show (ok_button);
    g_signal_connect (G_OBJECT(ok_button), "clicked",
			G_CALLBACK (sample_info_dialog_ok_cb), sample);
    
    gtk_widget_grab_default (ok_button);
    
  } else {
    dialog = gtk_widget_get_toplevel (sample->info_clist);
  }

  sample_info_update (sample);

  if (!GTK_WIDGET_VISIBLE(dialog)) {
    gtk_widget_show (dialog);
  } else {
    gdk_window_raise (dialog->window);
  }
}
