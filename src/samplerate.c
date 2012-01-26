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

#ifdef HAVE_LIBSAMPLERATE

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <samplerate.h>

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_types.h>
#include <sweep/sweep_typeconvert.h>
#include <sweep/sweep_undo.h>
#include <sweep/sweep_filter.h>
#include <sweep/sweep_sounddata.h>
#include <sweep/sweep_sample.h>

#include "sweep_app.h"
#include "edit.h"
#include "interface.h"
#include "preferences.h"
#include "question_dialogs.h"
#include "sw_chooser.h"

#include "../pixmaps/SRC.xpm"

/*#define DEBUG*/

#define BUFFER_LEN 4096

#define QUALITY_KEY "SRC_Quality"

/* assume libsamplerate's best quality is indexed 0 */
#define DEFAULT_QUALITY 0

extern GtkStyle * style_wb;

typedef struct {
  int new_rate;
  int quality;
} src_options;

static void
do_samplerate_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  src_options * so = (src_options *)inst->do_data;
  int new_rate, quality;
  double src_ratio;

  GList * gl;
  sw_sel * sel;

  sw_format * old_format = sample->sounddata->format;
  sw_sounddata * old_sounddata, * new_sounddata;
  sw_framecount_t old_nr_frames, new_nr_frames;

  SRC_STATE * src_state;
  SRC_DATA src_data;
  int error;

  sw_framecount_t remaining, offset_in, offset_out, run_total, ctotal;
  int percent;
#ifdef DEBUG
  int iter = 0;
#endif

  gboolean active = TRUE;

  if (so == NULL) return;

  quality = so->quality;
  new_rate = so->new_rate;
  g_free (so);

  if ((src_state = src_new (quality, old_format->channels, &error)) == NULL) {
    info_dialog_new (_("Resample error"), NULL,
		     "%s: %s", _("libsamplerate error"),
		     src_strerror (error));
    return;
  }

  src_ratio = (double)new_rate / (double)old_format->rate;

  src_data.end_of_input = 0;
  src_data.src_ratio = src_ratio;

  old_sounddata = sample->sounddata;
  old_nr_frames = old_sounddata->nr_frames;

  new_nr_frames = floor (old_nr_frames * src_ratio) ;
  new_sounddata = sounddata_new_empty (old_format->channels, new_rate,
				       new_nr_frames);

  remaining = new_nr_frames /* * old_format->channels*/ ;
  ctotal = remaining / 100;
  if (ctotal == 0) ctotal = 1;
  run_total = 0;
  offset_in = 0, offset_out = 0;

  /* Create selections */
  g_mutex_lock (sample->ops_mutex);
  for (gl = old_sounddata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    sounddata_add_selection_1 (new_sounddata, sel->sel_start * src_ratio,
			       sel->sel_end * src_ratio);
  }
  g_mutex_unlock (sample->ops_mutex);

  /* XXX: move play/rec offsets */

  /* Resample data */
  while (active) {
    g_mutex_lock (sample->ops_mutex);

    if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
      active = FALSE;
    } else {

      src_data.input_frames = MIN (old_nr_frames - offset_in, BUFFER_LEN);
      src_data.data_in = &((float *)old_sounddata->data)
	[offset_in * old_format->channels];

      src_data.output_frames = MAX (new_nr_frames - offset_out, 0);
      src_data.data_out = &((float *)new_sounddata->data)
	[offset_out * old_format->channels];

      if (src_data.input_frames < BUFFER_LEN) {
	src_data.end_of_input = TRUE;
      }

#ifdef DEBUG
      printf ("%d:\t%ld/%d in\t%ld/%d out\t", iter++,
	      offset_in + src_data.input_frames, old_nr_frames,
	      src_data.output_frames, new_nr_frames);
#endif

      if ((error = src_process (src_state, &src_data))) {
	info_dialog_new (_("Resample error"), NULL,
			 "%s: %s", _("libsamplerate error"),
			 src_strerror (error));
	active = FALSE;
      } else {

	remaining -= (sw_framecount_t)src_data.output_frames_gen;
	run_total += (sw_framecount_t)src_data.output_frames_gen;

	offset_in += (sw_framecount_t)src_data.input_frames_used;
	offset_out += (sw_framecount_t)src_data.output_frames_gen;

    /* This is the normal loop exit point. */
	if (src_data.output_frames_gen == 0) {
	    active = FALSE;
	}

#ifdef DEBUG
	printf ("%ld ->\t%ld\t(%d)\n", src_data.input_frames_used,
		src_data.output_frames_gen, remaining);
#endif

	percent = run_total / ctotal;
	sample_set_progress_percent (sample, percent);
      }
    }

    g_mutex_unlock (sample->ops_mutex);
  }

  /* Only an error if remaining > 1 */
  if (remaining > 1) { /* cancelled or failed */
    sounddata_destroy (new_sounddata);
  } else if (sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    /* Set real number of frames. */
    new_sounddata->nr_frames = run_total ;

    sample->sounddata = new_sounddata;

    inst->redo_data = inst->undo_data =
      sounddata_replace_data_new (sample, old_sounddata, new_sounddata);

    register_operation (sample, inst);
  }

  /*sample_set_edit_state (sample, SWEEP_EDIT_STATE_DONE);*/
}

