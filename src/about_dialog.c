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
#include <time.h>

#include <gtk/gtk.h>

#include <sweep/sweep_types.h>
#include <sweep/sweep_i18n.h>

#include "about_dialog.h"


static void about_dialog_destroy(void);
static int about_dialog_button(GtkWidget * widget,
			       GdkEventButton * event);

static GtkWidget *about_dialog = NULL;

static void
sweep_homepage (GtkWidget * widget, gpointer data)
{
  if (system ("gnome-moz-remote http://sweep.sourceforge.net/") < 0)
    perror ("system") ;
}

void
about_dialog_create()
{
 /* FIXME: for unused about box font style, reimplement?) 
  * GtkStyle *style; 
  */
  GtkWidget *vbox;
  GtkWidget *label;
#define BUF_LEN 64
  gchar buf[BUF_LEN];
  GtkWidget * button;
  GtkWidget * about_image;
  GtkWidget * about_ebox;
  gchar buf2[1024];

  if (!about_dialog) {
    about_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint (GTK_WINDOW(about_dialog), GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
    gtk_window_set_decorated ( GTK_WINDOW (about_dialog), FALSE);
    gtk_window_set_wmclass(GTK_WINDOW(about_dialog), "about_dialog", "Sweep");
    gtk_window_set_resizable (GTK_WINDOW(about_dialog), FALSE);
    gtk_window_set_position (GTK_WINDOW(about_dialog), GTK_WIN_POS_CENTER);
    g_signal_connect(GTK_OBJECT(about_dialog), "destroy",
		       G_CALLBACK(about_dialog_destroy), NULL);
    g_signal_connect(GTK_OBJECT(about_dialog), "button_press_event",
		       G_CALLBACK(about_dialog_button), NULL);
    gtk_widget_set_events(about_dialog, GDK_BUTTON_PRESS_MASK);



    vbox = gtk_vbox_new(FALSE, 1);
    gtk_container_add(GTK_CONTAINER(about_dialog), vbox);
    gtk_widget_show(vbox);

    about_ebox = gtk_event_box_new();
    gtk_box_pack_start(GTK_BOX(vbox), about_ebox, TRUE, TRUE, 0);
    gtk_widget_show(about_ebox);
    snprintf(buf2, sizeof (buf2), "%s/sweep_splash.png", PACKAGE_DATA_DIR);
    about_image = gtk_image_new_from_file (buf2);
    gtk_container_add(GTK_CONTAINER(about_ebox), about_image);
    gtk_widget_show(about_ebox);
    gtk_widget_show(about_image);

 /*  FIXME: old gtk font code. reimplement?
  *  style = gtk_style_new();
  *  gdk_font_unref(style->font);
  *  style->font = gdk_font_load("-Adobe-Helvetica-Medium-R-Normal--*-140-*-*-*-*-*-*");
  *  gtk_widget_push_style(style);
  */
    snprintf (buf, BUF_LEN, "%s %s", _("This is Sweep version"), VERSION);
    label = gtk_label_new(buf);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);
    gtk_widget_show(label);

#if 0
    label = gtk_label_new("Copyright (c) 2000 Conrad Parker, conrad@vergenet.net");
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);
    gtk_widget_show(label);
#endif

 /* FIXME: old style code
  *   gtk_widget_pop_style();
  */

#if 0
    alignment = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), alignment, FALSE, TRUE, 0);
    gtk_widget_show(alignment);

    label = gtk_label_new(_("http://sweep.sourceforge.net/"));
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);
    gtk_widget_show(label);
#endif

    button = gtk_button_new_with_label ("http://sweep.sourceforge.net/");
    gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, TRUE, 0);
    gtk_widget_show (button);
    g_signal_connect (GTK_OBJECT(button), "clicked",
			G_CALLBACK(sweep_homepage), NULL);
  }
  if (!GTK_WIDGET_VISIBLE(about_dialog)) {
    gtk_widget_show(about_dialog);
  } else {
    gdk_window_raise(about_dialog->window);
  }
}

static void
about_dialog_destroy()
{
  about_dialog = NULL;
}

static int
about_dialog_button(GtkWidget * widget,
		    GdkEventButton * event)
{
  gtk_widget_hide(about_dialog);

  return FALSE;
}
