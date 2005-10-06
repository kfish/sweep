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





void sweep_set_window_icon (GtkWindow *window)
{
  GdkPixbuf * window_icon;
  
  if (!GTK_IS_WINDOW(window))
	return;
  
  window_icon = gdk_pixbuf_new_from_xpm_data((const char **)sweep_app_icon_xpm);
  
  if (window_icon)
   {
      gtk_window_set_icon (GTK_WINDOW (window), window_icon);
      gdk_pixbuf_unref (window_icon);
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
  GdkColor * color_green_grey;
  GdkColor * color_red_grey;
  GdkColor * color_dark_grey;
  GdkColor * color_red;

  gdk_color_white(gdk_colormap_get_system(),&gdkwhite);
  gdk_color_black(gdk_colormap_get_system(),&gdkblack);
  
  color_LCD = create_color (33686,38273,29557);
  color_light_grey = create_color (0xaaaa, 0xaaaa, 0xaaaa);
  color_green_grey = create_color (0xaaaa, 0xbbbb, 0xaaaa);
  color_red_grey = create_color (0xd3d3, 0x9898, 0x9898);
  /*  color_dark_grey = create_color (0x4444,0x4444,0x4444);*/
  color_dark_grey = create_color (0x5555, 0x5555, 0x5555);
  color_red = create_color (0xffff, 0x0000, 0x0000);

  style_wb = create_style(&gdkwhite,&gdkblack,FALSE);
  style_bw = create_style(&gdkblack,&gdkwhite,FALSE);
  style_LCD = create_style(color_LCD, color_LCD, FALSE);
  style_light_grey = create_style (color_light_grey, color_light_grey, TRUE);
#if 0
  style_green_grey = create_style (color_green_grey, color_green_grey, TRUE);
#else
  style_green_grey = style_light_grey;
#endif

  style_red_grey = create_style (color_red_grey, color_red_grey, TRUE);

#if 0
  style_dark_grey = create_style(&gdkwhite, color_dark_grey, TRUE);
#else
  style_dark_grey = style_light_grey;
#endif

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


#if 0
static void
main_destroy_cb (GtkWidget * widget, gpointer data)
{
  gtk_main_quit();
}


GtkWidget*
create_toolbox (void)
{
  GtkWidget *window1;
  GtkWidget *vbox1;
  GtkWidget *handlebox2;
  GtkWidget *menubar1;
  GtkWidget *handlebox1;
  GtkWidget *toolbar1;
  GtkWidget *button1, * button;
  GtkWidget *pixmap1, *pixmap3;
  GtkWidget *menu;
  GtkWidget *menuitem;
  GtkAccelGroup *accel_group;

  window1 = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window1), _("Sweep"));
  gtk_widget_realize (window1);

  g_signal_connect (G_OBJECT(window1), "destroy",
		      G_CALLBACK(main_destroy_cb), NULL);


  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox1);
  gtk_container_add (GTK_CONTAINER (window1), vbox1);

  handlebox2 = gtk_handle_box_new ();
  gtk_widget_show (handlebox2);
  gtk_box_pack_start (GTK_BOX (vbox1), handlebox2, FALSE, FALSE, 0);

  menubar1 = gtk_menu_bar_new ();
  gtk_widget_show (menubar1);
  gtk_container_add (GTK_CONTAINER (handlebox2), menubar1);

  /* Create a GtkAccelGroup and add it to the window. */
  accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group (GTK_WINDOW(window1), accel_group);

  menu = gtk_menu_new();

  menuitem = gtk_menu_item_new_with_label (_("File"));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM(menuitem), menu);
  gtk_menu_bar_append (GTK_MENU_BAR(menubar1), menuitem);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label (_("New"));
  gtk_menu_append (GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		      G_CALLBACK(sample_new_empty_cb), NULL);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_n, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label (_("Open"));
  gtk_menu_append (GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		      G_CALLBACK(sample_load_cb), window1);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_o, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label (_("Quit"));
  gtk_menu_append (GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		      G_CALLBACK(exit_cb), window1);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_q, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  menu = gtk_menu_new();

  menuitem = gtk_menu_item_new_with_label (_("Help"));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM(menuitem), menu);
  gtk_menu_bar_append (GTK_MENU_BAR(menubar1), menuitem);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label (_("About..."));
  gtk_menu_append (GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		      G_CALLBACK(about_dialog_create), NULL);
  gtk_widget_show(menuitem);