static sw_operation samplerate_op = {
  SWEEP_EDIT_MODE_ALLOC,
  (SweepCallback)do_samplerate_thread,
  (SweepFunction)NULL,
  (SweepCallback)undo_by_sounddata_replace,
  (SweepFunction)sounddata_replace_data_destroy,
  (SweepCallback)redo_by_sounddata_replace,
  (SweepFunction)sounddata_replace_data_destroy
};

void
resample (sw_sample * sample, int new_rate, int quality)
{
  src_options * so;
  char buf[128];

  g_snprintf (buf, sizeof (buf), _("Resample from %d Hz to %d Hz"),
	      sample->sounddata->format->rate, new_rate);

  so = g_malloc (sizeof(src_options));
  so->new_rate = new_rate;
  so->quality = quality;

  schedule_operation (sample, buf, &samplerate_op, so);
}

static void
samplerate_dialog_ok_cb (GtkWidget * widget, gpointer data)
{
  sw_sample * sample = (sw_sample *)data;
  GtkWidget * dialog;
  GtkWidget * chooser;
  GtkWidget * quality_menu;
  GtkWidget * menu;
  GtkWidget * menuitem;
  GtkWidget * checkbutton;

  int new_rate;
  int quality;
  gboolean rem_quality;

  dialog = gtk_widget_get_toplevel (widget);

  chooser = g_object_get_data (G_OBJECT(dialog), "default");
  new_rate = samplerate_chooser_get_rate (chooser);

  quality_menu =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "quality_menu"));
  menu = gtk_option_menu_get_menu (GTK_OPTION_MENU(quality_menu));
  menuitem = gtk_menu_get_active (GTK_MENU(menu));
  quality = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(menuitem), "default"));

  checkbutton =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "rem_quality_chb"));
  rem_quality =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton));

  gtk_widget_destroy (dialog);

  if (rem_quality) {
    prefs_set_int (QUALITY_KEY, quality);
  }

  resample (sample, new_rate, quality);

  sample_set_edit_state (sample, SWEEP_EDIT_STATE_IDLE);
}

static void
samplerate_dialog_cancel_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;
  sw_sample * sample = (sw_sample *)data;

  dialog = gtk_widget_get_toplevel (widget);
  gtk_widget_destroy (dialog);

  sample_set_edit_state (sample, SWEEP_EDIT_STATE_IDLE);
}

static gboolean samplerate_dialog_delete_event_cb( GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   data )
{
    samplerate_dialog_cancel_cb(widget, data);
    return FALSE;
}

static void
src_update_ok_button (GtkWidget * widget)
{
  GtkWidget * dialog;
  GtkWidget * chooser;
  GtkWidget * ok_button;
  int old_rate, new_rate;
  double ratio;
  int is_valid = 0;

  dialog = gtk_widget_get_toplevel (widget);

  old_rate =
    GPOINTER_TO_INT(g_object_get_data (G_OBJECT(dialog), "old_rate"));

  chooser = g_object_get_data (G_OBJECT(dialog), "default");
  new_rate = samplerate_chooser_get_rate (chooser);

  if (old_rate > 0 && new_rate > 0 && old_rate != new_rate) {
    ratio = (double)new_rate / (double)old_rate;
    is_valid = src_is_valid_ratio (ratio);
  }

  ok_button =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "ok_button"));

  gtk_widget_set_sensitive (ok_button, is_valid);
}

