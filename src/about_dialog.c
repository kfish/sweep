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
#include <time.h>

#include <gtk/gtk.h>

#include <sweep/sweep_types.h>
#include <sweep/sweep_i18n.h>

#include "about_dialog.h"

#define USE_LOGO

static void about_dialog_destroy(void);
static int about_dialog_button(GtkWidget * widget,
			       GdkEventButton * event);

static GtkWidget *about_dialog = NULL;

#ifdef USE_LOGO
static int about_dialog_load_logo(GtkWidget * window);
static int about_dialog_logo_expose(GtkWidget * widget,
				    GdkEventExpose * event);

static GtkWidget *logo_area = NULL;
static GdkPixmap *logo_pixmap = NULL;
static int logo_width = 0;
static int logo_height = 0;
#endif

void
about_dialog_create()
{
  GtkStyle *style;
  GtkWidget *vbox;
#ifdef USE_LOGO
  GtkWidget *aboutframe;
#endif
  GtkWidget *label;
#define BUF_LEN 64
  gchar buf[BUF_LEN];

  if (!about_dialog) {
    about_dialog = gtk_window_new(GTK_WINDOW_DIALOG);
    gtk_window_set_wmclass(GTK_WINDOW(about_dialog), "about_dialog", "Sweep");
    gtk_window_set_title(GTK_WINDOW(about_dialog), _("About Sweep"));
    gtk_window_set_policy(GTK_WINDOW(about_dialog), FALSE, FALSE, FALSE);
    gtk_window_position(GTK_WINDOW(about_dialog), GTK_WIN_POS_CENTER);
    gtk_signal_connect(GTK_OBJECT(about_dialog), "destroy",
		       (GtkSignalFunc) about_dialog_destroy, NULL);
    gtk_signal_connect(GTK_OBJECT(about_dialog), "button_press_event",
		       (GtkSignalFunc) about_dialog_button, NULL);
    gtk_widget_set_events(about_dialog, GDK_BUTTON_PRESS_MASK);

#ifdef USE_LOGO
    if (!about_dialog_load_logo(about_dialog)) {
      gtk_widget_destroy(about_dialog);
      about_dialog = NULL;
      return;
    }
#endif

    vbox = gtk_vbox_new(FALSE, 1);
    gtk_container_border_width(GTK_CONTAINER(vbox), 1);
    gtk_container_add(GTK_CONTAINER(about_dialog), vbox);
    gtk_widget_show(vbox);

#ifdef USE_LOGO
    aboutframe = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(aboutframe), GTK_SHADOW_IN);
    gtk_container_border_width(GTK_CONTAINER(aboutframe), 0);
    gtk_box_pack_start(GTK_BOX(vbox), aboutframe, TRUE, TRUE, 0);
    gtk_widget_show(aboutframe);

    logo_area = gtk_drawing_area_new();
    gtk_signal_connect(GTK_OBJECT(logo_area), "expose_event",
		       (GtkSignalFunc) about_dialog_logo_expose, NULL);
    gtk_drawing_area_size(GTK_DRAWING_AREA(logo_area), logo_width, logo_height);
    gtk_widget_set_events(logo_area, GDK_EXPOSURE_MASK);
    gtk_container_add(GTK_CONTAINER(aboutframe), logo_area);
    gtk_widget_show(logo_area);

    gtk_widget_realize(logo_area);
    gdk_window_set_background(logo_area->window, &logo_area->style->black);
#endif

    style = gtk_style_new();
    gdk_font_unref(style->font);
    style->font = gdk_font_load("-Adobe-Helvetica-Medium-R-Normal--*-140-*-*-*-*-*-*");
    gtk_widget_push_style(style);

    snprintf (buf, BUF_LEN, "%s %s", _("This is Sweep version"), VERSION);
    label = gtk_label_new(buf);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);
    gtk_widget_show(label);

#if 0
    label = gtk_label_new("Copyright (c) 2000 Conrad Parker, conrad@vergenet.net");
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);
    gtk_widget_show(label);

    gtk_widget_pop_style();

    alignment = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), alignment, FALSE, TRUE, 0);
    gtk_widget_show(alignment);

    label = gtk_label_new(_("http://sweep.sourceforge.net/"));
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);
    gtk_widget_show(label);
#endif

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

#ifdef USE_LOGO

static int
about_dialog_load_logo(GtkWidget * window)
{
  GtkWidget *preview;
  GdkGC *gc;
  char buf[1024];
  unsigned char *pixelrow;
  FILE *fp;
  int count;
  int i;

  if (logo_pixmap)
    return TRUE;

  sprintf(buf, "%s/sweep_logo.ppm", PACKAGE_DATA_DIR);

  fp = fopen(buf, "rb");
  if (!fp)
    return 0;

  fgets(buf, 1024, fp);
  if (strcmp(buf, "P6\n") != 0) {
    fclose(fp);
    return 0;
  }
  fgets(buf, 1024, fp);
  fgets(buf, 1024, fp);
  sscanf(buf, "%d %d", &logo_width, &logo_height);

  fgets(buf, 1024, fp);
  if (strcmp(buf, "255\n") != 0) {
    fclose(fp);
    return 0;
  }
  preview = gtk_preview_new(GTK_PREVIEW_COLOR);
  gtk_preview_size(GTK_PREVIEW(preview), logo_width, logo_height);
  pixelrow = g_new(guchar, logo_width * 3);

  for (i = 0; i < logo_height; i++) {
    count = fread(pixelrow, sizeof(unsigned char), logo_width * 3, fp);
    if (count != (logo_width * 3)) {
      gtk_widget_destroy(preview);
      g_free(pixelrow);
      fclose(fp);
      return 0;
    }
    gtk_preview_draw_row(GTK_PREVIEW(preview), pixelrow, 0, i, logo_width);
  }

  gtk_widget_realize(window);
  logo_pixmap = gdk_pixmap_new(window->window, logo_width, logo_height,
			       gtk_preview_get_visual()->depth);
  gc = gdk_gc_new(logo_pixmap);
  gtk_preview_put(GTK_PREVIEW(preview),
		  logo_pixmap, gc,
		  0, 0, 0, 0, logo_width, logo_height);
  gdk_gc_destroy(gc);

  gtk_widget_unref(preview);
  g_free(pixelrow);

  fclose(fp);

  return TRUE;
}


static int
about_dialog_logo_expose(GtkWidget * widget,
			 GdkEventExpose * event)
{
  /*
     If we draw beyond the boundaries of the pixmap, then X
     will generate an expose area for those areas, starting
     an infinite cycle. We now set allow_grow = FALSE, so
     the drawing area can never be bigger than the preview.
     Otherwise, it would be necessary to intersect event->area
     with the pixmap boundary rectangle. 
   */

  gdk_draw_pixmap(widget->window, widget->style->black_gc, logo_pixmap,
		  event->area.x, event->area.y,
		  event->area.x, event->area.y,
		  event->area.width, event->area.height);

  return FALSE;
}

#endif /* USE_LOGO */
