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
#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_undo.h>
#include <sweep/sweep_sample.h>
#include <sweep/sweep_sounddata.h>
#include <sweep/sweep_selection.h>
#include <sweep/sweep_typeconvert.h>

#include "sweep_app.h"
#include "db_slider.h"
#include "edit.h"
#include "interface.h"
#include "print.h"

#include "../pixmaps/pastemix.xpm"
#include "../pixmaps/pastexfade.xpm"

/*#define DEBUG*/


#if 0
static GtkWidget *
create_pixmap_button (GtkWidget * widget, gchar ** xpm_data,
		      const gchar * label_text, const gchar * tip_text,
		      GtkSignalFunc clicked)
{
  GtkWidget * hbox;
  GtkWidget * label;
  GtkWidget * pixmap;
  GtkWidget * button;
  GtkTooltips * tooltips;

  button = gtk_button_new ();

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER(button), hbox);
  gtk_container_set_border_width (GTK_CONTAINER(button), 8);
  gtk_widget_show (hbox);

  if (xpm_data != NULL) {
    pixmap = create_widget_from_xpm (widget, xpm_data);
    gtk_box_pack_start (GTK_BOX(hbox), pixmap, FALSE, FALSE, 8);
    gtk_widget_show (pixmap);
  }

  if (label_text != NULL) {
    label = gtk_label_new (label_text);
    gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 8);
    gtk_widget_show (label);
  }

  if (tip_text != NULL) {
    tooltips = gtk_tooltips_new ();
    gtk_tooltips_set_tip (tooltips, button, tip_text, NULL);
  }

  if (clicked != NULL) {
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC(clicked), NULL);
  }

  return button;
}
#endif

static void
paste_dialog_destroy (GtkWidget * widget, gpointer data)
{
  sw_sample * sample = (sw_sample *)data;

  sample_set_edit_state (sample, SWEEP_EDIT_STATE_IDLE);
}

static void
paste_xfade_dialog_ok_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;
  GtkWidget * slider;
  GtkWidget * checkbutton;
  sw_sample * sample = (sw_sample *)data;
  gdouble src_gain_start, src_gain_end, dest_gain_start, dest_gain_end;

  dialog = gtk_widget_get_toplevel (widget);

  slider = gtk_object_get_data (GTK_OBJECT(dialog), "src_slider");
  src_gain_start = (double) db_slider_get_value (DB_SLIDER(slider));

  slider = gtk_object_get_data (GTK_OBJECT(dialog), "src_slider2");
  src_gain_end = (double) db_slider_get_value (DB_SLIDER(slider));

  checkbutton = gtk_object_get_data (GTK_OBJECT(dialog), "src_invert");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton))) {
    src_gain_start *= -1.0;
    src_gain_end *= -1.0;
  }

  slider = gtk_object_get_data (GTK_OBJECT(dialog), "dest_slider");
  dest_gain_start = (double) db_slider_get_value (DB_SLIDER(slider));

  slider = gtk_object_get_data (GTK_OBJECT(dialog), "dest_slider2");
  dest_gain_end = (double) db_slider_get_value (DB_SLIDER(slider));

  checkbutton = gtk_object_get_data (GTK_OBJECT(dialog), "dest_invert");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton))) {
    dest_gain_start *= -1.0;
    dest_gain_end *= -1.0;
  }

  gtk_widget_hide (dialog);

  do_paste_xfade (sample, src_gain_start, src_gain_end, dest_gain_start,
		  dest_gain_end);

  sample_set_edit_state (sample, SWEEP_EDIT_STATE_IDLE);
}

static void
paste_mix_dialog_ok_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;
  GtkWidget * slider;
  GtkWidget * checkbutton;
  sw_sample * sample = (sw_sample *)data;
  gdouble src_gain, dest_gain;

  dialog = gtk_widget_get_toplevel (widget);

  slider = gtk_object_get_data (GTK_OBJECT(dialog), "src_slider");
  src_gain = (double) db_slider_get_value (DB_SLIDER(slider));

  checkbutton = gtk_object_get_data (GTK_OBJECT(dialog), "src_invert");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton))) {
    src_gain *= -1.0;
  }

  slider = gtk_object_get_data (GTK_OBJECT(dialog), "dest_slider");
  dest_gain = (double) db_slider_get_value (DB_SLIDER(slider));

  checkbutton = gtk_object_get_data (GTK_OBJECT(dialog), "dest_invert");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton))) {
    dest_gain *= -1.0;
  }

  gtk_widget_hide (dialog);

  do_paste_mix (sample, src_gain, dest_gain);

  sample_set_edit_state (sample, SWEEP_EDIT_STATE_IDLE);
}