static void
src_quality_label_update (GtkWidget * dialog, int quality)
{
  GtkWidget * label;
  const char * desc;
  gchar * new_text, * c;

  label =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "quality_label"));

  desc = src_get_description (quality) ;
  if (desc == NULL)
    return ;

  new_text = g_strdup (desc);

  /* replace commas in description with newline characters */
  for (c = new_text; *c != '\0'; c++) {
    if (*c == ',') {
      *c = '\n';
    }
  }

  gtk_label_set_text (GTK_LABEL(label), new_text);

  g_free (new_text);
}

static void
src_quality_label_update_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;
  int quality;

  dialog = GTK_WIDGET(data);

  quality = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(widget), "default"));

  src_quality_label_update (dialog, quality);
}

static void
src_quality_options_reset_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;
  GtkWidget * quality_menu;

  int quality;

  dialog = gtk_widget_get_toplevel (widget);

  /* Quality */

  quality_menu =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "quality_menu"));

  quality =prefs_get_int (QUALITY_KEY, DEFAULT_QUALITY);

  gtk_option_menu_set_history (GTK_OPTION_MENU(quality_menu), quality);
  src_quality_label_update (dialog, quality);
}

static void
src_quality_options_default_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;
  GtkWidget * quality_menu;

  dialog = gtk_widget_get_toplevel (widget);

  /* Quality */

  quality_menu =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "quality_menu"));

  gtk_option_menu_set_history (GTK_OPTION_MENU(quality_menu), DEFAULT_QUALITY);
  src_quality_label_update (dialog, DEFAULT_QUALITY);
}

