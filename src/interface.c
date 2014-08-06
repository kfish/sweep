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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_types.h>
#include "sweep_app.h"

#include "driver.h"
#include "callbacks.h"
#include "file_dialogs.h"
#include "interface.h"
#include "about_dialog.h"
#include "pixmaps.h"
#include "../pixmaps/sweep_app_icon.xpm"  /* wm / taskbar icon */

GtkStyle * style_wb;
GtkStyle * style_bw;
GtkStyle * style_LCD;
GtkStyle * style_light_grey;
GtkStyle * style_green_grey;
GtkStyle * style_red_grey;
GtkStyle * style_dark_grey;
GtkStyle * style_red;

#if GTK_CHECK_VERSION (2, 10, 0)
GtkRecentManager *recent_manager = NULL;
#endif


static void
init_recent_manager(void)
{
#if GTK_CHECK_VERSION (2, 10, 0)

  recent_manager = gtk_recent_manager_get_default();

  /* good idea / bad idea? */
  if (!recent_manager)
    recent_manager = gtk_recent_manager_new();

#endif
}

void
recent_manager_add_item (gchar *path)
{
#if GTK_CHECK_VERSION (2, 10, 0)

  gchar *uri;

  //uri = g_strconcat("file:/", path, NULL);
  uri = g_filename_to_uri(path, NULL, NULL);

  if (recent_manager != NULL)
    gtk_recent_manager_add_item (recent_manager, uri);

  g_free(uri);
#endif
}

static void
init_accels (void)
{
  gchar * accels_path;

  accels_path = (char *)g_get_home_dir ();
  accels_path = g_strconcat (accels_path, "/.sweep/keybindings", NULL);
  gtk_accel_map_load (accels_path);

}

void
save_accels (void)
{
  gchar * accels_path;

  accels_path = (char *)g_get_home_dir ();
  accels_path = g_strconcat (accels_path, "/.sweep/keybindings", NULL);
  gtk_accel_map_save (accels_path);

}


void
sweep_set_window_icon (GtkWindow *window)
{
  GdkPixbuf * window_icon;

  if (!GTK_IS_WINDOW(window))
	return;

  window_icon = gdk_pixbuf_new_from_xpm_data((const char **)sweep_app_icon_xpm);

  if (window_icon)
   {
      gtk_window_set_icon (GTK_WINDOW (window), window_icon);
      g_object_unref (window_icon);
   }
}

GtkWidget *
create_widget_from_xpm (GtkWidget * widget, gchar **xpm_data)
{
  GdkBitmap * mask;
  GdkPixmap * pixmap_data;
  GdkWindow * window;
  GdkColormap * colormap;

  window = widget ? widget->window : NULL;
  colormap = widget ? gtk_widget_get_colormap (widget) :
    gtk_widget_get_default_colormap();

  pixmap_data = gdk_pixmap_colormap_create_from_xpm_d
    (window, colormap, &mask, NULL, xpm_data);

  return gtk_pixmap_new (pixmap_data, mask);
}

/*
 * create_color (r, g, b)
 *
 * Ripped from grip, Copyright (c) 1998-2002 Mike Oliphant
 */
GdkColor *
create_color (int red, int green, int blue)
{
  GdkColor *c;

  c=(GdkColor *)g_malloc(sizeof(GdkColor));
  c->red=red;
  c->green=green;
  c->blue=blue;

  gdk_color_alloc(gdk_colormap_get_system(),c);

  return c;
}

static gfloat style_color_mods[5]={0.0,-0.1,0.2,-0.2};

/*
 * create_style()
 *
 * Ripped from grip, Copyright (c) 1998-2002 Mike Oliphant
 */
GtkStyle *
create_style (GdkColor * fg, GdkColor * bg, gboolean do_grade)
{
  GtkStyle *def;
  GtkStyle *sty;
  int state;

  def=gtk_widget_get_default_style();
  sty=gtk_style_copy(def);

  for(state=0;state<5;state++) {
    if(fg) sty->fg[state]=*fg;

    if(bg) sty->bg[state]=*bg;

    if(bg && do_grade) {
      sty->bg[state].red+=sty->bg[state].red*style_color_mods[state];
      sty->bg[state].green+=sty->bg[state].green*style_color_mods[state];
      sty->bg[state].blue+=sty->bg[state].blue*style_color_mods[state];
    }
  }

  return sty;
}

