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
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include <gtk/gtk.h>
#include <sweep/sweep_i18n.h>

#include <sweep/sweep_types.h>
#include <sweep/sweep_sample.h>

#include "driver.h"
#include "interface.h"

extern sw_driver * driver_alsa;
extern sw_driver * driver_oss;
extern sw_driver * driver_solaris;

extern GMutex * play_mutex;

/* preferred driver */
static sw_driver _driver_null = {
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static sw_driver * pref = &_driver_null;

#include "preferences.h"
#include "pcmio.h"

char *
pcmio_get_default_main_dev (void)
{
  GList * names = NULL, * gl;

  if (pref->get_names)
    names = pref->get_names ();

  if ((gl = names) != NULL) {
    return (char *)gl->data;
  }

  return NULL;
}

char *
pcmio_get_default_monitor_dev (void)
{
  GList * names = NULL, * gl;

  if (pref->get_names)
    names = pref->get_names ();

  if ((gl = names) != NULL) {
    if ((gl = gl->next) != NULL) {
      return (char *)gl->data;
    }
  }

  return NULL;
}

char *
pcmio_get_main_dev (void)
{
  char * main_dev;

  main_dev = prefs_get_string (pref->primary_device_key);

  if (main_dev == NULL) return pcmio_get_default_main_dev();
  
  return main_dev;
}

char *
pcmio_get_monitor_dev (void)
{
  char * monitor_dev;

  monitor_dev = prefs_get_string (pref->monitor_device_key);

  if (monitor_dev == NULL) return pcmio_get_default_monitor_dev ();
  
  return monitor_dev;
}

gboolean
pcmio_get_use_monitor (void)
{
  int * use_monitor;

  use_monitor = prefs_get_int (USE_MONITOR_KEY);

  if (use_monitor == NULL) return DEFAULT_USE_MONITOR;
  else return (*use_monitor != 0);
}

int
pcmio_get_log_frags (void)
{
  int * log_frags;

  log_frags = prefs_get_int (pref->log_frags_key);
  if (log_frags == NULL) return DEFAULT_LOG_FRAGS;
  else return (*log_frags);
}

extern GtkStyle * style_bw;
static GtkWidget * dialog = NULL;
static GtkWidget * main_combo;
static GtkWidget * monitor_combo;
static GtkObject * adj;


static gboolean
monitor_checked (GtkWidget * dialog)
{
  GtkWidget * monitor_chb;

  monitor_chb =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "monitor_chb"));

  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (monitor_chb));
}

static void
config_dev_dsp_dialog_ok_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog = GTK_WIDGET (data);
  GtkAdjustment * adj;
  G_CONST_RETURN gchar * main_dev, * monitor_dev;

  adj = g_object_get_data (G_OBJECT(dialog), "buff_adj");

  prefs_set_int (pref->log_frags_key, adj->value);

  main_dev =
    gtk_entry_get_text (GTK_ENTRY(GTK_COMBO(main_combo)->entry));

  prefs_set_string (pref->primary_device_key, (gchar *)main_dev);

  if (monitor_checked (dialog)) {
    monitor_dev =
      gtk_entry_get_text (GTK_ENTRY(GTK_COMBO(monitor_combo)->entry)); 
    prefs_set_string (pref->monitor_device_key, (gchar *)monitor_dev);

    prefs_set_int (USE_MONITOR_KEY, 1);
  } else {
    prefs_set_int (USE_MONITOR_KEY, 0);
  }

  gtk_widget_hide (dialog);
}

static void
config_dev_dsp_dialog_cancel_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;

  dialog = gtk_widget_get_toplevel (widget);
  gtk_widget_hide (dialog);
}

static void
update_ok_button (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog = GTK_WIDGET(data);
  GtkWidget * ok_button;
  gchar * main_devname, * monitor_devname;
  gboolean ok = FALSE;

  ok_button =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "ok_button"));

  if (monitor_checked (dialog)) {
    main_devname = (gchar *)
      gtk_entry_get_text (GTK_ENTRY(GTK_COMBO(main_combo)->entry));
    monitor_devname = (gchar *)
      gtk_entry_get_text (GTK_ENTRY(GTK_COMBO(monitor_combo)->entry));

    ok = (strcmp (main_devname, monitor_devname) != 0);
  } else {
    ok = TRUE;
  }

  gtk_widget_set_sensitive (ok_button, ok);
}

