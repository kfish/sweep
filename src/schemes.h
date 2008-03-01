/*
 * Sweep, a sound wave editor.
 *
 * Copyright (C) 2000 Conrad Parker
 * Copyright (C) 2008 Peter Shorthose
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

/*
 * Scheme editor, menus and general scheme handling routine are here.
 * the scheme object itself is in sweep-scheme.{c,h}
 */

#ifndef __SCHEMES_H__
#define __SCHEMES_H__

#include "sweep-scheme.h"

enum {
  SCHEME_SELECT_DEFAULT,
  SCHEME_SELECT_FILENAME,
  SCHEME_SELECT_RANDOM,
  SCHEME_SELECT_LAST
};

enum {
  COLOR_SWATCH_COLUMN,
  SCHEME_OBJECT_COLUMN,
  ELEMENT_NAME_COLUMN,
  ELEMENT_NUMBER_COLUMN,
  N_COLUMNS 
};

#define SCHEME_ELEMENT_TMP_SEL 1
#define SCHEME_ELEMENT_PLAY 0

GList *schemes_list;

void
init_schemes (void);

void
save_schemes (void);

void
schemes_load_user (void);

void
schemes_load_default (void);

gboolean
schemes_were_modified (void);

gint
schemes_get_selection_method (void);

SweepScheme *
schemes_get_prefered_scheme (gchar *filename);

SweepScheme *
schemes_get_scheme_system_default (void);

SweepScheme *
schemes_get_scheme_user_default (void);

void
schemes_remove_scheme (SweepScheme *scheme);

void 
schemes_copy_scheme (SweepScheme *scheme, gchar *newname);

gpointer
schemes_get_nth (gint n);

SweepScheme *
schemes_find_by_name (gchar *name);

void
schemes_show_editor_window_cb (GtkMenuItem *item, gpointer user_data);

void
schemes_create_menu (GtkWidget *parent_menuitem, gboolean connect_signals);

GtkWidget *
schemes_create_editor (gint index);

void
schemes_refresh_list_store(gint scheme_index);

GdkColor *
copy_gdk_colour (GdkColor *color_src);

GtkWidget*
schemes_create_color_picker (void);

void
schemes_picker_set_edited_color (SweepScheme *scheme, gint element);

void
schemes_set_active_element_color (GtkColorSelection *selection);


#endif /* __SCHEMES_H__ */