void
init_styles (void)
{
  GdkColor gdkblack;
  GdkColor gdkwhite;
  GdkColor * color_LCD;
  GdkColor * color_light_grey;
  GdkColor * color_red_grey;
  GdkColor * color_red;

  gdk_color_white(gdk_colormap_get_system(),&gdkwhite);
  gdk_color_black(gdk_colormap_get_system(),&gdkblack);

  color_LCD = create_color (33686,38273,29557);
  color_light_grey = create_color (0xaaaa, 0xaaaa, 0xaaaa);
  color_red_grey = create_color (0xd3d3, 0x9898, 0x9898);
  color_red = create_color (0xffff, 0x0000, 0x0000);

  /* These two are created, but not used in this function. */
  create_color (0xaaaa, 0xbbbb, 0xaaaa);
  create_color (0x5555, 0x5555, 0x5555);

  style_wb = create_style(&gdkwhite,&gdkblack,FALSE);
  style_bw = create_style(&gdkblack,&gdkwhite,FALSE);
  style_LCD = create_style(color_LCD, color_LCD, FALSE);
  style_light_grey = create_style (color_light_grey, color_light_grey, TRUE);
  style_green_grey = style_light_grey;

  style_red_grey = create_style (color_red_grey, color_red_grey, TRUE);

  style_dark_grey = style_light_grey;

  style_red = create_style (&gdkblack, color_red, FALSE);
}

GtkWidget *
create_pixmap_button (GtkWidget * widget, gchar ** xpm_data,
		      const gchar * tip_text, GtkStyle * style,
		      sw_toolbar_button_type button_type,
		      GCallback clicked,
		      GCallback pressed, GCallback released,
		      gpointer data)
{
  GtkWidget * pixmap;
  GtkWidget * button;
  GtkTooltips * tooltips;

  switch (button_type) {
  case SW_TOOLBAR_TOGGLE_BUTTON:
    button = gtk_toggle_button_new ();
    break;
  case SW_TOOLBAR_RADIO_BUTTON:
    button = gtk_radio_button_new (NULL);
    break;
  case SW_TOOLBAR_BUTTON:
  default:
    button = gtk_button_new ();
    break;
  }

  if (xpm_data != NULL) {
    pixmap = create_widget_from_xpm (widget, xpm_data);
    gtk_widget_show (pixmap);
    gtk_container_add (GTK_CONTAINER (button), pixmap);
  }

  if (tip_text != NULL) {
    tooltips = gtk_tooltips_new ();
    gtk_tooltips_set_tip (tooltips, button, tip_text, NULL);
  }

  if (style != NULL) {
    gtk_widget_set_style (button, style);
  }

  if (clicked != NULL) {
    g_signal_connect (G_OBJECT (button), "clicked",
			G_CALLBACK(clicked), data);
  }

  if (pressed != NULL) {
    g_signal_connect (G_OBJECT(button), "pressed",
			G_CALLBACK(pressed), data);
  }

  if (released != NULL) {
    g_signal_connect (G_OBJECT(button), "released",
			G_CALLBACK(released), data);
  }

  return button;
}

/*
 * close the window when Ctrl and w are pressed.
 *	accel key combo is static. perhaps there is a better
 *	way to do this?
 */
void attach_window_close_accel(GtkWindow *window)
{
  GClosure *gclosure;
  GtkAccelGroup *accel_group;

  accel_group = gtk_accel_group_new ();
  gclosure = g_cclosure_new  ((GCallback)close_window_cb, NULL, NULL);
  gtk_accel_group_connect (accel_group,
                                             GDK_w,
                                             GDK_CONTROL_MASK,
                                             0, /* non of the GtkAccelFlags seem suitable? */
                                             gclosure);
  gtk_window_add_accel_group (GTK_WINDOW(window), accel_group);
}

static void
release_ui (void)
{
  g_object_unref (recent_manager);
}


void init_ui (void)
{
  init_accels();
  init_styles();
  init_recent_manager();

  atexit (release_ui);
}