static void
set_monitor_widgets (GtkWidget * dialog, gboolean use_monitor)
{
  GtkWidget * monitor_chb, * monitor_widget, * swap;

  monitor_chb =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "monitor_chb"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(monitor_chb), use_monitor);

  monitor_widget = g_object_get_data (G_OBJECT(dialog), "monitor_widget");
  gtk_widget_set_sensitive (monitor_widget, use_monitor);

  swap = g_object_get_data (G_OBJECT(dialog), "swap");
  gtk_widget_set_sensitive (swap, use_monitor);

}

static void
set_buff_adj (GtkWidget * dialog, gint logfrags)
{
  GtkAdjustment * adj;

  adj = g_object_get_data (G_OBJECT(dialog), "buff_adj");
  gtk_adjustment_set_value (adj, logfrags);
}

static void
pcmio_devname_swap_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog = GTK_WIDGET (data);
  gchar * main_dev, * monitor_dev;

  main_dev =
    g_strdup (gtk_entry_get_text (GTK_ENTRY(GTK_COMBO(main_combo)->entry)));
  monitor_dev = (gchar *)gtk_entry_get_text (GTK_ENTRY(GTK_COMBO(monitor_combo)->entry));

  gtk_entry_set_text (GTK_ENTRY(GTK_COMBO(main_combo)->entry), monitor_dev);
  gtk_entry_set_text (GTK_ENTRY(GTK_COMBO(monitor_combo)->entry), main_dev);

  g_free (main_dev);

  set_monitor_widgets (dialog, pcmio_get_use_monitor());

  update_ok_button (widget, data);
}

static void
pcmio_devname_reset_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog = GTK_WIDGET (data);
  char * main_dev, * monitor_dev;

  main_dev = pcmio_get_main_dev ();
  monitor_dev = pcmio_get_monitor_dev ();

  gtk_entry_set_text (GTK_ENTRY(GTK_COMBO(main_combo)->entry), main_dev);
  gtk_entry_set_text (GTK_ENTRY(GTK_COMBO(monitor_combo)->entry), monitor_dev);

  set_monitor_widgets (dialog, pcmio_get_use_monitor());

  update_ok_button (widget, data);
}

static void
pcmio_devname_default_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog = GTK_WIDGET (data);
  char * name;

  if ((name = pcmio_get_default_main_dev ()) != NULL) {
    gtk_entry_set_text (GTK_ENTRY(GTK_COMBO(main_combo)->entry), name);
  }

  if ((name = pcmio_get_default_monitor_dev ()) != NULL) {
    gtk_entry_set_text (GTK_ENTRY(GTK_COMBO(monitor_combo)->entry), name);
  }

  set_monitor_widgets (dialog, DEFAULT_USE_MONITOR);

  update_ok_button (widget, data);
}

static void
monitor_enable_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog = GTK_WIDGET (data);
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));

  set_monitor_widgets (dialog, active);
}

static void
pcmio_buffering_reset_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog = GTK_WIDGET (data);

  set_buff_adj (dialog, pcmio_get_log_frags());
}

static void
pcmio_buffering_default_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog = GTK_WIDGET (data);

  set_buff_adj (dialog, DEFAULT_LOG_FRAGS);
}

static GtkWidget *
create_devices_combo (void)
{
  GtkWidget * combo;
  GList * cbitems = NULL;

  if (pref->get_names)
    cbitems = pref->get_names();
  
  combo = gtk_combo_new ();
  
  gtk_combo_set_popdown_strings (GTK_COMBO(combo), cbitems);

  return combo;
}