static void
paste_dialog_cancel_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;
  sw_sample * sample = (sw_sample *)data;

  dialog = gtk_widget_get_toplevel (widget);
  gtk_widget_hide (dialog);

  sample_set_edit_state (sample, SWEEP_EDIT_STATE_IDLE);
}

static void
create_paste_dialog (sw_sample * sample, gboolean xfade)
{
  GtkWidget * dialog;
  GtkWidget * main_vbox, * vbox;
  GtkWidget * hbox, * hbox2;
  GtkWidget * frame;
  GtkWidget * slider;
  GtkWidget * checkbutton;
  GtkWidget * ebox;
  GtkWidget * pixmap;
  GtkWidget * label;
  GtkWidget * ok_button, * button;

  GtkAccelGroup * accel_group;

  GtkTooltips * tooltips;

  gchar * title, * common_slider_title;

  sw_time_t duration;

#undef BUF_LEN
#define BUF_LEN 16
  char buf[BUF_LEN];

  if (xfade) {
    common_slider_title = _("Start gain");
  } else {
    common_slider_title = _("Gain");
  }

  dialog = gtk_dialog_new ();
  gtk_window_set_wmclass(GTK_WINDOW(dialog), "paste_dialog", "Sweep");

  if (xfade) {
    gtk_window_set_title(GTK_WINDOW(dialog), _("Sweep: Paste crossfade"));
  } else {
    gtk_window_set_title(GTK_WINDOW(dialog), _("Sweep: Paste mix"));
  }

  gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, FALSE);
  gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);

  accel_group = gtk_accel_group_new ();
  gtk_window_add_accel_group (GTK_WINDOW(dialog), accel_group);

  gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
		     (GtkSignalFunc) paste_dialog_destroy, sample);

#if 0
  gtk_accel_group_add (accel_group, GDK_w, GDK_CONTROL_MASK, GDK_NONE,
		       GTK_OBJECT(dialog), "destroy");
