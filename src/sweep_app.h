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

#ifndef __SWEEP_APP_H__
#define __SWEEP_APP_H__

#include <gtk/gtk.h>

/* #include "i18n.h" */

/* Tools */
enum {
  TOOL_NONE = 0,
  TOOL_SELECT,
  TOOL_MOVE,
  TOOL_ZOOM,
  TOOL_CROP
};


typedef struct _sw_view sw_view;

struct _sw_view {
  sw_sample * sample;

  sw_framecount_t start, end; /* bounds of visible frames */
  gfloat vol;

  GtkWidget * window;
  GtkWidget * time_ruler;
  GtkWidget * scrollbar;
  GtkWidget * display;
  GtkWidget * pos;
  GtkWidget * status;
  GtkWidget * menubar;
  GtkWidget * menu;
  GtkObject * adj;
  GtkObject * vol_adj;
};


#endif /* __SWEEP_APP_H__ */