void
device_config (void)
{
  GtkWidget * ebox;
  GtkWidget * notebook;
  GtkWidget * separator;
  GtkWidget * hbox, * hbox2, *hbox3;
  GtkWidget * vbox;
  GtkWidget * label;
  GtkWidget * checkbutton;
  GtkWidget * hscale;
  GtkWidget * ok_button;
  GtkWidget * button;

  GtkTooltips * tooltips;

  if (dialog == NULL) {

    dialog = gtk_dialog_new ();

    g_signal_connect ((gpointer) dialog, "destroy",
                     G_CALLBACK(gtk_widget_destroyed),
                     &dialog);

    gtk_window_set_title (GTK_WINDOW(dialog), _("Sweep: audio device configuration"));
    sweep_set_window_icon (GTK_WINDOW (dialog));
    gtk_window_set_position (GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);

    /* OK */

    ok_button = gtk_button_new_from_stock ("gtk-ok");
    GTK_WIDGET_SET_FLAGS (GTK_WIDGET (ok_button), GTK_CAN_DEFAULT);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), ok_button,
			TRUE, TRUE, 0);
    gtk_widget_show (ok_button);
    g_signal_connect (G_OBJECT(ok_button), "clicked",
			G_CALLBACK (config_dev_dsp_dialog_ok_cb),
			dialog);

    g_object_set_data (G_OBJECT (dialog), "ok_button", ok_button);

    /* Cancel */

    button = gtk_button_new_from_stock ("gtk-cancel");
    GTK_WIDGET_SET_FLAGS (GTK_WIDGET (button), GTK_CAN_DEFAULT);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), button,
			TRUE, TRUE, 0);
    gtk_widget_show (button);
    g_signal_connect (G_OBJECT(button), "clicked",
			G_CALLBACK (config_dev_dsp_dialog_cancel_cb),
			NULL);

    gtk_widget_grab_default (ok_button);


    ebox = gtk_event_box_new ();
    gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), ebox,
			TRUE, TRUE, 0);
    gtk_widget_set_style (ebox, style_bw);
    gtk_widget_show (ebox);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER(ebox), vbox);
    gtk_widget_show (vbox);

    /* Changes ... info */
    label = gtk_label_new (_("Changes to device settings will take effect on"
			     " next playback."));
    gtk_box_pack_start (GTK_BOX(vbox), label, TRUE, TRUE, 8);
    gtk_widget_show (label);


    /* Notebook */

    notebook = gtk_notebook_new ();
    gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), notebook,
			TRUE, TRUE, 4);
    gtk_widget_show (notebook);

    /* Device name */
    label = gtk_label_new (_("Device name"));
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);
    gtk_container_set_border_width (GTK_CONTAINER(vbox), 4);
    gtk_widget_show (vbox);

    label = gtk_label_new (_("Set the main device for playback and recording"));
    gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 4);
    gtk_widget_show (label);

    /* Main output */ 
    hbox = gtk_hbox_new (FALSE, 8);
    gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, TRUE, 8);
    gtk_container_set_border_width (GTK_CONTAINER(hbox), 12);
    gtk_widget_show (hbox);
      
    label = gtk_label_new (_("Main device:"));
    gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);
      
    main_combo = create_devices_combo ();
    gtk_box_pack_start (GTK_BOX(hbox), main_combo, TRUE, TRUE, 0);
    gtk_widget_show (main_combo);

    g_signal_connect (G_OBJECT(GTK_COMBO(main_combo)->entry), "changed",
			G_CALLBACK(update_ok_button), dialog);

    g_object_set_data (G_OBJECT (dialog), "main_combo", main_combo);

#if 0
    button = gtk_button_new_with_label (_("Default"));
    gtk_box_pack_start (GTK_BOX(hbox), button, FALSE, FALSE, 0);
    gtk_widget_show (button);
    g_signal_connect (G_OBJECT(button), "clicked",
			G_CALLBACK(default_devicename_cb), NULL);
