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
#include <errno.h>
#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_types.h>
#include <sweep/sweep_sample.h>

#include "interface.h"

#include "../pixmaps/scrubby.xpm"
#include "../pixmaps/scrubby_system.xpm"

static gboolean up_and_running = FALSE;

static void
question_dialog_destroy_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;
  sw_sample * sample;
  gboolean quit_if_no_files = FALSE;

  dialog = gtk_widget_get_toplevel (widget);

  quit_if_no_files = (gboolean)
    GPOINTER_TO_INT(g_object_get_data (G_OBJECT(widget), "quit_nofiles"));

  sample = g_object_get_data (G_OBJECT(dialog), "default");
  if (sample && sample_bank_contains (sample)) {
    sample_set_edit_mode (sample, SWEEP_EDIT_MODE_READY);
  }

  if (quit_if_no_files || !up_and_running)
    sample_bank_remove (NULL);
}

static void
question_dialog_answer_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;
  GCallback(*func) (GtkWidget* widget, gpointer data);

  up_and_running = TRUE;

  func = g_object_get_data (G_OBJECT(widget), "default");
  dialog = gtk_widget_get_toplevel (widget);

  if (func != NULL)
    func (widget, data);

  /* "destroy" will call destroy above */
  
  gtk_widget_destroy (dialog);
}

static void
query_dialog_new (sw_sample * sample, char * title, char * question,
		  gboolean show_cancel, char * ok_answer, char * no_answer,
      char * ok_stock_id, char * no_stock_id, 
		  GCallback ok_callback, gpointer ok_callback_data,
		  GCallback no_callback, gpointer no_callback_data,
		  gpointer xpm_data, gboolean quit_if_no_files)
{
  gchar * new_title;

  GtkWidget * window;
  GtkWidget * button, * ok_button;
  GtkWidget * vbox;
  GtkWidget * hbox;
  GtkWidget * label;
  GtkWidget * pixmap;
  GtkWidget * button_label;
  GtkWidget * image;
  GtkWidget * button_hbox;

  window = gtk_dialog_new ();
  sweep_set_window_icon (GTK_WINDOW(window));

  new_title = g_strdup_printf ("%s: %s", "Sweep", title);
  gtk_window_set_title (GTK_WINDOW(window), new_title);
  g_free (new_title);

  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
  gtk_container_set_border_width  (GTK_CONTAINER(window), 8);

  g_object_set_data (G_OBJECT(window), "default", sample);

  g_signal_connect (G_OBJECT(window), "destroy",
		      G_CALLBACK(question_dialog_destroy_cb), window);

  attach_window_close_accel(GTK_WINDOW(window));

  g_object_set_data (G_OBJECT(window), "quit_nofiles",
		       GINT_TO_POINTER((gint)quit_if_no_files));

  vbox = GTK_DIALOG(window)->vbox;

  /* Question */

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(vbox), hbox, TRUE, FALSE, 0);  
  gtk_container_set_border_width (GTK_CONTAINER(hbox), 12);
  gtk_widget_show (hbox);

  if (xpm_data == NULL) xpm_data = scrubby_xpm;

  pixmap = create_widget_from_xpm (window, xpm_data);
  gtk_box_pack_start (GTK_BOX(hbox), pixmap, FALSE, FALSE, 12);
  gtk_widget_show (pixmap);

  label = gtk_label_new (question);
  gtk_box_pack_start (GTK_BOX(hbox), label, TRUE, FALSE, 12);
  gtk_widget_show (label);

  /* New layout of buttons */

  gtk_button_box_set_layout (GTK_BUTTON_BOX(GTK_DIALOG(window)->action_area), GTK_BUTTONBOX_SPREAD);

  /* OK */

  if (ok_answer == NULL) ok_answer = _("OK");
    
  if (ok_stock_id == NULL)
    ok_button = gtk_button_new_with_label (ok_answer);
  else
  {
    ok_button = gtk_button_new ();
    button_hbox = create_widget_label (NULL, ok_stock_id, GTK_ICON_SIZE_BUTTON, 
                                         ok_answer, FALSE);
    gtk_container_add (GTK_CONTAINER (ok_button), button_hbox);
  }
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (ok_button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(window)->action_area),
		      ok_button, FALSE, FALSE, 0);
  g_object_set_data (G_OBJECT(ok_button), "default", ok_callback);
  gtk_widget_show_all (ok_button);
  g_signal_connect (G_OBJECT(ok_button), "clicked",
		      G_CALLBACK (question_dialog_answer_cb),
		      ok_callback_data);

  /* Cancel */
  
  if (show_cancel) {
    if (no_answer == NULL) no_answer = _("Cancel");
      
    if (no_stock_id == NULL)
      button = gtk_button_new_with_label (no_answer);
    else
    {
      button = gtk_button_new ();
      button_hbox = create_widget_label (NULL, no_stock_id, GTK_ICON_SIZE_BUTTON, 
                                         no_answer, FALSE);
      gtk_container_add (GTK_CONTAINER (button), button_hbox);
    }

    GTK_WIDGET_SET_FLAGS (GTK_WIDGET (button), GTK_CAN_DEFAULT);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(window)->action_area),
			button, FALSE, FALSE, 0);
    g_object_set_data (G_OBJECT(button), "default", no_callback);
    gtk_widget_show_all (button);
    g_signal_connect (G_OBJECT(button), "clicked",
			G_CALLBACK (question_dialog_answer_cb),
			no_callback_data);
  }

  gtk_widget_grab_default (ok_button);

  gtk_widget_show (window);
}

