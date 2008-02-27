/*
 * Sweep, a sound wave editor
 *
 * Copyright (C) 2000 Conrad Parker
 * Copyright (C) 2006 Radoslaw Korzeniewski
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

#ifndef _SCROLL_PANE_H
#define _SCROLL_PANE_H

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#include <sweep/sweep_types.h>
#include <sweep/sweep_sample.h>
#include "view.h"

#define SCROLL_PANE(obj)          GTK_CHECK_CAST (obj, scroll_pane_get_type (), ScrollPane)
#define SCROLL_PANE_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, scroll_pane_get_type (), ScrollPaneClass)
#define IS_SCROLL_PANE(obj)       GTK_CHECK_TYPE (obj, scroll_pane_get_type ())

typedef struct _ScrollPane       ScrollPane;
typedef struct _ScrollPaneClass  ScrollPaneClass;

enum {
  SCROLL_PANECOL_FG,
  SCROLL_PANECOL_SLIDER,
  SCROLL_PANECOL_PLAYMAKER,
  SCROLL_PANECOL_LAST
};

struct _ScrollPane
{
  GtkHScrollbar widget;

  SweepScheme *scheme;
  GdkGC * fg_gcs[SCROLL_PANECOL_LAST];

  /* The view (and hence, sample) we're displaying */
  sw_view * view;

  /* play_offset */
  gint play_offset;
};

struct _ScrollPaneClass
{
  GtkHScrollbarClass parent_class;
  GdkColor colors[SCROLL_PANECOL_LAST];
};

GType
scroll_pane_get_type (void);

GtkWidget* scroll_pane_new (GtkAdjustment *adjustment);

void
scroll_pane_refresh (ScrollPane *s);

void
scroll_pane_set_view (ScrollPane *s, sw_view *view);


#endif /* _SCROLL_PANE_H */