#endif

    separator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, FALSE, 8);
    gtk_widget_show (separator);

    /* Monitor */
    checkbutton =
      gtk_check_button_new_with_label (_("Use a different device for monitoring"));
    gtk_box_pack_start (GTK_BOX(vbox), checkbutton, FALSE, FALSE, 4);
    gtk_widget_show (checkbutton);

    g_signal_connect (G_OBJECT(checkbutton), "toggled",
			G_CALLBACK(update_ok_button), dialog);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, TRUE, 8);
    gtk_container_set_border_width (GTK_CONTAINER(hbox), 12);
    gtk_widget_show (hbox);
      
    label = gtk_label_new (_("Monitor output:"));
    gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    monitor_combo = create_devices_combo ();
    gtk_box_pack_start (GTK_BOX(hbox), monitor_combo, TRUE, TRUE, 0);
    gtk_widget_show (monitor_combo);

    g_signal_connect (G_OBJECT(GTK_COMBO(monitor_combo)->entry), "changed",
			G_CALLBACK(update_ok_button), dialog);

    g_signal_connect (G_OBJECT(checkbutton), "toggled",
			G_CALLBACK(monitor_enable_cb), dialog);

    g_object_set_data (G_OBJECT (dialog), "monitor_chb", checkbutton);
    g_object_set_data (G_OBJECT (dialog), "monitor_widget", hbox);


    /* Swap / Remember / Reset device names*/

    hbox = gtk_hbox_new (FALSE, 4);
    gtk_box_pack_end (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER(hbox), 12);
    gtk_widget_show (hbox);

    button = gtk_button_new_with_label (_("Swap"));
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 4);
    g_signal_connect (G_OBJECT(button), "clicked",
			G_CALLBACK(pcmio_devname_swap_cb), dialog);
    gtk_widget_show (button);

    tooltips = gtk_tooltips_new ();
    gtk_tooltips_set_tip (tooltips, button,
			  _("Swap main and monitor devices."),
			  NULL);

    g_object_set_data (G_OBJECT (dialog), "swap", button);

    hbox2 = gtk_hbox_new (TRUE, 4);
    gtk_box_pack_end (GTK_BOX (hbox), hbox2, FALSE, TRUE, 0);
    gtk_widget_show (hbox2);

    button = gtk_button_new ();
    hbox3   = create_widget_label (NULL, "gtk-undo", 
                                  GTK_ICON_SIZE_MENU, _("Reset"), FALSE);
    gtk_container_add (GTK_CONTAINER (button), hbox3);
    gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, TRUE, 4);
    g_signal_connect (G_OBJECT(button), "clicked",
			G_CALLBACK(pcmio_devname_reset_cb), dialog);
    gtk_widget_show (button);

    tooltips = gtk_tooltips_new ();
    gtk_tooltips_set_tip (tooltips, button,
			  _("Reset to the last remembered device names."),
			  NULL);

    /* Call the reset callback now to set remembered options */
    pcmio_devname_reset_cb (button, dialog);

    button = gtk_button_new_with_label (_("Defaults"));
    gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, TRUE, 4);
    g_signal_connect (G_OBJECT(button), "clicked",
			G_CALLBACK(pcmio_devname_default_cb), dialog);
    gtk_widget_show (button);

    tooltips = gtk_tooltips_new ();
    gtk_tooltips_set_tip (tooltips, button,
			  _("Set to default device names."),
			  NULL);


    separator = gtk_hseparator_new ();
    gtk_box_pack_end (GTK_BOX (vbox), separator, FALSE, FALSE, 8);
    gtk_widget_show (separator);


    /* Buffering */

    label = gtk_label_new (_("Device buffering"));
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);
    gtk_container_set_border_width (GTK_CONTAINER(vbox), 4);
    gtk_widget_show (vbox);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_box_pack_start (GTK_BOX(vbox), hbox, TRUE, TRUE, 8);
    gtk_widget_show (hbox);

    label = gtk_label_new (_("Low latency /\nMore dropouts"));
    gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 8);
    gtk_widget_show (label);

    adj = gtk_adjustment_new (pcmio_get_log_frags (), /* value */
			      LOG_FRAGS_MIN, /* lower */
			      LOG_FRAGS_MAX+1, /* upper */
			      1, /* step incr */
			      1, /* page incr */
			      1  /* page size */
			      );

    g_object_set_data (G_OBJECT(dialog), "buff_adj", adj);

    hscale = gtk_hscale_new (GTK_ADJUSTMENT(adj));
    gtk_box_pack_start (GTK_BOX(hbox), hscale, TRUE, TRUE, 4);
    gtk_scale_set_draw_value (GTK_SCALE(hscale), TRUE);
    gtk_scale_set_digits (GTK_SCALE(hscale), 0);
    gtk_range_set_update_policy (GTK_RANGE(hscale), GTK_UPDATE_CONTINUOUS);
    gtk_widget_set_size_request(hscale, 160, -1);
    gtk_widget_show (hscale);

    label = gtk_label_new (_("High latency /\nFewer dropouts"));
    gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 8);
    gtk_widget_show (label);

    label = gtk_label_new (_("Varying this slider controls the lag between "
			     "cursor movements and playback. This is "
			     "particularly noticeable when \"scrubbing\" "
			     "during playback.\n\nLower values improve "
			     "responsiveness but may degrade audio quality "
			     "on heavily-loaded systems."));
    gtk_label_set_line_wrap (GTK_LABEL(label), TRUE);
    gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 8);
    gtk_widget_show (label);

    /* Remember / Reset device buffering */

    hbox = gtk_hbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER(hbox), 12);
    gtk_widget_show (hbox);

