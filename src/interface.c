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

#include "sweep_types.h"
#include "sweep_app.h"

#include "callbacks.h"
#include "file_dialogs.h"
#include "interface.h"
#include "about_dialog.h"
#include "pixmaps.h"

static void
main_destroy_cb (GtkWidget * widget, gpointer data)
{
  gtk_main_quit();
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

GtkWidget*
create_toolbox (void)
{
  GtkWidget *window1;
  GtkWidget *vbox1;
  GtkWidget *handlebox2;
  GtkWidget *menubar1;
  GtkWidget *handlebox1;
  GtkWidget *toolbar1;
  GtkWidget *button1,/*  *button2,*/ *button3;
  GtkWidget *pixmap1, *pixmap3;
  GtkWidget *menu;
  GtkWidget *menuitem;
  GtkAccelGroup *accel_group;

  window1 = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window1), _("Sweep"));
  gtk_widget_realize (window1);

  gtk_signal_connect (GTK_OBJECT(window1), "destroy",
		      GTK_SIGNAL_FUNC(main_destroy_cb), NULL);


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
  gtk_signal_connect (GTK_OBJECT(menuitem), "activate",
		      GTK_SIGNAL_FUNC(sample_new_empty_cb), window1);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_n, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label (_("Open"));
  gtk_menu_append (GTK_MENU(menu), menuitem);
  gtk_signal_connect (GTK_OBJECT(menuitem), "activate",
		      GTK_SIGNAL_FUNC(sample_load_cb), window1);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_o, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label (_("Quit"));
  gtk_menu_append (GTK_MENU(menu), menuitem);
  gtk_signal_connect (GTK_OBJECT(menuitem), "activate",
		      GTK_SIGNAL_FUNC(exit_cb), window1);
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
  gtk_signal_connect (GTK_OBJECT(menuitem), "activate",
		      GTK_SIGNAL_FUNC(about_dialog_create), NULL);
  gtk_widget_show(menuitem);


  handlebox1 = gtk_handle_box_new ();
  gtk_widget_show (handlebox1);
  gtk_box_pack_start (GTK_BOX (vbox1), handlebox1, FALSE, FALSE, 0);

  toolbar1 = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
  gtk_widget_show (toolbar1);
  gtk_container_add (GTK_CONTAINER (handlebox1), toolbar1);

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
  button2 = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar1),
					GTK_TOOLBAR_CHILD_RADIOBUTTON,
					button1, /* Radio group */
					_("Move"),
					_("Move regions in a sample"),
					_("With this tool you can move"
					  " selected regions of a sample."),
					NULL, /* icon */
					set_tool_cb, (gpointer)TOOL_MOVE);
  gtk_widget_show (button2);
#endif

  pixmap3 = create_widget_from_xpm (window1, magnify_xpm);
  gtk_widget_show (pixmap3);

  button3 = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar1),
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
  gtk_widget_show (button3);

  return window1;
}