#if 0
  handlebox1 = gtk_handle_box_new ();
  gtk_widget_show (handlebox1);
  gtk_box_pack_start (GTK_BOX (vbox1), handlebox1, FALSE, FALSE, 0);

  toolbar1 = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
  gtk_widget_show (toolbar1);
  gtk_container_add (GTK_CONTAINER (handlebox1), toolbar1);

  /* SELECT */

  pixmap1 = create_widget_from_xpm (toolbar1, rect_xpm);
  gtk_widget_show (pixmap1);

  button1 = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar1),
					GTK_TOOLBAR_CHILD_RADIOBUTTON,
					NULL, /* Radio group */
					_("Select"),
					_("Select regions of a sample"), 
					_("This tool allows you to select"
					  " regions of a sample. You can then"
					  " apply edits and effects to the:"
					  " selected regions. Hold down shift"
					  " whilst selecting to add"
					  " discontinuous regions to the"
					  " selection."),
					pixmap1, /* icon */
					set_tool_cb, (gpointer)TOOL_SELECT);
  gtk_widget_show (button1);

#if 0
  /* MOVE */

  button = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar1),
				       GTK_TOOLBAR_CHILD_RADIOBUTTON,
				       button1, /* Radio group */
					_("Move"),
				       _("Move regions in a sample"),
				       _("With this tool you can move"
					 " selected regions of a sample."),
				       NULL, /* icon */
				       set_tool_cb, (gpointer)TOOL_MOVE);
  gtk_widget_show (button);
#endif

  /* SCRUB */

  button = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar1),
				       GTK_TOOLBAR_CHILD_RADIOBUTTON,
				       button1, /* Radio group */
				       _("Scrub"),
				       _("Locate sounds directly"),
				       _("Place the play marker on a sample."
					 " Click anywhere in a view to"
					 " instantly move the playback"
					 " position to that part of the"
					 " sample."),
				       NULL, /* icon */
				       set_tool_cb, (gpointer)TOOL_SCRUB);
  gtk_widget_show (button);

  /* ZOOM */

  pixmap3 = create_widget_from_xpm (window1, magnify_xpm);
  gtk_widget_show (pixmap3);

  button = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar1),
				       GTK_TOOLBAR_CHILD_RADIOBUTTON,
				       button1, /* Radio group */
				       _("Zoom"),
				       _("Zoom in & out"),
				       _("Zoom in and out of a view. Click"
					 " anywhere in a view to zoom in on"
					 " that part of the sample. Hold"
					 " down shift and click on the view"
					 " to zoom out."),
				       pixmap3, /* icon */
				       set_tool_cb, (gpointer)TOOL_ZOOM);
  gtk_widget_show (button);

#if 1 /* These need undos! */

  /* PENCIL */

  button = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar1),
				       GTK_TOOLBAR_CHILD_RADIOBUTTON,
				       button1, /* Radio group */
				       _("Pencil"),
				       _("Edit PCM sample values"),
				       _("When zoomed down to individual "
					 " samples, click to edit"),
				       NULL, /* icon */
				       set_tool_cb, (gpointer)TOOL_PENCIL);
  gtk_widget_show (button);

  /* NOISE */

  button = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar1),
				       GTK_TOOLBAR_CHILD_RADIOBUTTON,
				       button1, /* Radio group */
				       _("Noise"),
				       _("Add noise"),
				       _("Randomise PCM values"),
				       NULL, /* icon */
				       set_tool_cb, (gpointer)TOOL_NOISE);
  gtk_widget_show (button);
#endif

#endif /* TOOLBAR */

  return window1;
}

#endif

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
