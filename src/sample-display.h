
/*
 * Sweep, a sound wave editor
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

#ifndef _SAMPLE_DISPLAY_H
#define _SAMPLE_DISPLAY_H

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#include "sweep_types.h"
#include "sweep_sample.h"
#include "view.h"

#define SAMPLE_DISPLAY(obj)          GTK_CHECK_CAST (obj, sample_display_get_type (), SampleDisplay)
#define SAMPLE_DISPLAY_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, sample_display_get_type (), SampleDisplayClass)
#define IS_SAMPLE_DISPLAY(obj)       GTK_CHECK_TYPE (obj, sample_display_get_type ())

typedef struct _SampleDisplay       SampleDisplay;
typedef struct _SampleDisplayClass  SampleDisplayClass;

enum {
  SAMPLE_DISPLAYCOL_BG,
  SAMPLE_DISPLAYCOL_FG,
  SAMPLE_DISPLAYCOL_MIXERPOS,
  SAMPLE_DISPLAYCOL_ZERO,
  SAMPLE_DISPLAYCOL_SEL,
  SAMPLE_DISPLAYCOL_TMP_SEL,
  SAMPLE_DISPLAYCOL_CROSSING,
  SAMPLE_DISPLAYCOL_MINMAX,
  SAMPLE_DISPLAYCOL_HIGHLIGHT,
  SAMPLE_DISPLAYCOL_LOWLIGHT,
  SAMPLE_DISPLAYCOL_LAST
};

struct _SampleDisplay
{
  GtkWidget widget;

  GdkGC *bg_gc, *fg_gc, *mixerpos_gc, *sel_gc, *tmp_sel_gc, *crossing_gc;
  GdkGC *minmax_gc, *zeroline_gc, *highlight_gc, *lowlight_gc;

  GdkPixmap * backing_pixmap;

  int width, height; /* Width and height of the widget */

  sw_view * view; /* The view (and hence, sample) we're displaying */

  int mixerpos, old_mixerpos; /* current playing offset of the sample */

#if 0 /* only used in optimised ST-style drawing routine (hashed out) */
  int complete_redraw; /* bool: whether or not to do a complete redraw
			  on the next call to _draw() */

  int old_ss, old_se;
#endif

  gint mouse_x;
  glong mouse_offset; /* what the pointer is currently pointing to */

  int selecting; /* Current state of this sample-display */
  int selection_mode; /* Mode of selection: replace or intersect */

  gint marching_tag; /* gtk_timeout tag for marching ants */
  gboolean marching_offset; /* dash offset of marching ants */

  /* Window panning */
  int selecting_x0;  /* the coordinate where the mouse was clicked */
  int selecting_wins0; /* stored value of view->v_start when the mouse
			* was clicked */
};

struct _SampleDisplayClass
{
  GtkWidgetClass parent_class;

  GdkColor colors[SAMPLE_DISPLAYCOL_LAST];

  /* Cursors */
  GdkCursor * crosshair_cr, * move_cr, * horiz_cr, * horiz_plus_cr;

  void (*selection_changed)(SampleDisplay *s, int start, int end);
  void (*window_changed)(SampleDisplay *s, int start, int end);
  void (*mouse_offset_changed)(SampleDisplay *s, int mouse_offset);
};

guint
sample_display_get_type (void);

GtkWidget*
sample_display_new (void);

void
sample_display_refresh (SampleDisplay *s);

void
sample_display_set_view (SampleDisplay *s, sw_view *view);

void
sample_display_set_playmarker (SampleDisplay *s, int offset);

void
sample_display_set_window (SampleDisplay *s, sw_framecount_t start,
			   sw_framecount_t end);

void
sample_display_clear_sel (SampleDisplay * s);

void
sample_display_sink_tmp_sel (SampleDisplay * s);

void
sample_display_start_marching_ants (SampleDisplay * s);

void
sample_display_stop_marching_ants (SampleDisplay * s);

#endif /* _SAMPLE_DISPLAY_H */