void
question_dialog_new (sw_sample * sample, char * title, char * question,
		     char * yes_answer, char * no_answer,
        char * yes_stock_id, char * no_stock_id,
		     GCallback yes_callback, gpointer yes_callback_data,
		     GCallback no_callback, gpointer no_callback_data,
		     sw_edit_mode edit_mode)
{
  if (edit_mode != SWEEP_EDIT_MODE_READY)
    sample_set_edit_mode (sample, edit_mode);

  query_dialog_new (sample, title, question, TRUE, yes_answer, no_answer,
        yes_stock_id, no_stock_id,
		    yes_callback, yes_callback_data,
		    no_callback, no_callback_data, NULL, FALSE);
}

/* Thread safe info dialogs */

typedef struct {
  char * title;
  char * message;
  gpointer xpm_data;
} info_dialog_data;

static gint
do_info_dialog (gpointer data)
{
  info_dialog_data * id = (info_dialog_data *)data;

  query_dialog_new ( NULL, id->title, id->message, 
                    FALSE, _("OK"), NULL,
                    "gtk-ok", NULL,
                    NULL, NULL,
		                NULL, NULL, 
                    id->xpm_data, TRUE);

  g_free (id->title);
  g_free (id->message);

  return FALSE;
}

void
info_dialog_new (char * title, gpointer xpm_data, const char * fmt, ...)
{
  info_dialog_data * id;
  va_list ap;
#define BUF_LEN 512
  char buf[BUF_LEN];

  va_start (ap, fmt);
  vsnprintf (buf, BUF_LEN, fmt, ap);
  va_end (ap);

  id = g_malloc (sizeof (info_dialog_data));
  id->title = g_strdup (title);
  id->message = g_strdup (buf);
  id->xpm_data = xpm_data;

  sweep_timeout_add ((guint32)0, (GtkFunction)do_info_dialog, id);
}

/* Thread safe GUI perror reporting */

typedef struct {
  int thread_errno;
  char * message;
} sweep_perror_data;

static gint
syserror_dialog_new (gpointer data)
{
  sweep_perror_data * pd = (sweep_perror_data *)data;
  gchar * sys_errstr = NULL;
  char * new_message;

  sys_errstr = (gchar *) g_strerror (pd->thread_errno);

  if (sys_errstr != NULL) {
    new_message = g_strdup_printf ("%s:\n\n%s", pd->message, sys_errstr);
    
    query_dialog_new (NULL, sys_errstr, new_message, FALSE,
		      _("OK"), NULL, "gtk-ok", NULL, NULL, NULL,
		      NULL, NULL, scrubby_system_xpm, TRUE);
  }

  g_free (pd->message);
  g_free (pd);

  return FALSE;
}

void
sweep_perror (int thread_errno, const char * fmt, ...)
{
  sweep_perror_data * pd;
  va_list ap;
#undef BUF_LEN
#define BUF_LEN 512
  char buf[BUF_LEN];

  va_start (ap, fmt);
  vsnprintf (buf, BUF_LEN, fmt, ap);
  va_end (ap);

  pd = g_malloc (sizeof (sweep_perror_data));
  pd->thread_errno = thread_errno;
  pd->message = g_strdup (buf);

  sweep_timeout_add ((guint32)0, (GtkFunction)syserror_dialog_new, pd);
}
