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

#include <sweep/sweep_types.h>
#include <sweep/sweep_sample.h>
#include "sweep-scheme.h"
#include "view.h"

#define SAMPLE_DISPLAY(obj)          GTK_CHECK_CAST (obj, sample_display_get_type (), SampleDisplay)
#define SAMPLE_DISPLAY_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, sample_display_get_type (), SampleDisplayClass)
#define IS_SAMPLE_DISPLAY(obj)       GTK_CHECK_TYPE (obj, sample_display_get_type ())

typedef struct _SampleDisplay       SampleDisplay;
typedef struct _SampleDisplayClass  SampleDisplayClass;

enum {
  SAMPLE_DISPLAYCOL_BG,
  SAMPLE_DISPLAYCOL_FG,
  SAMPLE_DISPLAYCOL_PLAY,
  SAMPLE_DISPLAYCOL_PAUSE,
  SAMPLE_DISPLAYCOL_ZERO,
  SAMPLE_DISPLAYCOL_SEL,
  SAMPLE_DISPLAYCOL_TMP_SEL,
  SAMPLE_DISPLAYCOL_CROSSING,
  SAMPLE_DISPLAYCOL_MINMAX,
  SAMPLE_DISPLAYCOL_HIGHLIGHT,
  SAMPLE_DISPLAYCOL_LOWLIGHT,
  SAMPLE_DISPLAYCOL_REC,
  SAMPLE_DISPLAYCOL_LAST
};

struct _SampleDisplay
{
  GtkWidget widget;
    
  GdkGC          *gcs[SCHEME_ELEMENT_LAST];
  GdkColor *gc_colors[SCHEME_ELEMENT_LAST];

  GdkPixmap * backing_pixmap;

  int width, height; /* Width and height of the widget */

  sw_view * view; /* The view (and hence, sample) we're displaying */

  /* current user offset of the sample */
  int user_offset_x, old_user_offset_x;

  /* previous play_offset drawn */
  int play_offset_x, old_play_offset_x;

  /* current recording offset */
  int rec_offset_x, old_rec_offset_x;

  gint mouse_x;
  glong mouse_offset; /* what the pointer is currently pointing to */

  int selecting; /* Current state of this sample-display */
  int selection_mode; /* Mode of selection: replace or intersect */

  gint marching_tag; /* gtk_timeout tag for marching ants */
  gboolean marching; /* whether or not ants are marching */

  gint pulsing_tag; /* gtk_timeout tag for cursor pulse */
  gboolean pulse;

  gint hand_scroll_tag;   /* gtk_timeout tag for natural hand scrolling */
  gint hand_scroll_delta; /* natural hand scrolling */

  /* Window panning */
  int selecting_x0;  /* the coordinate where the mouse was clicked */
  int selecting_wins0; /* stored value of view->v_start when the mouse
			* was clicked */

  /* Scrolling timeout tags */
  gint scroll_left_tag, scroll_right_tag;

  /* Meta key down? */
  gboolean meta_down;
    
  SweepScheme *scheme;
    
};

struct _SampleDisplayClass
{
  GtkWidgetClass parent_class;

  //GdkColor colors[SAMPLE_DISPLAYCOL_LAST];
  //GdkColor bg_colors[VIEW_COLOR_MAX];
  //GdkColor fg_colors[VIEW_COLOR_MAX];

  void (*selection_changed)(SampleDisplay *s, int start, int end);
  void (*window_changed)(SampleDisplay *s, int start, int end);
  void (*mouse_offset_changed)(SampleDisplay *s, int mouse_offset);
};

GType
sample_display_get_type (void);

GtkWidget*
sample_display_new (void);

void
sample_display_refresh (SampleDisplay *s);

sw_framecount_t
sample_display_get_mouse_offset (SampleDisplay * s);

void
sample_display_set_view (SampleDisplay *s, sw_view *view);

void
sample_display_refresh_user_marker (SampleDisplay *s);

void
sample_display_refresh_play_marker (SampleDisplay *s);

void
sample_display_refresh_rec_marker (SampleDisplay *s);

void
sample_display_set_cursor (SampleDisplay * s, GdkCursor * cursor);

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

void
sample_display_refresh_scheme_data (SampleDisplay *s, gboolean redraw);

void
sample_display_set_scheme (SampleDisplay *s, SweepScheme *scheme);

#endif /* _SAMPLE_DISPLAY_H */