void
samplerate_dialog_new_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;
  GtkWidget * dialog;
  GtkWidget * main_vbox;
  GtkWidget * ebox;
  GtkWidget * pixmap;
  GtkWidget * notebook;
  GtkWidget * vbox;
  GtkWidget * label;
  GtkWidget * chooser;
  GtkWidget * frame;
  GtkWidget * vbox2;
  GtkWidget * option_menu;
  GtkWidget * menu;
  GtkWidget * menuitem;
  GtkWidget * hbox, * hbox2;
  GtkWidget * checkbutton;
  GtkWidget * button, * ok_button;

  GtkTooltips * tooltips;

  char * desc;
  int i;

  gchar current [256];

  dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW(dialog), _("Sweep: Resample"));
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);

  g_signal_connect (G_OBJECT (dialog), "delete_event", G_CALLBACK (samplerate_dialog_delete_event_cb), sample);

  main_vbox = GTK_DIALOG(dialog)->vbox;

  ebox = gtk_event_box_new ();
  gtk_box_pack_start (GTK_BOX(main_vbox), ebox, FALSE, FALSE, 0);
  gtk_widget_set_style (ebox, style_wb);
  gtk_widget_show (ebox);

  pixmap = create_widget_from_xpm (dialog, SRC_xpm);
  gtk_container_add (GTK_CONTAINER(ebox), pixmap);
  gtk_widget_show (pixmap);

  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX(main_vbox), notebook, TRUE, TRUE, 4);
  gtk_widget_show (notebook);

  /* Conversion */

  label = gtk_label_new (_("Conversion"));

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), vbox, label);
  gtk_container_set_border_width (GTK_CONTAINER(vbox), 4);
  gtk_widget_show (vbox);

  snprintf (current, sizeof (current), _("Current sample rate: %d Hz"),
			     sample->sounddata->format->rate);
  label = gtk_label_new (current);
  gtk_box_pack_start (GTK_BOX(vbox), label, TRUE, TRUE, 8);
  gtk_widget_show (label);

  g_object_set_data (G_OBJECT(dialog), "old_rate",
		       GINT_TO_POINTER(sample->sounddata->format->rate));

  chooser = samplerate_chooser_new (_("New sample rate"));
  gtk_box_pack_start (GTK_BOX(vbox), chooser, TRUE, TRUE, 0);
  gtk_widget_show (chooser);

  g_signal_connect (G_OBJECT(chooser), "number-changed",
		      G_CALLBACK(src_update_ok_button), NULL);

  g_object_set_data (G_OBJECT(dialog), "default", chooser);

  /* Quality */

  label = gtk_label_new (_("Quality"));

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), vbox, label);
  gtk_container_set_border_width (GTK_CONTAINER(vbox), 4);
  gtk_widget_show (vbox);

  frame = gtk_frame_new (_("Converter"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(frame), vbox2);
  gtk_widget_show (vbox2);

  option_menu = gtk_option_menu_new ();
  gtk_box_pack_start (GTK_BOX (vbox2), option_menu, FALSE, TRUE, 0);
  gtk_widget_show (option_menu);

  menu = gtk_menu_new ();

  for (i = 0; (desc = (char *) src_get_name (i)) != NULL; i++) {
    menuitem = gtk_menu_item_new_with_label (desc);
    gtk_menu_append (GTK_MENU(menu), menuitem);
    g_object_set_data (G_OBJECT(menuitem), "default", GINT_TO_POINTER(i));
    gtk_widget_show (menuitem);

    g_signal_connect (G_OBJECT(menuitem), "activate",
			G_CALLBACK(src_quality_label_update_cb), dialog);
  }

  gtk_option_menu_set_menu (GTK_OPTION_MENU(option_menu), menu);

  g_object_set_data (G_OBJECT(dialog), "quality_menu", option_menu);

  /* long description ... */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (vbox2), label, TRUE, FALSE, 4);
  gtk_widget_show (label);

  g_object_set_data (G_OBJECT(dialog), "quality_label", label);

  /* Remember / Reset */

  hbox = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_end (GTK_BOX(vbox), hbox, FALSE, FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER(hbox), 12);
  gtk_widget_show (hbox);

  checkbutton =
    gtk_check_button_new_with_label (_("Remember this quality"));
  gtk_box_pack_start (GTK_BOX (hbox), checkbutton, TRUE, TRUE, 0);
  gtk_widget_show (checkbutton);

  g_object_set_data (G_OBJECT (dialog), "rem_quality_chb", checkbutton);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkbutton), TRUE);

  hbox2 = gtk_hbox_new (TRUE, 4);
  gtk_box_pack_start (GTK_BOX (hbox), hbox2, FALSE, TRUE, 0);
  gtk_widget_show (hbox2);

  button = gtk_button_new_with_label (_("Reset"));
  gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, TRUE, 4);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK(src_quality_options_reset_cb), NULL);
  gtk_widget_show (button);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, button,
			_("Reset to the last remembered quality."),
			NULL);

  /* Call the reset callback now to set remembered options */
  src_quality_options_reset_cb (button, NULL);

  button = gtk_button_new_with_label (_("Default"));
  gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, TRUE, 4);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK(src_quality_options_default_cb), NULL);
  gtk_widget_show (button);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, button,
			_("Set to default quality."),
			NULL);


  /* About */

  label = gtk_label_new (_("About"));

  ebox = gtk_event_box_new ();
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), ebox, label);
  gtk_widget_set_style (ebox, style_wb);
  gtk_widget_show (ebox);

  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK(notebook), ebox,
				      TRUE, TRUE, GTK_PACK_END);

  vbox = gtk_vbox_new (FALSE, 16);
  gtk_container_add (GTK_CONTAINER(ebox), vbox);
  gtk_container_set_border_width (GTK_CONTAINER(vbox), 8);
  gtk_widget_show (vbox);

  label =
    gtk_label_new (_("Secret Rabbit Code (aka libsamplerate) is a\n"
		     "Sample Rate Converter for audio by Erik de Castro Lopo\n"));

  gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_set_style (label, style_wb);
  gtk_widget_show (label);

  button = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, FALSE, 8);
  gtk_widget_show (button);

  label = gtk_label_new (_("This user interface by Conrad Parker,\n"
			   "Copyright (C) 2002 CSIRO Australia.\n\n"));
  gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_set_style (label, style_wb);
  gtk_widget_show (label);


  /* OK */

  ok_button = gtk_button_new_with_label (_("Resample"));
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (ok_button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), ok_button,
		      TRUE, TRUE, 0);
  gtk_widget_show (ok_button);
  g_signal_connect (G_OBJECT(ok_button), "clicked",
		      G_CALLBACK (samplerate_dialog_ok_cb),
		      sample);

  g_object_set_data (G_OBJECT (dialog), "ok_button", ok_button);

  /* Cancel */

  button = gtk_button_new_with_label (_("Don't resample"));
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), button,
		      TRUE, TRUE, 0);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK (samplerate_dialog_cancel_cb),
		      sample);

  gtk_widget_grab_default (ok_button);

  src_update_ok_button (dialog);

  sample_set_edit_state (sample, SWEEP_EDIT_STATE_BUSY);
  sample_set_edit_mode (sample, SWEEP_EDIT_MODE_FILTER);
  sample_set_progress_percent (sample, 0);

  gtk_widget_show (dialog);
}

#endif /* HAVE_LIBSAMPLERATE */
