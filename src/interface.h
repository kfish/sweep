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

#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include "sweep_app.h"

GtkWidget *
create_widget_from_xpm (GtkWidget * widget, gchar **xpm_data);

GdkColor *
create_color (int red, int green, int blue);

GtkStyle *
create_style (GdkColor * fg, GdkColor * bg, gboolean do_grade);

void
init_styles (void);

void
sweep_set_window_icon (GtkWindow * window);



GtkWidget *
create_pixmap_button (GtkWidget * widget, gchar ** xpm_data,
		      const gchar * tip_text, GtkStyle * style,
		      sw_toolbar_button_type button_type,
		      GCallback clicked,
		      GCallback pressed, GCallback released,
		      gpointer data);


GtkWidget* create_toolbox (void);

void 
attach_window_close_accel(GtkWindow *window);

#endif /* __INTERFACE_H__ */