#if 0
    checkbutton =
      gtk_check_button_new_with_label (_("Remember these options"));
    gtk_box_pack_start (GTK_BOX (hbox), checkbutton, TRUE, TRUE, 0);
    gtk_widget_show (checkbutton);

    g_object_set_data (G_OBJECT (dialog), "rem_options_chb", checkbutton);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkbutton), TRUE);
#endif

    hbox2 = gtk_hbox_new (TRUE, 4);
    gtk_box_pack_end (GTK_BOX (hbox), hbox2, FALSE, TRUE, 0);
    gtk_widget_show (hbox2);

    button = gtk_button_new ();
    hbox3   = create_widget_label (NULL, "gtk-undo", 
                                  GTK_ICON_SIZE_MENU, _("Reset"), FALSE);
    gtk_container_add (GTK_CONTAINER (button), hbox3);
    gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, TRUE, 4);
    g_signal_connect (G_OBJECT(button), "clicked",
			G_CALLBACK(pcmio_buffering_reset_cb), dialog);
    gtk_widget_show (button);

    tooltips = gtk_tooltips_new ();
    gtk_tooltips_set_tip (tooltips, button,
			  _("Reset to the last remembered device buffering."),
			  NULL);

    /* Call the reset callback now to set remembered options */
    pcmio_buffering_reset_cb (button, dialog);

    button = gtk_button_new_with_label (_("Default"));
    gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, TRUE, 4);
    g_signal_connect (G_OBJECT(button), "clicked",
			G_CALLBACK(pcmio_buffering_default_cb), dialog);
    gtk_widget_show (button);

    tooltips = gtk_tooltips_new ();
    gtk_tooltips_set_tip (tooltips, button,
			  _("Set to default device buffering."),
			  NULL);
  }

  gtk_entry_set_text (GTK_ENTRY(GTK_COMBO(main_combo)->entry),
		      pcmio_get_main_dev ());

  gtk_entry_set_text (GTK_ENTRY(GTK_COMBO(monitor_combo)->entry),
		      pcmio_get_monitor_dev ());

  gtk_adjustment_set_value (GTK_ADJUSTMENT(adj), pcmio_get_log_frags ());

  if (!GTK_WIDGET_VISIBLE(dialog)) {
    gtk_widget_show(dialog);
  } else {
    gdk_window_raise(dialog->window);
  }
}

sw_handle *
device_open (int cueing, int flags)
{
  if (pref->open)
    return pref->open (cueing, flags);
  else
    return NULL;
}

void
device_setup (sw_handle * handle, sw_format * format)
{
  if (pref->setup)
    pref->setup (handle, format);
}

int
device_wait (sw_handle * handle)
{
  if (pref->wait)
    return pref->wait (handle);
  else
    return 0;
}

ssize_t
device_read (sw_handle * handle, sw_audio_t * buf, size_t count)
{
  if (pref->read)
    return pref->read (handle, buf, count);
  else
    return -1;
}

ssize_t
device_write (sw_handle * handle, sw_audio_t * buf, size_t count,
	      sw_framecount_t offset)
{
#ifdef DEBUG
  printf ("device_write: %d from %d\n", count, offset);
#endif

  if (pref->write)
    return pref->write (handle, buf, count, offset);
  else
    return -1;
}

sw_framecount_t
device_offset (sw_handle * handle)
{
  if (pref->offset)
    return pref->offset (handle);
  else
    return -1;
}

void
device_reset (sw_handle * handle)
{
  if (pref->reset)
    pref->reset (handle);
}

void
device_flush (sw_handle * handle)
{
  if (pref->flush)
    pref->flush (handle);
}

void
device_drain (sw_handle * handle)
{
  if (pref->drain)
    pref->drain (handle);
}

void
device_close (sw_handle * handle)
{
  if (pref->close)
    pref->close (handle);

  handle->driver_fd = -1;
}

void
init_devices (void)
{
#if defined(DRIVER_ALSA)
  pref = driver_alsa;
#elif defined(DRIVER_OSS)
  pref = driver_oss;
#elif defined(DRIVER_SOLARIS_AUDIO)
  pref = driver_solaris;
#endif

  play_mutex = g_mutex_new ();
}