#endif

  main_vbox = GTK_DIALOG(dialog)->vbox;

  hbox = gtk_hbox_new (TRUE, 0);
  gtk_box_pack_start (GTK_BOX(main_vbox), hbox, TRUE, TRUE, 8);
  gtk_widget_show (hbox);

  /* Source */

  title = g_strdup_printf ("%s: %s", _("Source"), _("Clipboard"));
  frame = gtk_frame_new (title);
  gtk_container_set_border_width (GTK_CONTAINER(frame), 8);
  gtk_box_pack_start (GTK_BOX(hbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);
  g_free (title);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(frame), vbox);
  gtk_widget_show (vbox);

  hbox2 = gtk_hbox_new (TRUE, 8);
  gtk_box_pack_start (GTK_BOX(vbox), hbox2, TRUE, TRUE, 2);
  gtk_widget_show (hbox2);

  slider = db_slider_new (common_slider_title, (xfade ? 0.0 : 1.0), 0.0, 2.0);
  gtk_box_pack_start (GTK_BOX(hbox2), slider, TRUE, TRUE, 2);
  gtk_widget_show (slider);

  gtk_object_set_data (GTK_OBJECT(dialog), "src_slider", slider);

  if (xfade) {
    slider = db_slider_new (_("End gain"), 1.0, 0.0, 2.0);
    gtk_box_pack_start (GTK_BOX(hbox2), slider, TRUE, TRUE, 2);
    gtk_widget_show (slider);
    
    gtk_object_set_data (GTK_OBJECT(dialog), "src_slider2", slider);
  }

  checkbutton =
    gtk_check_button_new_with_label (_("Invert phase"));
  gtk_box_pack_start (GTK_BOX (vbox), checkbutton, TRUE, FALSE, 2);
  gtk_widget_show (checkbutton);

  gtk_object_set_data (GTK_OBJECT(dialog), "src_invert", checkbutton);

  /* Destination */

  title = g_strdup_printf ("%s: %s", _("Destination"),
			   g_basename (sample->pathname));
  frame = gtk_frame_new (title);
  gtk_container_set_border_width (GTK_CONTAINER(frame), 8);
  gtk_box_pack_start (GTK_BOX(hbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);
  g_free (title);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(frame), vbox);
  gtk_widget_show (vbox);

  hbox2 = gtk_hbox_new (TRUE, 8);
  gtk_box_pack_start (GTK_BOX(vbox), hbox2, TRUE, TRUE, 2);
  gtk_widget_show (hbox2);

  slider = db_slider_new (common_slider_title, 1.0, 0.0, 2.0);
  gtk_box_pack_start (GTK_BOX(hbox2), slider, TRUE, TRUE, 2);
  gtk_widget_show (slider);

  gtk_object_set_data (GTK_OBJECT(dialog), "dest_slider", slider);

  if (xfade) {
    slider = db_slider_new (_("End gain"), 0.0, 0.0, 2.0);
    gtk_box_pack_start (GTK_BOX(hbox2), slider, TRUE, TRUE, 2);
    gtk_widget_show (slider);

    gtk_object_set_data (GTK_OBJECT(dialog), "dest_slider2", slider);
  }

  checkbutton =
    gtk_check_button_new_with_label (_("Invert phase"));
  gtk_box_pack_start (GTK_BOX (vbox), checkbutton, TRUE, FALSE, 2);
  gtk_widget_show (checkbutton);

  gtk_object_set_data (GTK_OBJECT(dialog), "dest_invert", checkbutton);

  /* Info frame */

  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, TRUE, TRUE, 4);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_widget_show (frame);

  ebox = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER(frame), ebox);
  gtk_widget_show (ebox);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, ebox,
			_("Indicates the total duration of the clipboard, "
			  "which is the maximum length that will be pasted."),
			NULL);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(ebox), hbox);
  gtk_container_set_border_width (GTK_CONTAINER(hbox), 2);
  gtk_widget_show (hbox);

  if (xfade) {
    pixmap = create_widget_from_xpm (dialog, pastexfade_xpm);
  } else {
    pixmap = create_widget_from_xpm (dialog, pastemix_xpm);
  }
  gtk_box_pack_start (GTK_BOX(hbox), pixmap, FALSE, FALSE, 4);
  gtk_widget_show (pixmap);

  label = gtk_label_new (_("Clipboard duration:"));
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 4);
  gtk_widget_show (label);

  duration = frames_to_time (sample->sounddata->format, clipboard_width ());
  snprint_time (buf, BUF_LEN, duration);

  label = gtk_label_new (buf);
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 4);
  gtk_widget_show (label);

  /* OK */

  ok_button = gtk_button_new_with_label (xfade ? _("Crossfade") : _("Mix"));;
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (ok_button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area),
		      ok_button, TRUE, TRUE, 0);
  gtk_widget_show (ok_button);

  if (xfade) {
    gtk_signal_connect (GTK_OBJECT(ok_button), "clicked",
		      GTK_SIGNAL_FUNC (paste_xfade_dialog_ok_cb),
			sample);
  } else {
    gtk_signal_connect (GTK_OBJECT(ok_button), "clicked",
		      GTK_SIGNAL_FUNC (paste_mix_dialog_ok_cb),
			sample);
  }

  /* Cancel */

  button = gtk_button_new_with_label (xfade ?
				      _("Don't crossfade") : _("Don't mix"));
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area),
		      button, TRUE, TRUE , 0);
  gtk_widget_show (button);
  gtk_signal_connect (GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC (paste_dialog_cancel_cb),
		      sample);

  gtk_widget_grab_default (ok_button);

  sample_set_edit_state (sample, SWEEP_EDIT_STATE_BUSY);
  sample_set_edit_mode (sample, SWEEP_EDIT_MODE_FILTER);
  sample_set_progress_percent (sample, 0);
  
  if (!GTK_WIDGET_VISIBLE(dialog)) {
    gtk_widget_show (dialog);
  } else {
    gdk_window_raise (dialog->window);
  }
}

void
create_paste_mix_dialog (sw_sample * sample)
{
  create_paste_dialog (sample, FALSE);
}

void
create_paste_xfade_dialog (sw_sample * sample)
{
  create_paste_dialog (sample, TRUE);
}
