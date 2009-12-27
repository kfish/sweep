/*
 * Sweep, a sound wave editor.
 *
 * Copyright (C) 2000 Conrad Parker
 *
 * This widget was originally based on sample-display by Michael Krause
 * in ``The Real Soundtracker'', Copyright (C) 1998-1999 Michael Krause
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

#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "sample-display.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <sweep/sweep_i18n.h>

#include "sweep_app.h"
#include "sample.h"
#include "head.h"
#include "cursors.h"
#include "play.h"
#include "callbacks.h"
#include "edit.h"
#include "undo_dialog.h"

/*#define DEBUG*/

/*#define DOUBLE_BUFFER */

/*#define ALWAYS_REDRAW_ALL*/

/*#define LEGACY_DRAW_MODE*/

#define SEL_SCRUBS

extern GdkCursor * sweep_cursors[];

/* Maximum number of samples to consider per pixel */
#define STEP_MAX 32


/* Whether or not to compile in support for
 * drawing the crossing vectors
 */
/* #define DRAW_CROSSING_VECTORS */

#define PIXEL_TO_OFFSET(p) \
  ((sw_framecount_t)(((gdouble)(p)) * (((gdouble)(s->view->end - s->view->start)) / ((gdouble)s->width))))

#define XPOS_TO_OFFSET(x) \
  CLAMP(s->view->start + PIXEL_TO_OFFSET(x), 0, s->view->sample->sounddata->nr_frames)

#define SAMPLE_TO_PIXEL(n) \
  ((sw_framecount_t)((gdouble)(n) * (gdouble)s->width / (gdouble)(s->view->end - s->view->start)))

#define OFFSET_TO_XPOS(o) \
  SAMPLE_TO_PIXEL((o) - s->view->start)

#define OFFSET_RANGE(l, x) ((x) < 0 ? 0 : ((x) >= (l) ? (l) - 1 : (x)))

#define SET_CURSOR(w, c) \
  gdk_window_set_cursor ((w)->window, sweep_cursors[SWEEP_CURSOR_##c])

#define YPOS_TO_CHANNEL(y) \
  (s->view->sample->sounddata->format->channels * y / s->height)

#define CHANNEL_HEIGHT \
  (s->height / s->view->sample->sounddata->format->channels)

#define YPOS_TO_VALUE(y) \
  ((sw_audio_t)(CHANNEL_HEIGHT/2 - (y - (YPOS_TO_CHANNEL(y) * CHANNEL_HEIGHT)))/(CHANNEL_HEIGHT/2))

#define MARCH_INTERVAL 300
#define PULSE_INTERVAL 450
#define HAND_SCROLL_INTERVAL 50

extern sw_view * last_tmp_view;

static int last_button; /* button index which started the last selection;
			 * This is global to allow comparison for
			 * last_tmp_view
			 */

static const int default_colors[] = {
  200, 200, 193,  /* bg */
  199, 203, 158,  /* fg */
  0, 0xaa, 0, /* play (mask) */
  220, 230, 255, /* user */
  100, 100, 100, /* zero */
  240, 230, 240, /* sel box */
  110, 110, 100, /* tmp_sel XOR mask */
  108, 115, 134,    /* sel bg */
  166, 166, 154, /* minmax */
  240, 250, 240, /* highlight */
  81, 101, 81,   /* lowlight */
  230, 0, 0,     /* rec */
};

static const int bg_colors[] = {
  250, 250, 237,    /* black bg */
  200, 200, 193,    /* red bg */
  147, 147, 140,    /* orange bg */
  160, 160, 150,    /* yellow bg */
  250, 250, 237,    /* blue bg */
  160, 160, 150,    /* white bg */
  0, 0, 0,          /* greenscreen bg */
  60, 70, 170,          /* bluescreen bg */
};

static const int fg_colors[] = {
  80, 80, 60,    /* black fg */
  220, 80, 40,   /* red fg */
  220, 170, 120, /* orange fg */
  199, 203, 158, /* yellow fg */
  128, 138, 184, /* blue fg */
  230, 240, 255, /* white fg */
  0, 220, 0,     /* greenscreen fg */
  240, 240, 240,     /* bluescreen fg */
};

/* Values for s->selecting */
enum {
  SELECTING_NOTHING = 0,
  SELECTING_SELECTION_START,
  SELECTING_SELECTION_END,
  SELECTING_PAN_WINDOW,
  SELECTING_PLAYMARKER,
  SELECTING_PENCIL,
  SELECTING_NOISE,
  SELECTING_HAND,
};

enum {
  SELECTION_MODE_NONE = 0, /* Not selecting; used for consistency check. */
  SELECTION_MODE_REPLACE,
  SELECTION_MODE_INTERSECT,
  SELECTION_MODE_SUBTRACT,
  SELECTION_MODE_MAX
};

enum {
  SIG_SELECTION_CHANGED,
  SIG_WINDOW_CHANGED,
  SIG_MOUSE_OFFSET_CHANGED,
  LAST_SIGNAL
};

static gchar * selection_mode_names[SELECTION_MODE_MAX] = {
  N_(""), /* NONE */
  N_("New selection"), /* REPLACE */
  N_("Selection: add/modify region"), /* INTERSECT */
  N_("Selection: subtract region"), /* SUBTRACT */
};

#define IS_INITIALIZED(s) (s->view != NULL)

static guint sample_display_signals[LAST_SIGNAL] = { 0 };

static gint8 sel_dash_list[2] = { 4, 4 }; /* Equivalent to GDK's default
					  *  dash list.
					  */

void
sample_display_refresh (SampleDisplay *s)
{
  sw_sample * sample;

  g_return_if_fail(s != NULL);
  g_return_if_fail(IS_SAMPLE_DISPLAY(s));

  if(!IS_INITIALIZED(s))
    return;

  sample = s->view->sample;

  s->old_user_offset_x = s->user_offset_x =
    OFFSET_TO_XPOS(sample->user_offset);

  s->old_play_offset_x = s->play_offset_x =
    OFFSET_TO_XPOS(sample->play_head->offset);

  if (sample->rec_head != NULL) {
    s->old_rec_offset_x = s->rec_offset_x =
      OFFSET_TO_XPOS(sample->rec_head->offset);
  }

  gtk_widget_queue_draw_area (GTK_WIDGET(s), 0, 0, s->width, s->height);
}

sw_framecount_t
sample_display_get_mouse_offset (SampleDisplay * s)
{
  int x, y;
  GdkModifierType state;

  gdk_window_get_pointer (GTK_WIDGET(s)->window, &x, &y, &state);

  return XPOS_TO_OFFSET(x);
}

void
sample_display_set_view (SampleDisplay *s, sw_view *view)
{
  g_return_if_fail(s != NULL);
  g_return_if_fail(IS_SAMPLE_DISPLAY(s));

  s->view = view;
  s->old_user_offset_x = -1;
  s->user_offset_x = -1;
  s->old_play_offset_x = -1;
  s->play_offset_x = -1;

  /*  gtk_signal_emit(GTK_OBJECT(s), sample_display_signals[SIG_WINDOW_CHANGED], s->view->start, s->view->start + (s->view->end - s->view->start));*/
	
  s->selecting = SELECTING_NOTHING;
  s->selection_mode = SELECTION_MODE_NONE;

  gtk_widget_queue_draw(GTK_WIDGET(s));
}

void
sample_display_refresh_user_marker (SampleDisplay *s)
{
  sw_sample * sample;
  gint x, width;

  g_return_if_fail(s != NULL);
  g_return_if_fail(IS_SAMPLE_DISPLAY(s));

  if(!IS_INITIALIZED(s))
    return;

  sample = s->view->sample;

  s->user_offset_x = OFFSET_TO_XPOS(sample->user_offset);

  /* paint changed user cursor pos */
  if (s->old_user_offset_x != s->user_offset_x) {
    x = CLAMP (s->old_user_offset_x - 15, 0, s->width);
    width = MIN (s->width - x, 29);
    gtk_widget_queue_draw_area (GTK_WIDGET(s), x, 0, width, s->height);
  }
  
  /* paint cursor */
  if (s->user_offset_x >= 0 && s->user_offset_x <= s->width) {
    x = CLAMP (s->user_offset_x - 15, 0, s->width);
    width = MIN (s->width - x, 29);

    gtk_widget_queue_draw_area (GTK_WIDGET(s), x, 0, width, s->height);

    s->old_user_offset_x = s->user_offset_x;
  }
}

void
sample_display_refresh_play_marker (SampleDisplay *s)
{
  sw_sample * sample;
  sw_head * head;
  gint x, width;

  g_return_if_fail(s != NULL);
  g_return_if_fail(IS_SAMPLE_DISPLAY(s));

  if(!IS_INITIALIZED(s))
    return;

  sample = s->view->sample;
  head = sample->play_head;

  s->play_offset_x = OFFSET_TO_XPOS(head->offset);

  /* paint play offset */
  if (s->old_play_offset_x != s->play_offset_x) {
    x = CLAMP (s->old_play_offset_x - 15, 0, s->width);
    width = MIN (s->width - x, 29);
    gtk_widget_queue_draw_area (GTK_WIDGET(s), x, 0, width, s->height);
  }
  
  if (s->play_offset_x >= 0 && s->play_offset_x <= s->width) {
    x = CLAMP (s->play_offset_x - 15, 0, s->width);
    width = MIN (s->width - x, 29);
    gtk_widget_queue_draw_area (GTK_WIDGET(s), x, 0, width, s->height);

    s->old_play_offset_x = s->play_offset_x;
  }
}

void
sample_display_refresh_rec_marker (SampleDisplay *s)
{
  sw_sample * sample;
  sw_head * rec_head;
  gint x, width;

  g_return_if_fail(s != NULL);
  g_return_if_fail(IS_SAMPLE_DISPLAY(s));

  if(!IS_INITIALIZED(s))
    return;

  sample = s->view->sample;
  rec_head = sample->rec_head;

  if (rec_head == NULL) return;

  s->rec_offset_x = OFFSET_TO_XPOS(rec_head->offset);

  /* paint rec offset */
  if (s->old_rec_offset_x != s->rec_offset_x) {
    x = CLAMP (s->old_rec_offset_x - 15, 0, s->width);
    width = MIN (s->width - x, 29);
    gtk_widget_queue_draw_area (GTK_WIDGET(s), x, 0, width, s->height);
  }
  
  if (s->rec_offset_x >= 0 && s->rec_offset_x <= s->width) {
    x = CLAMP (s->rec_offset_x - 15, 0, s->width);
    width = MIN (s->width - x, 29);
    gtk_widget_queue_draw_area (GTK_WIDGET(s), x, 0, width, s->height);

    s->old_rec_offset_x = s->rec_offset_x;
  }
}

static void
sample_display_refresh_sels (SampleDisplay * s)
{
  int x, x2;
  sw_sample * sample;
  GList * gl;
  sw_sel * sel;

  g_return_if_fail(s != NULL);
  g_return_if_fail(IS_SAMPLE_DISPLAY(s));

  if(!IS_INITIALIZED(s))
    return;

  sample = s->view->sample;

  /* paint marching ants */
    
  /* real selection */
  for (gl = sample->sounddata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;
      
    x = OFFSET_TO_XPOS(sel->sel_start);
    x2 = OFFSET_TO_XPOS(sel->sel_end);

    if ((x >= 0) && (x <= s->width)) {
      gtk_widget_queue_draw_area (GTK_WIDGET(s), x, 0, 1, s->height);
    }
    if ((x2 >= 0) && (x2 <= s->width)) {
      gtk_widget_queue_draw_area (GTK_WIDGET(s), x2, 0, 1, s->height);
    }

    if ((x <= s->width) && (x2 >= 0)) {
      x = CLAMP (x, 0, s->width);
      x2 = CLAMP (x2, 0, s->width);
      gtk_widget_queue_draw_area (GTK_WIDGET(s), x, 0, x2 - x, 1);
      gtk_widget_queue_draw_area (GTK_WIDGET(s), x, s->height - 1, x2 - x, 1);
    }
  }
    
  /* temporary selection */
  sel = sample->tmp_sel;

  if (sel) {
    x = OFFSET_TO_XPOS(sel->sel_start);
    x2 = OFFSET_TO_XPOS(sel->sel_end);

    if ((x >= 0) && (x <= s->width)) {
      gtk_widget_queue_draw_area (GTK_WIDGET(s), x, 0, 1, s->height);
    }
    if ((x2 >= 0) && (x2 <= s->width)) {
      gtk_widget_queue_draw_area (GTK_WIDGET(s), x2, 0, 1, s->height);
    }

    if ((x <= s->width) && (x2 >= 0)) {
      x = CLAMP (x, 0, s->width);
      x2 = CLAMP (x2, 0, s->width);
      gtk_widget_queue_draw_area (GTK_WIDGET(s), x, 0, x2 - x, 1);
      gtk_widget_queue_draw_area (GTK_WIDGET(s), x, s->height - 1, x2 - x, 1);
    }
  }
}

void
sample_display_set_cursor (SampleDisplay * s, GdkCursor * cursor)
{
  gdk_window_set_cursor (GTK_WIDGET(s)->window, cursor);
}

void
sample_display_set_default_cursor (SampleDisplay * s)
{
  GdkCursor * cursor;

  switch (s->view->current_tool) {
  case TOOL_SELECT:
    cursor = sweep_cursors[SWEEP_CURSOR_CROSSHAIR];
    break;
  case TOOL_ZOOM:
    cursor = sweep_cursors[SWEEP_CURSOR_ZOOM_IN];
    break;
  case TOOL_MOVE:
    cursor = sweep_cursors[SWEEP_CURSOR_MOVE];
    break;
  case TOOL_SCRUB:
    cursor = sweep_cursors[SWEEP_CURSOR_NEEDLE];
    break;
  case TOOL_PENCIL:
    cursor = sweep_cursors[SWEEP_CURSOR_PENCIL];
    break;
  case TOOL_NOISE:
    cursor = sweep_cursors[SWEEP_CURSOR_NOISE];
    break;
  case TOOL_HAND:
    cursor = sweep_cursors[SWEEP_CURSOR_HAND_OPEN];
    break;
  default:
    cursor = NULL;
    break;
  }

  gdk_window_set_cursor (GTK_WIDGET(s)->window, cursor);
}

static void
sample_display_set_intersect_cursor (SampleDisplay * s)
{
  sw_sample * sample = s->view->sample;
  GtkWidget * widget = GTK_WIDGET(s);

  /* Check if there are other selection regions.
   * NB. This assumes that tmp_sel has already been
   * set.
   */
  if (sample_sel_nr_regions(sample) > 0) {
    SET_CURSOR(widget, HORIZ_PLUS);
  } else {
    SET_CURSOR(widget, HORIZ);
  }
}

static void
sample_display_set_subtract_cursor (SampleDisplay * s)
{
  sw_sample * sample = s->view->sample;
  GtkWidget * widget = GTK_WIDGET(s);

  /* Check if there are other selection regions.
   * NB. This assumes that tmp_sel has already been
   * set.
   */
  if (sample_sel_nr_regions(sample) > 0) {
    SET_CURSOR(widget, HORIZ_MINUS);
  } else {
    SET_CURSOR(widget, HORIZ);
  }
}

void
sample_display_set_window (SampleDisplay *s,
			   sw_framecount_t start,
			   sw_framecount_t end)
{
  sw_framecount_t len, vlen;

  g_return_if_fail(s != NULL);
  g_return_if_fail(IS_SAMPLE_DISPLAY(s));

  len = s->view->sample->sounddata->nr_frames;
  vlen = end - start;
  
  g_return_if_fail(end >= start);


  if (vlen > len) {
    /* Align to middle if entire length of sample is visible */
    start = (len - vlen) / 2;
    end = start + vlen;
  } else if (vlen == 0 && len > 0) {
    /* Zoom normal if window is zero but there is data; eg. after pasting
     * into an empty buffer */
    start = 0;
    end = MIN (len, s->width * 1024);
  }

  s->view->start = start;
  s->view->end = end;

  sample_display_refresh_user_marker (s);

  g_signal_emit_by_name(GTK_OBJECT(s), "window-changed");

  s->mouse_offset = XPOS_TO_OFFSET (s->mouse_x);
  g_signal_emit_by_name(GTK_OBJECT(s),
		  "mouse-offset-changed");

  gtk_widget_queue_draw(GTK_WIDGET(s));
}

static void
sample_display_init_display (SampleDisplay *s,
			     int w,
			     int h)
{
  GdkWindow * window;
  GdkVisual * visual;
  sw_framecount_t len, vlen, vlendelta;


  /* If the window was already displaying data before this event
   * was received, we are handling a resize event.
   * We want to ensure that the relative size of displayed data
   * remains constant over the resize -- the window simply 'uncovers'
   * or 'covers' the visualisation.
   */
  if (s->width > 1) {

    len = s->view->sample->sounddata->nr_frames;
    vlen = s->view->end - s->view->start;

    /* If we're already viewing EXACTLY the entire sample,
     * and dealing with a widening of the window, then
     * allow the visualisation to stretch. */
    if (s->view->start == 0 && vlen == len && w > s->width)
      goto stretch;

    /*
     * Funky integer error minimisation: this gives non-lossy results,
     *  as opposed to just using:
     *
     * s->view->end = s->view->start +
     *   (s->view->end - s->view->start) * w / s->width;
     *
     * However there is a noticeable waver when resizing the display
     * when zoomed in far enough that individual samples are visible.
     *
     * The alternative is to represent s->view->end (from which the
     * visible length is determined) as a floating point number.
     */

    vlendelta = vlen * (w - s->width) / s->width;

    if (vlen+vlendelta > len) {
      s->view->start = (len - (vlen+vlendelta)) / 2;
      s->view->end = s->view->start + vlen + vlendelta;
    } else if (s->view->start < 0) {
      s->view->end += vlendelta - s->view->start;
      s->view->start = 0;
    } else if (s->view->end > len) {
      s->view->start = s->view->end - vlen - vlendelta;
      s->view->end = len;
    } else {
      s->view->end += vlendelta;
    }

    g_signal_emit_by_name(GTK_OBJECT(s), "window-changed");
  }

 stretch:
  
  s->width = w;
  s->height = h;

  window = GTK_WIDGET(s)->window;
  visual = gdk_rgb_get_visual();

#if DOUBLE_BUFFER
  if(s->backing_pixmap) {
    g_object_unref(s->backing_pixmap);
  }
  s->backing_pixmap = gdk_pixmap_new (GTK_WIDGET(s)->window,  
                                      w, h, visual->depth);
#endif

}

static void
sample_display_size_request (GtkWidget *widget,
			     GtkRequisition *requisition)
{
  requisition->width = 40;
  requisition->height = 20;
}

static void
sample_display_size_allocate (GtkWidget *widget,
			      GtkAllocation *allocation)
{
  SampleDisplay *s;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (IS_SAMPLE_DISPLAY (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget)) {
    s = SAMPLE_DISPLAY (widget);

    gdk_window_move_resize (widget->window,
			    allocation->x, allocation->y,
			    allocation->width, allocation->height);

    sample_display_init_display(s, allocation->width, allocation->height);
  }
}

static void
sample_display_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  SampleDisplay *s;
  gint i;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (IS_SAMPLE_DISPLAY (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  s = SAMPLE_DISPLAY(widget);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = GDK_ALL_EVENTS_MASK;

  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (widget->parent->window,
				   &attributes, attributes_mask);

  widget->style = gtk_style_attach (widget->style, widget->window);

  s->width = 0;

  s->bg_gc = gdk_gc_new(widget->window);
  gdk_gc_set_foreground(s->bg_gc, &SAMPLE_DISPLAY_CLASS(GTK_WIDGET_GET_CLASS(widget))->colors[SAMPLE_DISPLAYCOL_BG]);

  s->fg_gc = gdk_gc_new(widget->window);
  gdk_gc_set_foreground(s->fg_gc, &SAMPLE_DISPLAY_CLASS(GTK_WIDGET_GET_CLASS(widget))->colors[SAMPLE_DISPLAYCOL_FG]);

  s->zeroline_gc = gdk_gc_new(widget->window);
  gdk_gc_set_foreground(s->zeroline_gc, &SAMPLE_DISPLAY_CLASS(GTK_WIDGET_GET_CLASS(widget))->colors[SAMPLE_DISPLAYCOL_ZERO]);

  s->play_gc = gdk_gc_new(widget->window);
  gdk_gc_set_foreground(s->play_gc, &SAMPLE_DISPLAY_CLASS(GTK_WIDGET_GET_CLASS(widget))->colors[SAMPLE_DISPLAYCOL_PLAY]);
  gdk_gc_set_function (s->play_gc, GDK_OR_REVERSE);

  s->user_gc = gdk_gc_new(widget->window);
  gdk_gc_set_foreground(s->user_gc, &SAMPLE_DISPLAY_CLASS(GTK_WIDGET_GET_CLASS(widget))->colors[SAMPLE_DISPLAYCOL_PAUSE]);

  s->rec_gc = gdk_gc_new(widget->window);
  gdk_gc_set_foreground(s->rec_gc, &SAMPLE_DISPLAY_CLASS(GTK_WIDGET_GET_CLASS(widget))->colors[SAMPLE_DISPLAYCOL_REC]);
  
  s->sel_gc = gdk_gc_new(widget->window);
  gdk_gc_set_foreground(s->sel_gc, &SAMPLE_DISPLAY_CLASS(GTK_WIDGET_GET_CLASS(widget))->colors[SAMPLE_DISPLAYCOL_SEL]);
  gdk_gc_set_line_attributes(s->sel_gc, 1 /* line width */,
			     GDK_LINE_DOUBLE_DASH,
			     GDK_CAP_BUTT,
			     GDK_JOIN_MITER);

  s->tmp_sel_gc = gdk_gc_new(widget->window);
  gdk_gc_set_foreground(s->tmp_sel_gc, &SAMPLE_DISPLAY_CLASS(GTK_WIDGET_GET_CLASS(widget))->colors[SAMPLE_DISPLAYCOL_TMP_SEL]);
  gdk_gc_set_function (s->tmp_sel_gc, GDK_XOR);

  s->crossing_gc = gdk_gc_new(widget->window);
  gdk_gc_set_foreground(s->crossing_gc, &SAMPLE_DISPLAY_CLASS(GTK_WIDGET_GET_CLASS(widget))->colors[SAMPLE_DISPLAYCOL_CROSSING]);

  s->minmax_gc = gdk_gc_new (widget->window);
  gdk_gc_set_foreground(s->minmax_gc, &SAMPLE_DISPLAY_CLASS(GTK_WIDGET_GET_CLASS(widget))->colors[SAMPLE_DISPLAYCOL_MINMAX]);

  s->highlight_gc = gdk_gc_new (widget->window);
  gdk_gc_set_foreground(s->highlight_gc, &SAMPLE_DISPLAY_CLASS(GTK_WIDGET_GET_CLASS(widget))->colors[SAMPLE_DISPLAYCOL_HIGHLIGHT]);

  s->lowlight_gc = gdk_gc_new (widget->window);
  gdk_gc_set_foreground(s->lowlight_gc, &SAMPLE_DISPLAY_CLASS(GTK_WIDGET_GET_CLASS(widget))->colors[SAMPLE_DISPLAYCOL_LOWLIGHT]);

  for (i = 0; i < VIEW_COLOR_MAX; i++) {
    s->bg_gcs[i] = gdk_gc_new (widget->window);
    gdk_gc_set_foreground(s->bg_gcs[i], &SAMPLE_DISPLAY_CLASS(GTK_WIDGET_GET_CLASS(widget))->bg_colors[i]);

    s->fg_gcs[i] = gdk_gc_new (widget->window);
    gdk_gc_set_foreground(s->fg_gcs[i], &SAMPLE_DISPLAY_CLASS(GTK_WIDGET_GET_CLASS(widget))->fg_colors[i]);
  }

  sample_display_init_display(s, attributes.width, attributes.height);

  sample_display_set_default_cursor (s);

  gdk_window_set_user_data (widget->window, widget);
}


static void
sample_display_draw_data_channel (GdkDrawable * win,
				  const SampleDisplay * s,
				  int x,
				  int y,
				  int width,
				  int height,
				  int channel)
{
  GList * gl;
  GdkGC * gc, * fg_gc;
  sw_sel * sel;
  int x1, x2, y1;
  sw_audio_t vhigh, vlow;
  sw_audio_intermediate_t totpos, totneg;
  sw_audio_t d, maxpos, avgpos, minneg, avgneg;
  sw_audio_t prev_maxpos, prev_minneg;
  sw_framecount_t i, step, nr_frames, nr_pos, nr_neg;
  sw_sample * sample;
  const int channels = s->view->sample->sounddata->format->channels;

  sample = s->view->sample;

  fg_gc = s->fg_gcs[sample->color];

  gdk_draw_rectangle(win, s->bg_gcs[sample->color],
		     TRUE, x, y, width, height);

  /* Draw real selection */
  for (gl = sample->sounddata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    x1 = OFFSET_TO_XPOS(sel->sel_start);
    x1 = CLAMP(x1, x, x+width);

    x2 = OFFSET_TO_XPOS(sel->sel_end);
    x2 = CLAMP(x2, x, x+width);

    if (x2 - x1 > 1){ 
      gdk_draw_rectangle (win, s->crossing_gc, TRUE,
			  x1, y, x2 - x1, y + height -1);
    }
  }

  /* Draw temporary selection */
  sel = sample->tmp_sel;

  if (sel) {
    if (sel->sel_start != sel->sel_end) {
      x1 = OFFSET_TO_XPOS(sel->sel_start);
      x1 = CLAMP(x1, x, x+width);
      
      x2 = OFFSET_TO_XPOS(sel->sel_end);
      x2 = CLAMP(x2, x, x+width);
      
      if (x2 - x1 > 1) {
	gdk_draw_rectangle (win, s->tmp_sel_gc, TRUE,
			    x1, y, x2 - x1, y + height -1);
      }
    }
  }

  vhigh = s->view->vhigh;
  vlow = s->view->vlow;

#define YPOS(v) CLAMP(y + height - ((((v) - vlow) * height) \
		               / (vhigh - vlow)), y, y+height)

  /* Draw zero and 6db lines */
  y1 = YPOS(0.5);
  gdk_draw_line(win, s->zeroline_gc,
		x, y1, x + width - 1, y1);

  y1 = YPOS(0.0);
  gdk_draw_line(win, s->zeroline_gc,
		x, y1, x + width - 1, y1);

  y1 = YPOS(-0.5);
  gdk_draw_line(win, s->zeroline_gc,
		x, y1, x + width - 1, y1);

  totpos = totneg = 0.0;
  maxpos = minneg = prev_maxpos = prev_minneg = 0.0;

  nr_frames = sample->sounddata->nr_frames;

  /* 'step' ensures that no more than STEP_MAX values get looked at
   * per pixel */
  step = MAX (1, PIXEL_TO_OFFSET(1)/STEP_MAX);

#ifdef LEGACY_DRAW_MODE
  {
    int py, ty;
    sw_audio_t peak;

    py = y+height/2;

    while (width >= 0) {
      peak = 0;
      for (i = OFFSET_RANGE(nr_frames, XPOS_TO_OFFSET(x));
	   i < OFFSET_RANGE(nr_frames, XPOS_TO_OFFSET(x+1));
	   i+=step) {
	d = ((sw_audio_t *)sample->sounddata->data)[i*channels + channel];
	if (fabs(d) > fabs(peak)) peak = d;
      }

      ty = YPOS(peak);

      gdk_draw_line (win, fg_gc, x-1, py, x, ty);

      py = ty;

      x++;
      width--;
    }
  }

#else

  for (i = OFFSET_RANGE (nr_frames, XPOS_TO_OFFSET(x-1));
       i < OFFSET_RANGE (nr_frames, XPOS_TO_OFFSET(x));
       i += step) {
    d = ((sw_audio_t *)sample->sounddata->data)[i*channels + channel];
    if (d >= 0) {
      if (d > prev_maxpos) prev_maxpos = d;
    } else {
      if (d < prev_minneg) prev_minneg = d;
    }
  }

  while(width >= 0) {
    nr_pos = nr_neg = 0;
    totpos = totneg = 0;
    
    maxpos = minneg = 0;

    /* lock the sounddata against destructive ops to make sure
     * sounddata->data doesn't change under us */
    g_mutex_lock (sample->ops_mutex);

    for (i = OFFSET_RANGE(nr_frames, XPOS_TO_OFFSET(x));
	 i < OFFSET_RANGE(nr_frames, XPOS_TO_OFFSET(x+1));
	 i+=step) {
      d = ((sw_audio_t *)sample->sounddata->data)[i*channels + channel];
      if (d >= 0) {
	if (d > maxpos) maxpos = d;
	totpos += d;
	nr_pos++;
      } else {
	if (d < minneg) minneg = d;
	totneg += d;
	nr_neg++;
      }
    }

    g_mutex_unlock (sample->ops_mutex);
    
    if (nr_pos > 0) {
      avgpos = totpos / nr_pos;
    } else {
      avgpos = 0;
    }

    if (nr_neg > 0) {
      avgneg = totneg / nr_neg;
    } else {
      avgneg = 0;
    }

    gdk_draw_line(win, s->minmax_gc,
		  x, YPOS(maxpos),
		  x, YPOS(minneg));

    gc = maxpos > prev_maxpos ? s->highlight_gc : s->lowlight_gc;
    gdk_draw_line(win, gc,
		  x, YPOS(prev_maxpos),
		  x, YPOS(maxpos));

    gc = minneg > prev_minneg ? s->lowlight_gc : s->highlight_gc;
    gdk_draw_line(win, gc,
		  x, YPOS(prev_minneg),
		  x, YPOS(minneg));

    gdk_draw_line(win, fg_gc,
		  x, YPOS(avgpos),
		  x, YPOS(avgneg));

    prev_maxpos = maxpos;
    prev_minneg = minneg;

    x++;
    width--;
  }
#endif

}

static void
sample_display_draw_data (GdkDrawable *win, const SampleDisplay *s,
			  int x, int width)
{
  const int sh = s->height;
  int start_x, end_x, i, cy, cheight, cerr;
  const int channels = s->view->sample->sounddata->format->channels;

  if (width == 0)
    return;

  g_return_if_fail(x >= 0);
  g_return_if_fail(x + width <= s->width);

#ifdef DEBUG
  g_print("draw_data: view %u --> %u, drawing x=%d, width=%d\n",
	  s->view->start, s->view->end, x, width);
#endif

  start_x = OFFSET_TO_XPOS(0);
  end_x = OFFSET_TO_XPOS(s->view->sample->sounddata->nr_frames);

  if (start_x > x + width || end_x < x) {
    gtk_style_apply_default_background (GTK_WIDGET(s)->style, win,
					TRUE, GTK_STATE_NORMAL,
					NULL,
					x, 0,
					width, sh);
    return;
  }

  if (start_x > x) {
    gtk_style_apply_default_background (GTK_WIDGET(s)->style, win,
					TRUE, GTK_STATE_NORMAL,
					NULL,
					x, 0,
					start_x - x, sh);

    if (start_x > 0)
      gdk_draw_line (win, s->lowlight_gc, start_x-1, 0, start_x-1, sh);

    x = start_x;
  }

  if (end_x < x + width) {
    gtk_style_apply_default_background (GTK_WIDGET(s)->style, win,
					TRUE, GTK_STATE_NORMAL,
					NULL,
					end_x, 0,
					x + width - end_x, sh);

    gdk_draw_line (win, s->highlight_gc,
		   end_x, 0, end_x, sh);

    width = end_x - x;
  }

  cheight = sh / channels;
  cerr = sh - (channels * cheight);
  if (cerr == channels - 1) {
    cheight++;
    cerr = 0;
  }
  cy = 0;

  for (i = 0; i < channels; i++) {
    if (i >= 0) {
      gtk_style_apply_default_background (GTK_WIDGET(s)->style, win,
					  TRUE, GTK_STATE_NORMAL, NULL,
					  x, cy, width, 1);
      cy += 1;
    }

    sample_display_draw_data_channel (win, s,
				      x, cy, width, cheight-2, i);
    cy += cheight-2;

    gtk_style_apply_default_background (GTK_WIDGET(s)->style, win,
					TRUE, GTK_STATE_NORMAL, NULL,
					x, cy, width, 1);
    cy += 1;

  }

  if (cy < sh) {
    gtk_style_apply_default_background (GTK_WIDGET(s)->style, win,
					TRUE, GTK_STATE_NORMAL, NULL,
					x, cy, width, sh-cy);
  }

}

/*** CROSSING VECTORS ***/

#ifdef DRAW_CROSSING_VECTORS

static void
sample_display_draw_crossing_vector (GdkDrawable * win,
				     GdkGC * gc,
				     const SampleDisplay * s,
				     int x)
{
  sw_sample * sample = s->view->sample;
  const int sh = s->height;
  int cx1, cx2, cy1, cy2;

#define VRAD 8

  cx1 = ( x > VRAD ? VRAD : 0 );
  cx2 = ( x < sample->sounddata->nr_frames - VRAD ? VRAD : 0 );

  cy1 = ((sw_audio_t *)sample->sounddata->data)[OFFSET_RANGE(sample->sounddata->nr_frames, XPOS_TO_OFFSET(x)) - cx1];
  cy2 = ((sw_audio_t *)sample->sounddata->data)[OFFSET_RANGE(sample->sounddata->nr_frames, XPOS_TO_OFFSET(x)) + cx2];
  
  gdk_draw_line(win, s->crossing_gc,
		x - cx1, (((cy1 + 1.0) * sh) / 2.0),
		x + cx2, (((cy2 + 1.0) * sh) / 2.0));
}	
			     
#endif

/*** MARCHING ANTS ***/

/*
 * sample_display_sel_box_march_ants ()
 *
 * gtk_idle function to move the marching ants used by
 * selections.
 */
static gint
sd_march_ants (gpointer data)
{
  SampleDisplay * s = (SampleDisplay *)data;
  GdkGC * gc = s->sel_gc;
  static int dash_offset = 0;

  gdk_gc_set_dashes (gc, dash_offset, sel_dash_list, 2);

  dash_offset++;
  dash_offset %= 8;

  sample_display_refresh_sels (s);

  return TRUE;
}

static void
sd_start_marching_ants_timeout (SampleDisplay * s)
{
  if (s->marching_tag > 0)
    g_source_remove (s->marching_tag);

  s->marching_tag = g_timeout_add (MARCH_INTERVAL,
				     (GSourceFunc)sd_march_ants,
				     s);
}

void
sample_display_start_marching_ants (SampleDisplay * s)
{
  sd_start_marching_ants_timeout (s);
  s->marching = TRUE;
}

static void
sd_stop_marching_ants_timeout (SampleDisplay * s)
{
  if (s->marching_tag > 0)
    g_source_remove (s->marching_tag);

  s->marching_tag = 0;
}

void
sample_display_stop_marching_ants (SampleDisplay * s)
{
  sd_stop_marching_ants_timeout (s);
  s->marching = FALSE;
}

/*** SELECTION BOXES ***/

static void
sample_display_draw_sel_box(GdkDrawable * win,
			    GdkGC * gc,
			    const SampleDisplay * s,
			    int x,
			    int width,
			    int draw_left, int draw_right)
{
  if (width <= 0) {
    gdk_draw_line (win, gc, x, 0, x, s->height - 1);
    return;
  }

  /* Must draw individual lines for these: if you optimise by
   * drawing a rectangle where possible, the marching ants go
   * crazy. We don't want that to happen, they are cute.
   */

  gdk_draw_line(win, gc,
		x, 0,
		x+width, 0);
  gdk_draw_line(win, gc,
		  x, s->height - 1,
		x+width, s->height - 1);
  if (draw_left) {
    gdk_draw_line(win, gc,
		  x, 0,
		  x, s->height - 1);
    
#ifdef DRAW_CROSSING_VECTORS
    /* crossing vector */
    sample_display_draw_crossing_vector (win, gc, s, x);
#endif
  }
  if (draw_right) {
    gdk_draw_line(win, gc,
		  x+width, 0,
		  x+width, s->height - 1);
    
#ifdef DRAW_CROSSING_VECTORS
    /* crossing vector */
    sample_display_draw_crossing_vector (win, gc, s, x+width);
#endif
  }
}

static void
sample_display_draw_sel (GdkDrawable * win,
			 const SampleDisplay * s,
			 int x_min, int x_max)
{
  sw_sample * sample = s->view->sample;
  GList * gl;
  sw_sel * sel;
  int x, x2;
  int l_end, r_end; /* draw left + right ends of sel */

  /* Draw real selection */
  for (gl = sample->sounddata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    x = OFFSET_TO_XPOS(sel->sel_start);
    x2 = OFFSET_TO_XPOS(sel->sel_end);

    if (x > x_max) break;
    if (x2 < x_min) continue;

    l_end = (x >= x_min) && (x <= x_max);
    x = CLAMP (x, x_min, x_max);
    
    r_end = (x2 >= x_min) && (x2 <= x_max);
    x2 = CLAMP (x2, x_min, x_max);
    
    /* draw the selection */
    sample_display_draw_sel_box(win, s->sel_gc,
				s, x, x2 - x - 1,
				l_end, r_end /* draw_ends */);


  }

  /* Draw temporary selection */
  sel = sample->tmp_sel;

  if (sel) {
    x = OFFSET_TO_XPOS(sel->sel_start);
    l_end = (x >= x_min) && (x <= x_max);
    x = CLAMP (x, x_min, x_max);

    x2 = OFFSET_TO_XPOS(sel->sel_end);
    r_end = (x2 >= x_min) && (x2 <= x_max);
    x2 = CLAMP (x2, x_min, x_max);

    /* draw the selection */
    sample_display_draw_sel_box(win, s->tmp_sel_gc,
				s, x, x2 - x - 1,
				l_end, r_end /* draw_ends */);
  }
}


static gint
sd_pulse_cursor (gpointer data)
{
  SampleDisplay * s = (SampleDisplay *)data;

  if (s->pulse)
    gdk_gc_set_function (s->user_gc, GDK_NOOP);
  else
    gdk_gc_set_function (s->user_gc, GDK_COPY);

  s->pulse = (!s->pulse);

  sample_display_refresh_user_marker (s);

  return TRUE;
}

void
sample_display_start_cursor_pulse (SampleDisplay * s)
{
  gdk_gc_set_function (s->user_gc, GDK_NOOP);

  sample_display_refresh_user_marker (s);

  s->pulsing_tag = g_timeout_add (PULSE_INTERVAL,
				    (GSourceFunc)sd_pulse_cursor, s);
}

void
sample_display_stop_cursor_pulse (SampleDisplay * s)
{
  if (s->pulsing_tag > 0)
    g_source_remove (s->pulsing_tag);

  s->pulsing_tag = 0;

  gdk_gc_set_function (s->user_gc, GDK_COPY);

  sample_display_refresh_user_marker (s);
}

static void
sample_display_draw_user_offset (GdkDrawable * win, GdkGC * gc,
				 SampleDisplay * s, int x,
				 int x_min, int x_max)
{
  GdkPoint poly[4];
  gboolean fill;

  if(x >= x_min && x <= x_max) {
    gdk_draw_line(win, s->zeroline_gc,
		  x-2, 0, 
		  x-2, s->height);

    gdk_draw_line(win, gc,
		  x-1, 0, 
		  x-1, s->height);
    gdk_draw_line(win, gc,
		  x+1, 0, 
		  x+1, s->height);

    gdk_draw_line(win, s->zeroline_gc,
		  x+2, 0, 
		  x+2, s->height);



    if (!s->view->sample->play_head->going) {
      fill = !s->view->sample->play_head->mute;

      if (x < 20) {
	poly[0].x = 11;
	poly[1].x = 11;
	poly[2].x = 14;
	poly[3].x = 14;
      } else {
	poly[0].x = x - 8;
	poly[1].x = x - 8;
	poly[2].x = x - 5;
	poly[3].x = x - 5;
      }

      poly[0].y = 4;
      poly[1].y = 14;
      poly[2].y = 14;
      poly[3].y = 4;

      gdk_draw_polygon (win, gc, fill, poly, 4);
      gdk_draw_polygon (win, s->zeroline_gc, FALSE, poly, 4);

      poly[0].x -= 5;
      poly[1].x -= 5;
      poly[2].x -= 5;
      poly[3].x -= 5;

      gdk_draw_polygon (win, gc, fill, poly, 4);
      gdk_draw_polygon (win, s->zeroline_gc, FALSE, poly, 4);
    }
  }
}

static void
sample_display_draw_play_offset (GdkDrawable * win, GdkGC * gc,
				 SampleDisplay * s, int x,
				 int x_min, int x_max)
{
  sw_sample * sample;
  GdkPoint poly[4];
  sw_head * head;

  if(x >= x_min && x <= x_max) {
    gdk_draw_rectangle(win, gc, TRUE,
		       x-1, 0, 
		       3, s->height);

    sample = s->view->sample;
    head = sample->play_head;

    if (head->going) {
      if (x < 20) {
	if (head->reverse) {
	  poly[0].x = 14;
	  poly[1].x = 14;
	  poly[2].x = 6;
	} else {
	  poly[0].x = 6;
	  poly[1].x = 6;
	  poly[2].x = 14;
	}
      } else {
	if (head->reverse) {
	  poly[0].x = x - 5;
	  poly[1].x = x - 5;
	  poly[2].x = x - 13;
	} else {
	  poly[0].x = x - 13;
	  poly[1].x = x - 13;
	  poly[2].x = x - 5;
	}
      }

      poly[0].y = 4;
      poly[1].y = 14;
      poly[2].y = 9;
      
      gdk_draw_polygon (win, gc, !head->mute, poly, 3);
      gdk_draw_polygon (win, s->zeroline_gc, FALSE, poly, 3);
    }
  }
}

static void
sample_display_draw_rec_offset (GdkDrawable * win, GdkGC * gc,
				SampleDisplay * s, int x,
				int x_min, int x_max)
{
  sw_sample * sample;

  if(x >= x_min && x <= x_max) {
    gdk_draw_line(win, s->zeroline_gc,
		  x-2, 0, 
		  x-2, s->height);

    gdk_draw_line(win, gc,
		  x-1, 0, 
		  x-1, s->height);
    gdk_draw_line(win, gc,
		  x+1, 0, 
		  x+1, s->height);

    gdk_draw_line(win, s->zeroline_gc,
		  x+2, 0, 
		  x+2, s->height);

    sample = s->view->sample;

    if (x < 20) x = 19;
    gdk_draw_arc (win, gc, TRUE, x-14, 15, 8, 8, 0, 360 * 64);
    gdk_draw_arc (win, s->zeroline_gc, FALSE, x-14, 15, 8, 8, 0, 360 * 64);

  }
}


/*** DRAW ***/

static void
sample_display_draw (GtkWidget *widget, GdkRectangle *area)
{
  SampleDisplay *s = SAMPLE_DISPLAY(widget);
  sw_sample * sample = s->view->sample;
  GdkDrawable * drawable;

  /*  g_return_if_fail(area->x >= 0);*/
  if (area->x < 0) return;
  if (area->y < 0) return;

  if(area->width == 0)
    return;

  if(area->x + area->width > s->width)
    return;

#ifdef DEBUG
    g_print ("sample_display_draw: (%d, %d) [%d, %d]\n",
	     area->x, area->y, area->width, area->height);
#endif

  if(!IS_INITIALIZED(s)) {
    gtk_style_apply_default_background (GTK_WIDGET(s)->style, widget->window,
					TRUE, GTK_STATE_NORMAL,
					NULL, 0, 0, s->width, s->height);
  } else {
    const int x_min = area->x;
    const int x_max = area->x + area->width;

#ifdef DOUBLE_BUFFER
    drawable = s->backing_pixmap;
#else
    drawable = widget->window;
#endif

    /* draw the sample graph */
    sample_display_draw_data(drawable, s, x_min, x_max - x_min);

    /* draw the selection bounds */
    sample_display_draw_sel (drawable, s, x_min, x_max);


    /* draw the offset cursors */

    if(!sample->play_head->going) {
      /* Draw user offset */
      sample_display_draw_user_offset (drawable, s->user_gc,
				       s, s->user_offset_x,
				       x_min, x_max);
    } else {
      /* Draw play offset */
      sample_display_draw_play_offset (drawable, s->play_gc,
				       s, s->play_offset_x,
				       x_min, x_max);
      /* Draw user offset */
      sample_display_draw_user_offset (drawable, s->user_gc,
				       s, s->user_offset_x,
				       x_min, x_max);

    }

    /* Draw rec offset */
    if (sample->rec_head /*&& sample->rec_head->transport_mode != SWEEP_TRANSPORT_STOP*/ ) {
      sample_display_draw_rec_offset (drawable, s->rec_gc,
				      s, s->rec_offset_x,
				      x_min, x_max);
    }

#ifdef DOUBLE_BUFFER
    gdk_draw_pixmap(widget->window, s->fg_gc, s->backing_pixmap,
		    area->x, area->y,
		    area->x, area->y,
		    area->width, area->height);
#endif
  }
}

/*** EVENT HANDLERS ***/


static gint
sample_display_expose (GtkWidget *widget,
		       GdkEventExpose *event)
{
  GdkRectangle * a;
  a = &event->area;

#ifdef DEBUG
  g_print ("received expose event for (%d, %d) [%d, %d]; %d follow\n",
	   a->x, a->y, a->width, a->height, event->count);
#endif

  sample_display_draw (widget, a);

  return FALSE;
}

static gint
sample_display_hand_scroll (SampleDisplay * s)
{
  gint new_win_start, win_length;
  gfloat step;
  
  win_length = s->view->end - s->view->start;

  step = win_length * 1.0 / s->width;

  new_win_start = s->view->start + s->hand_scroll_delta * step;
  
  new_win_start = CLAMP(new_win_start, 0,
			s->view->sample->sounddata->nr_frames -
			(s->view->end - s->view->start));

  if(new_win_start != s->view->start) {
    sample_display_set_window (s,
			       new_win_start,
			       new_win_start + win_length);
  } else {
	  s->hand_scroll_delta = 0;
  }
/*
  g_print ("s->delta: %i new_win_start: %i\n", s->hand_scroll_delta, new_win_start);
*/
  s->hand_scroll_delta *= 0.98;

  return (s->hand_scroll_delta != 0);
}

static gint
sample_display_scroll_left (gpointer data)
{
  SampleDisplay * s = (SampleDisplay *)data;
  int new_win_start, win_length;
  
  win_length = s->view->end - s->view->start;
  new_win_start = s->view->start - win_length/8;
  
  new_win_start = CLAMP(new_win_start, 0,
			s->view->sample->sounddata->nr_frames -
			(s->view->end - s->view->start));
  
  if(new_win_start != s->view->start) {
    sample_display_set_window (s,
			       new_win_start,
			       new_win_start + win_length);
  }

  s->view->sample->tmp_sel->sel_start = new_win_start;

  return (new_win_start > 0);
}

static gint
sample_display_scroll_right (gpointer data)
{
  SampleDisplay * s = (SampleDisplay *)data;
  int new_win_start, win_length;
  
  win_length = s->view->end - s->view->start;
  new_win_start = s->view->start + win_length/8;
  
  new_win_start = CLAMP(new_win_start, 0,
			s->view->sample->sounddata->nr_frames -
			(s->view->end - s->view->start));
  
  if(new_win_start != s->view->start) {
    sample_display_set_window (s,
			       new_win_start,
			       new_win_start + win_length);
  }

  s->view->sample->tmp_sel->sel_end = s->view->end;

  return (new_win_start >= (s->view->end - win_length));
}

static void
sample_display_handle_playmarker_motion (SampleDisplay * s, int x, int y)
{
  sw_sample * sample;
  sw_framecount_t offset;

  sample = s->view->sample;
  offset = XPOS_TO_OFFSET(x);

  sample_set_playmarker (sample, offset, TRUE);
}

void
sample_display_clear_sel (SampleDisplay * s)
{
  sample_clear_tmp_sel (s->view->sample);
  s->selecting = SELECTING_NOTHING;
  s->selection_mode = SELECTION_MODE_NONE;
  sample_display_set_default_cursor (s);
  sample_clear_tmp_message (s->view->sample);
  g_signal_emit_by_name(GTK_OBJECT(s),
		  "selection-changed");
}

static void
sample_display_handle_sel_motion (SampleDisplay *s,
			      int x,
			      int y,
			      int just_clicked)
{
  sw_sample * sample;
  sw_sel * sel;
  int o;
  int ss, se;
  gboolean scroll_left = FALSE, scroll_right = FALSE;

  if (s->view->current_tool != TOOL_SELECT) return;

  if(!s->selecting)
    return;

  if (!s->view->sample)
    return;

  if (!s->view->sample->tmp_sel)
    return;

  sample = s->view->sample;

  if (sample->edit_mode != SWEEP_EDIT_MODE_READY) {
    sample_display_clear_sel (s);
    return;
  }

  o = XPOS_TO_OFFSET(x);

#ifdef DEBUG
  if (o < 0) {
    g_print ("OI! setting an offset < 0!\n");
  }
#endif

  if (!sample->play_head->going) {
    sample_set_playmarker (sample, o, TRUE);
  }

  sel = sample->tmp_sel;

  ss = sel->sel_start;
  se = sel->sel_end;

  if(x < 0) {
    scroll_left = TRUE;
    x = 0;
  } else if(x >= s->width - 1) {
    scroll_right = TRUE;
    x = s->width - 1;
  }

  if (just_clicked) {
    ss = o;
    se = o+1;
  } else {
    switch (s->selecting) {
    case SELECTING_SELECTION_START:
      if (o < se) {
	ss = o;
      } else {
	if (o != ss+1) ss = se;
	se = o;
	s->selecting = SELECTING_SELECTION_END;      }
      break;
    case SELECTING_SELECTION_END:
      if (o > ss) {
	se = o;
      } else {
	if (o != se-1) se = ss;
	ss = o;
	s->selecting = SELECTING_SELECTION_START;
      }
      break;
    default:
      g_assert_not_reached ();
      break;
    }
  }

  if(sel->sel_start != ss || sel->sel_end != se || just_clicked) {
    sel->sel_start = ss;
    sel->sel_end = se;
    g_signal_emit_by_name(GTK_OBJECT(s),
		    "selection-changed");
  }

  if (scroll_left && s->scroll_left_tag == 0) {
    if (s->scroll_right_tag != 0) {
      g_source_remove (s->scroll_right_tag);
      s->scroll_right_tag = 0;
    }

    s->scroll_left_tag = g_timeout_add (100, sample_display_scroll_left,
					  (gpointer)s);

  } else if (scroll_right && s->scroll_right_tag == 0) {
    if (s->scroll_left_tag != 0) {
      g_source_remove (s->scroll_left_tag);
      s->scroll_left_tag = 0;
    }

    s->scroll_right_tag = g_timeout_add (100, sample_display_scroll_right,
					   (gpointer)s);

  } else if (!scroll_right && !scroll_left) {
    if (s->scroll_right_tag != 0) {
      g_source_remove (s->scroll_right_tag);
      s->scroll_right_tag = 0;
    }
    if (s->scroll_left_tag != 0) {
      g_source_remove (s->scroll_left_tag);
      s->scroll_left_tag = 0;
    }
  }
}

/* Handle middle mousebutton display window panning */
static void
sample_display_handle_move_motion (SampleDisplay *s, int x, int y)
{
  sw_sample * sample = s->view->sample;
  sw_framecount_t vlen, offset_xpos, new_offset;
  int new_win_start;

  vlen = s->view->end - s->view->start;

  offset_xpos = OFFSET_TO_XPOS(sample->user_offset);

  new_win_start =
    s->selecting_wins0 + (s->selecting_x0 - x) * vlen / s->width;

  new_win_start = CLAMP(new_win_start, 0,
			sample->sounddata->nr_frames - vlen);

  new_offset = new_win_start + offset_xpos * vlen / s->width;
  
  sample_set_scrubbing (sample, TRUE);
  sample_set_playmarker (sample, new_offset, TRUE);

  if(new_win_start != s->view->start) {
    sample_display_set_window (s,
			       new_win_start,
			       new_win_start + vlen);
  }
}

static void
sample_display_handle_pencil_motion (SampleDisplay * s, int x, int y)
{
  sw_sample * sample;
  sw_framecount_t offset;
  int channel, channels;
  sw_audio_t value;
  sw_audio_t * sampledata;

  offset = XPOS_TO_OFFSET(x);

  if (offset < s->view->start || offset > s->view->end) return;

  sample = s->view->sample;
  sampledata = (sw_audio_t *)sample->sounddata->data;
  channels = sample->sounddata->format->channels;

  y = CLAMP (y, 0, s->height);

  channel = YPOS_TO_CHANNEL(y);
  value = YPOS_TO_VALUE(y);
  sampledata[offset*channels + channel] = value;

  sample_refresh_views (sample);
}

static void
sample_display_handle_hand_motion (SampleDisplay * s, int x, int y)
{
  gdouble move, vstart, vend;
  gdouble step = (gdouble)(s->view->end - s->view->start) / ((gdouble)s->width);
  GtkAdjustment * adj = GTK_ADJUSTMENT(s->view->adj);
  gint delta;

  delta = s->view->hand_offset - x;

  s->hand_scroll_delta *= 0.9;

  if (abs (delta) > abs (s->hand_scroll_delta))
	  s->hand_scroll_delta = delta;
  
  if (s->view->hand_offset != x){
    move = s->view->hand_offset - x;
    move *= step;

    vstart = s->view->start + move;
    vend = s->view->end + move;

    if (vstart < 0){
	vstart = 0;
	vend = adj->page_size;
    }
    if (vend > s->view->sample->sounddata->nr_frames){
	vstart = s->view->sample->sounddata->nr_frames - adj->page_size; 
	vend = s->view->sample->sounddata->nr_frames;
    }
    
	vstart = ceil(vstart + (move < 0 ? 0.5 : -0.5));
    vend = ceil(vend + (move < 0 ? 0.5 : -0.5));

    if (s->view->start != vstart && s->view->end != vend)
	    s->view->hand_offset = x;

    s->view->start = vstart;
    s->view->end = vend;

    view_refresh_display(s->view);

    gtk_adjustment_set_value( GTK_ADJUSTMENT(s->view->adj), vstart);
  }
}

static void
sample_display_handle_noise_motion (SampleDisplay * s, int x, int y)
{
  sw_sample * sample;
  sw_framecount_t offset;
  int channel;
  sw_audio_t value, oldvalue;
  sw_audio_t * sampledata;

  offset = XPOS_TO_OFFSET(x);

  if (offset < s->view->start || offset > s->view->end) return;

  sample = s->view->sample;
  sampledata = (sw_audio_t *)sample->sounddata->data;

  y = CLAMP (y, 0, s->height);

  value = 2.0 * (random() - RAND_MAX/2) / (sw_audio_t)RAND_MAX;

  if (sample->sounddata->format->channels == 1) {
    oldvalue = sampledata[offset];
  } else {
    channel = YPOS_TO_CHANNEL(y);
    offset = offset*2 + channel;
    oldvalue = sampledata[offset];
  }

  sampledata[offset] = CLAMP(oldvalue * 0.8 + value * 0.2,
			     SW_AUDIO_T_MIN, SW_AUDIO_T_MAX);

  sample_refresh_views (sample);
}

static void
sample_display_handle_sel_button_press (SampleDisplay * s, int x, int y,
					GdkModifierType state)
{
  sw_sample * sample;
  GList * gl;
  sw_sel * sel, * tmp_sel = NULL;
  int xss, xse;
  int min_xs;
  gboolean just_clicked = TRUE;

  sample = s->view->sample;

  if (sample->edit_mode != SWEEP_EDIT_MODE_READY) {
    sample_display_clear_sel (s);
    return;
  }

  for (gl = sample->sounddata->sels; gl; gl = gl->next) {

    /* If the cursor is near the current start or end of
     * the selection, move that.
     */

    sel = (sw_sel *)gl->data;
	  
    xss = OFFSET_TO_XPOS(sel->sel_start);
    xse = OFFSET_TO_XPOS(sel->sel_end);
	  
    if(abs(x-xss) < 5) {
      sample_set_tmp_sel (sample, s->view, sel);
      s->selecting = SELECTING_SELECTION_START;
      s->selection_mode = SELECTION_MODE_INTERSECT;
      sample_display_set_intersect_cursor (s);
      just_clicked = FALSE;
      goto motion;
    } else if(abs(x-xse) < 5) {
      sample_set_tmp_sel (sample, s->view, sel);
      s->selecting = SELECTING_SELECTION_END;
      s->selection_mode = SELECTION_MODE_INTERSECT;
      sample_display_set_intersect_cursor (s);
      just_clicked = FALSE;
      goto motion;
    }
  }

  /* If shift is held down, move the closest selection edge to the mouse */

  if ((state & GDK_SHIFT_MASK) && (sample->sounddata->sels != NULL)) {
    min_xs = G_MAXINT;

    for (gl = sample->sounddata->sels; gl; gl = gl->next) {
      sel = (sw_sel *)gl->data;
      
      xss = OFFSET_TO_XPOS(sel->sel_start);
      xse = OFFSET_TO_XPOS(sel->sel_end);

      if (abs(x-xss) > min_xs) break;

      tmp_sel = sel;

      min_xs = abs(x-xss);

      s->selecting = SELECTING_SELECTION_START;

      if (abs(x-xse) > min_xs) break;

      min_xs = abs(x-xse);

      s->selecting = SELECTING_SELECTION_END;
    }

    sample_set_tmp_sel (sample, s->view, tmp_sel);
    s->selection_mode = SELECTION_MODE_INTERSECT;
    sample_display_set_intersect_cursor (s);
    just_clicked = FALSE;
    goto motion;
  }

  /* Otherwise, start a new selection region. */
	
  sample_set_tmp_sel_1(sample, s->view,
		       XPOS_TO_OFFSET(x),
		       XPOS_TO_OFFSET(x)+1);
	
  s->selecting = SELECTING_SELECTION_END;

  if(state & GDK_CONTROL_MASK) {
    s->selection_mode = SELECTION_MODE_INTERSECT;
    sample_display_set_intersect_cursor (s);
  } else if (state & GDK_MOD1_MASK) /* how to get ALT? */{
    s->selection_mode = SELECTION_MODE_SUBTRACT;
    sample_display_set_subtract_cursor (s);
  } else {
    s->selection_mode = SELECTION_MODE_REPLACE;
    SET_CURSOR(GTK_WIDGET(s), HORIZ);
  }

  just_clicked = TRUE;

 motion:

  sample_set_tmp_message (sample, _(selection_mode_names[s->selection_mode]));
  sample_set_progress_ready (sample);

  sample_display_handle_sel_motion (s, x, y, just_clicked);
}

static gint
sample_display_on_playmarker (SampleDisplay * s, gint x, gint y)
{
  gint xp = OFFSET_TO_XPOS(s->view->sample->user_offset);

  if (abs(x-xp) < 5 || ((abs(x-xp) < 15) && (y < 17)))
    return TRUE;
  
  return FALSE;
}

static gint
sample_display_on_sel (SampleDisplay * s, gint x, gint y)
{
  GList * gl;
  sw_sel * sel;
  int xss, xse;

  for (gl = s->view->sample->sounddata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    xss = OFFSET_TO_XPOS(sel->sel_start);
    xse = OFFSET_TO_XPOS(sel->sel_end);

    if(abs(x-xss) < 5 || abs(x-xse) < 5)
      return TRUE;    
  }

  return FALSE;
}

static gint
sample_display_scroll_event(GtkWidget *widget,
				 GdkEventScroll *event)
{
  SampleDisplay *s;
	
  s = SAMPLE_DISPLAY(widget);
	
  if (event->direction == GDK_SCROLL_UP) {    /* mouse wheel scroll up */
  	view_zoom_in (s->view, 2.0);
	return TRUE;
  }  else if (event->direction == GDK_SCROLL_DOWN) {   /* mouse wheel scroll down */
  	view_zoom_out (s->view, 2.0);
	return TRUE;
  }
  return FALSE; /* redundant? */
}

static gint
sample_display_button_press (GtkWidget      *widget,
			     GdkEventButton *event)
{
  SampleDisplay *s;
  GdkModifierType state;
  sw_sample * sample;
  int x, y;
  int o;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (IS_SAMPLE_DISPLAY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  s = SAMPLE_DISPLAY(widget);
  
  if(!IS_INITIALIZED(s))
    return TRUE;

  gtk_widget_grab_focus (widget);

  sample = s->view->sample;

  if (s->meta_down && s->view->current_tool == TOOL_SCRUB &&
      s->selecting == SELECTING_PLAYMARKER) {
    gdk_window_get_pointer (event->window, &x, &y, &state);
    sample_display_handle_playmarker_motion (s, x, y);
  } else 
  if(s->selecting && event->button != last_button) {
    /* Cancel the current operation if a different button is pressed. */
    sample_display_clear_sel (s);
  } else 
  if (last_tmp_view && last_tmp_view != s->view && event->button != last_button) {
    view_clear_last_tmp_view ();
  } else {
    last_button = event->button;
    gdk_window_get_pointer (event->window, &x, &y, &state);
    
    if(last_button == 1) {

      if (XPOS_TO_OFFSET(x) < 0 ||
	  XPOS_TO_OFFSET(x) > sample->sounddata->nr_frames)
	return TRUE;

      switch (s->view->current_tool) {
      case TOOL_SCRUB:
	s->selecting = SELECTING_PLAYMARKER;
	SET_CURSOR(widget, NEEDLE);
	sample_set_scrubbing (s->view->sample, TRUE);
	if (sample->play_head->going) {
	  head_set_restricted (sample->play_head, FALSE);
	  sample_refresh_playmode (sample);
	} else {
	  play_view_all (s->view);
	}
	sample_display_handle_playmarker_motion (s, x, y);
	return TRUE;
	break;
      case TOOL_SELECT:
	/* If the cursor is near a sel, move that */
	if (sample_display_on_sel (s, x, y)) {
	  sample_display_handle_sel_button_press (s, x, y, state);
	} else {
	  sample_display_handle_sel_button_press (s, x, y, state);
	}
#ifdef SEL_SCRUBS
	/* scrub along the changing selection edge, unless we're already
	 * playing. NB. the play_head->scrubbing state is used by the
	 * motion and release callbacks here to determine whether or not
	 * to scrub the moving edge, and whether or not to stop playback
	 * upon release. */
	if (sample->play_head->going) {
	  sample_set_scrubbing (sample, FALSE);
	  sample_refresh_playmode (sample);
	} else {
	  /*sample_set_monitor (sample, TRUE);*/
	  sample_set_scrubbing (sample, TRUE);
	  play_view_all (s->view);
	  sample_display_handle_playmarker_motion (s, x, y);
	}
#endif
	break;
      case TOOL_HAND:
	    s->selecting = SELECTING_HAND;
	    s->view->hand_offset = x;
	    s->hand_scroll_delta = 0;
	    if (s->hand_scroll_tag){
		   g_source_remove (s->hand_scroll_tag);
		   s->hand_scroll_tag = 0;
	    }
	    SET_CURSOR(widget, HAND_CLOSE);
	    sample_display_handle_hand_motion (s, x, y);
	break;
      case TOOL_ZOOM:
	    o = XPOS_TO_OFFSET(x);
	    view_center_on (s->view, o);
	    if (state & GDK_SHIFT_MASK) {
	      view_zoom_out (s->view, 2.0);
	    } else {
	      view_zoom_in (s->view, 2.0);
	    }
	break;
      case TOOL_PENCIL:
	s->selecting = SELECTING_PENCIL;
	sample_display_handle_pencil_motion (s, x, y);
	break;
      case TOOL_NOISE:
	s->selecting = SELECTING_NOISE;
	sample_display_handle_noise_motion (s, x, y);
	break;
      default:
	break;
      }

    } else if(last_button == 2) {
      s->selecting = SELECTING_PAN_WINDOW;
      gdk_window_get_pointer (event->window, &s->selecting_x0, NULL, NULL);
      s->selecting_wins0 = s->view->start;
      SET_CURSOR(widget, MOVE);
      sample_set_scrubbing (s->view->sample, TRUE);
    } else if(last_button == 3) {
      if(s->view && s->view->menu) {
	view_popup_context_menu (s->view, 3, event->time);
      }
    }
  }
	    
  return TRUE;
}

void
sample_display_sink_tmp_sel (SampleDisplay * s)
{
  sw_sample * sample = s->view->sample;
  sw_sel * t;

  s->selecting = SELECTING_NOTHING;

  if (s->scroll_right_tag != 0) {
    g_source_remove (s->scroll_right_tag);
    s->scroll_right_tag = 0;
  }
  if (s->scroll_left_tag != 0) {
    g_source_remove (s->scroll_left_tag);
    s->scroll_left_tag = 0;
  }

  t = sample->tmp_sel;

  if (t->sel_end == (t->sel_start + 1)) {
    if (!sample->play_head->going) {
      sample_set_playmarker (sample, t->sel_start, TRUE);
    }
    sample_clear_tmp_sel (sample);
  } else {

    if (s->selecting == SELECTING_SELECTION_START) {
      sample_set_playmarker (sample, t->sel_start, TRUE);
    } else if (s->selecting == SELECTING_SELECTION_END) {
      sample_set_playmarker (sample, t->sel_end, TRUE);
    }

    if(s->selection_mode == SELECTION_MODE_REPLACE) {
      sample_selection_replace_with_tmp_sel (sample);
    } else if (s->selection_mode == SELECTION_MODE_SUBTRACT) {
      sample_selection_subtract_tmp_sel (sample); 
    } else {
      sample_selection_insert_tmp_sel (sample);
    }
    s->selection_mode = SELECTION_MODE_NONE;

    g_signal_emit_by_name(GTK_OBJECT(s),
		    "selection-changed");
  }
}

static gint
sample_display_button_release (GtkWidget      *widget,
			       GdkEventButton *event)
{
  SampleDisplay *s;
#ifdef SEL_SCRUBS
  GdkModifierType state;
  int x, y;
#endif

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (IS_SAMPLE_DISPLAY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
    
  s = SAMPLE_DISPLAY(widget);

  switch (s->view->current_tool) {
  case TOOL_SELECT:
    
    /* If the user has released the button they were selecting with,
     * sink this sample's temporary selection.
     */
    if (s->selecting && event->button == last_button) {
      switch (s->selecting) {
      case SELECTING_SELECTION_START:
      case SELECTING_SELECTION_END:
	sample_display_sink_tmp_sel (s);
	break;
      default:
	break;
      }
    }
    
    /* If the user has released the button in a sweep window different
     * to that used for selection, then sink the appropriate temporary
     * selection.
     */
    if (last_tmp_view != s->view) {
      view_sink_last_tmp_view();
    }

#ifdef SEL_SCRUBS
    if (s->view->sample->play_head->scrubbing) {
      gdk_window_get_pointer (event->window, &x, &y, &state);
      pause_playback (s->view->sample);
      sample_display_handle_playmarker_motion (s, x, y);
    }
#endif

    break;
  case TOOL_SCRUB:
    if (s->meta_down) return TRUE;
    break;
  case TOOL_HAND:
    s->view->hand_offset = -1;

    s->hand_scroll_tag = g_timeout_add (HAND_SCROLL_INTERVAL,
		    (GSourceFunc)sample_display_hand_scroll,
		    s);

    break;
  case TOOL_MOVE:
    break;
  case TOOL_ZOOM:
    break;
  default:
    break;
  }

  if (s->meta_down && s->selecting == SELECTING_PLAYMARKER)
    return TRUE;

  s->selecting = SELECTING_NOTHING;

  sample_set_scrubbing (s->view->sample, FALSE);

  sample_display_set_default_cursor (s);
    
  return FALSE;
}

static gint
sample_display_motion_notify (GtkWidget *widget,
			      GdkEventMotion *event)
{
  SampleDisplay *s;
  gint x, y;
  GdkModifierType state;
  sw_framecount_t o;

  s = SAMPLE_DISPLAY(widget);

  if(!IS_INITIALIZED(s))
    return FALSE;

  if (event->is_hint) {
    gdk_window_get_pointer (event->window, &x, &y, &state);
  } else {
    x = event->x;
    y = event->y;
    state = event->state;
  }

  o = XPOS_TO_OFFSET(x);
  s->mouse_x = x;
  s->mouse_offset = o;
  g_signal_emit_by_name(GTK_OBJECT(s),
		  "mouse-offset-changed");

  if (s->selecting) {
    if (s->meta_down && s->selecting == SELECTING_PLAYMARKER) {
      sample_display_handle_playmarker_motion (s, x, y);
    } else if((state & GDK_BUTTON1_MASK) && last_button == 1) {
      switch (s->selecting) {
      case SELECTING_PLAYMARKER:
	sample_display_handle_playmarker_motion (s, x, y);
	break;
      case SELECTING_HAND:
	    sample_display_handle_hand_motion (s, x, y);
	    break;
      case SELECTING_PENCIL:
	sample_display_handle_pencil_motion (s, x, y);
	break;
      case SELECTING_NOISE:
	sample_display_handle_noise_motion (s, x, y);
	break;
      default:
	sample_display_handle_sel_motion(s, x, y, 0);
#ifdef SEL_SCRUBS
	if (s->view->sample->play_head->scrubbing) {
	  sample_display_handle_playmarker_motion (s, x, y);
	}
#endif
	break;
      }
    } else if((state & GDK_BUTTON2_MASK) && last_button == 2) {
      sample_display_handle_move_motion (s, x, y);
    } else {
      /*    sample_display_clear_sel (s);*/
      if (s->selecting == SELECTING_SELECTION_START ||
	  s->selecting == SELECTING_SELECTION_END) {
	/* XXX: Need to sink_tmp_sel here instead for consistency ??? 
	 *  It seems to be clearing fast tmp_sels now, but at least not
	 * leaving them lying around.*/
	sample_display_sink_tmp_sel (s);

	sample_display_set_default_cursor (s);
      }
    }

  } else {
    if (s->view->current_tool == TOOL_SELECT &&
	sample_display_on_sel (s, x, y)) {
      SET_CURSOR(widget, HORIZ);
    } else if (s->view->current_tool == TOOL_SELECT &&
	       sample_display_on_playmarker (s, x, y)) {
      SET_CURSOR(widget, NEEDLE);
    } else {

      if (o > 0 && o < s->view->sample->sounddata->nr_frames)
	sample_display_set_default_cursor (SAMPLE_DISPLAY(widget));
      else
	gdk_window_set_cursor (widget->window, NULL);
    }
  }
    
  return FALSE;
}

static gint
sample_display_enter_notify (GtkWidget *widget,
			     GdkEventCrossing *event)
{
  gtk_widget_grab_focus (widget);
  return TRUE;
}

static gint
sample_display_leave_notify (GtkWidget *widget,
			     GdkEventCrossing *event)
{
  SampleDisplay *s;
 
  s = SAMPLE_DISPLAY(widget);
  s->mouse_offset = -1;
  g_signal_emit_by_name(GTK_OBJECT(s),
		 "mouse-offset-changed");

  return TRUE;
}

static gint
sample_display_focus_in (GtkWidget * widget, GdkEventFocus * event)
{
  SampleDisplay * s;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (IS_SAMPLE_DISPLAY(widget), FALSE);

  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
 /*
  * FIXME: nonexistant in GTK+-2.0
  * docs say draw in the expose function. draw what though?
  * 	
  *	gtk_widget_draw_focus (widget);
  */

  s = SAMPLE_DISPLAY(widget);

  if (s->marching) {
    sd_start_marching_ants_timeout (s);
  }

  sample_display_start_cursor_pulse (s);

  undo_dialog_set_sample (s->view->sample);

  return FALSE;
}

static gint
sample_display_focus_out (GtkWidget * widget, GdkEventFocus * event)
{
  SampleDisplay * s;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (IS_SAMPLE_DISPLAY(widget), FALSE);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
 /*
  * FIXME: nonexistant in GTK+-2.0
  * docs say draw in the expose function. draw what though?
  * 	
  *	gtk_widget_draw_focus (widget);
  */

  s = SAMPLE_DISPLAY(widget);

  sd_stop_marching_ants_timeout (s);

  sample_display_stop_cursor_pulse (s);

  return FALSE;  
}

static gint
sample_display_key_press (GtkWidget * widget, GdkEventKey * event)
{
  SampleDisplay * s = SAMPLE_DISPLAY(widget);
  sw_view * view = s->view;
  sw_sample * sample;
  sw_framecount_t vlen, move_delta = 0, sel_t;
  GList * gl;
  sw_sel * sel;
  int x, y, xss, xse;

  sample = view->sample;
  vlen = view->end - view->start;

  /*g_print ("key 0x%X pressed\n", event->keyval);*/

  switch (event->keyval) {
  case GDK_Meta_L:
  case GDK_Super_L:
  case GDK_Multi_key:
    if (sample->edit_mode == SWEEP_EDIT_MODE_ALLOC) break;

    s->meta_down = TRUE;

    if (s->selecting == SELECTING_NOTHING) {
      s->selecting = SELECTING_PLAYMARKER;
      SET_CURSOR(widget, NEEDLE);
      sample_set_scrubbing (sample, TRUE);
    }

    if (s->selecting == SELECTING_PLAYMARKER) {
      gdk_window_get_pointer (widget->window, &x, &y, NULL);
      sample_display_handle_playmarker_motion (s, x, y);
      if (!sample->play_head->going) {
	play_view_all (s->view);
      }
    }

    return TRUE;
    break;
  case GDK_Menu:
    if(view->menu) {
      gtk_menu_popup(GTK_MENU(view->menu),
		     NULL, NULL, NULL,
		     NULL, 3, event->time);
    }
    return TRUE;
    break;
  case GDK_BackSpace:
    if (sample->edit_mode == SWEEP_EDIT_MODE_READY)
      do_clear (sample);
    return TRUE;
    break;
  case GDK_Delete:
    if (sample->edit_mode == SWEEP_EDIT_MODE_READY)
      do_delete (sample);
    return TRUE;
    break;
  case GDK_less:
    if (sample->edit_mode == SWEEP_EDIT_MODE_READY)
      select_shift_left_cb (widget, s);
    return TRUE;
  case GDK_greater:
    if (sample->edit_mode == SWEEP_EDIT_MODE_READY)
      select_shift_right_cb (widget, s);
    return TRUE;
  case GDK_Up:
  case GDK_KP_Up:
    if (event->state & GDK_CONTROL_MASK) {
      zoom_1to1_cb (GTK_WIDGET(s), s);
    } else if (event->state & GDK_SHIFT_MASK) {
      view_vzoom_in (view, 1.2);
    } else {
      view_zoom_in (view, 2.0);
    }
    return TRUE;
    break;
  case GDK_Down:
  case GDK_KP_Down:
    if (event->state & GDK_CONTROL_MASK) {
      zoom_norm_cb (GTK_WIDGET(s), s);
    } else if (event->state & GDK_SHIFT_MASK) {
      view_vzoom_out (view, 1.2);
    } else {
      view_zoom_out (view, 2.0);
    }
    return TRUE;
    break;
  case GDK_Left:
  case GDK_KP_Left:
    move_delta = MIN(-1,  -vlen/s->width);
    if (event->state & GDK_CONTROL_MASK) {
      if (!(event->state & GDK_SHIFT_MASK)) {
	sample_set_offset_next_bound_left (sample);
	return TRUE;
      }
      move_delta *= 10;
    }
    break;
  case GDK_Right:
  case GDK_KP_Right:
    move_delta = MAX(1,  vlen/s->width);
    if (event->state & GDK_CONTROL_MASK) {
      if (!(event->state & GDK_SHIFT_MASK)) {
	sample_set_offset_next_bound_right (sample);
	return TRUE;
      }
      move_delta *= 10;
    }
    break;
  default: /* Random other key pressed */
    return FALSE;
    break;
  }

  /* Handle movement only from here on */

  if ((event->state & GDK_SHIFT_MASK) &&
      (sample->edit_mode == SWEEP_EDIT_MODE_READY)) {
    sel = sample->tmp_sel;
    
    switch (s->selecting) {
    case SELECTING_SELECTION_START:
      sel->sel_start += move_delta;
      if (sel->sel_start > sel->sel_end) {
	sel_t = sel->sel_start;
	sel->sel_start = sel->sel_end;
	sel->sel_end = sel_t;
	s->selecting = SELECTING_SELECTION_END;
      }
      break;
    case SELECTING_SELECTION_END:
      sample->tmp_sel->sel_end += move_delta;
      if (sel->sel_start > sel->sel_end) {
	sel_t = sel->sel_start;
	sel->sel_start = sel->sel_end;
	sel->sel_end = sel_t;
	s->selecting = SELECTING_SELECTION_START;
      }
      break;
    default:
      last_button = 0;

      x = OFFSET_TO_XPOS(sample->user_offset);

      if ((sel = sample->tmp_sel) != NULL) {

	xss = OFFSET_TO_XPOS(sel->sel_start);
	xse = OFFSET_TO_XPOS(sel->sel_end);
	  
	if(abs(x-xss) < 5) {
	  s->selecting = SELECTING_SELECTION_START;
	  sel->sel_start += move_delta;
	  if (sel->sel_start > sel->sel_end) {
	    sel_t = sel->sel_start;
	    sel->sel_start = sel->sel_end;
	    sel->sel_end = sel_t;
	    s->selecting = SELECTING_SELECTION_END;
	  }
	  break;
	} else if(abs(x-xse) < 5) {
	  s->selecting = SELECTING_SELECTION_END;
	  sel->sel_end += move_delta;
	  if (sel->sel_start > sel->sel_end) {
	    sel_t = sel->sel_start;
	    sel->sel_start = sel->sel_end;
	    sel->sel_end = sel_t;
	    s->selecting = SELECTING_SELECTION_START;
	  }
	  break;
	}
      }

      if (s->selecting != SELECTING_SELECTION_START &&
	  s->selecting != SELECTING_SELECTION_END) {
	
	for (gl = sample->sounddata->sels; gl; gl = gl->next) {
	  
	  
	  /* If the cursor is near the current start or end of
	   * the selection, move that.
	   */
	  
	  sel = (sw_sel *)gl->data;
	  
	  xss = OFFSET_TO_XPOS(sel->sel_start);
	  xse = OFFSET_TO_XPOS(sel->sel_end);
	  
	  if(abs(x-xss) < 5) {
	    sample_set_tmp_sel (sample, s->view, sel);
	    s->selecting = SELECTING_SELECTION_START;
	    s->selection_mode = SELECTION_MODE_INTERSECT;
	    sample_display_set_intersect_cursor (s);
	    break;
	  } else if(abs(x-xse) < 5) {
	    sample_set_tmp_sel (sample, s->view, sel);
	    s->selecting = SELECTING_SELECTION_END;
	    s->selection_mode = SELECTION_MODE_INTERSECT;
	    sample_display_set_intersect_cursor (s);
	    break;
	  }
	}
      }
      
      if (s->selecting != SELECTING_SELECTION_START &&
	  s->selecting != SELECTING_SELECTION_END) {

	sample_set_tmp_sel_1 (sample, view, sample->user_offset,
			      sample->user_offset + move_delta);
	s->selecting = SELECTING_SELECTION_START;
	s->selection_mode = SELECTION_MODE_REPLACE;
	SET_CURSOR (GTK_WIDGET(s), HORIZ);
      }
      break;
    } 
    g_signal_emit_by_name(GTK_OBJECT(s),
		    "selection-changed");
  } else if (s->selecting == SELECTING_SELECTION_START ||
	     s->selecting == SELECTING_SELECTION_END) {
    sample_display_sink_tmp_sel (s);
    s->selecting = SELECTING_NOTHING;
    sample_display_set_default_cursor (s);
  }

  sample_set_playmarker (sample, sample->user_offset + move_delta, TRUE);

  return TRUE;
}

static gint
sample_display_key_release (GtkWidget * widget, GdkEventKey * event)
{
  SampleDisplay * s = SAMPLE_DISPLAY(widget);
  GdkModifierType state;

  switch (event->keyval) {
  case GDK_Meta_L:
  case GDK_Super_L:
  case GDK_Multi_key:
    s->meta_down = FALSE;

    gdk_window_get_pointer (widget->window, NULL, NULL, &state);

    /* Don't cancel scrubbing if the mouse is down for it */
    if ((state & GDK_BUTTON1_MASK) && s->view->current_tool == TOOL_SCRUB)
      return TRUE;

    if (s->selecting == SELECTING_PLAYMARKER) {
      s->selecting = SELECTING_NOTHING;
      sample_set_scrubbing (s->view->sample, FALSE);
      sample_display_set_default_cursor (s);
    }

    return TRUE;
    break;
  default:
    break;
  }

  return FALSE;
}

static gint
sample_display_destroy (GtkWidget * widget, GdkEventAny * event)
{
  gtk_widget_queue_draw(widget);
  return 0;
}

static void
sample_display_class_init (SampleDisplayClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  int n;
  const int *p;
  GdkColor *c;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  widget_class->realize = sample_display_realize;
  widget_class->size_allocate = sample_display_size_allocate;
  widget_class->expose_event = sample_display_expose;
  widget_class->size_request = sample_display_size_request;
  widget_class->button_press_event = sample_display_button_press;
  widget_class->button_release_event = sample_display_button_release;
  widget_class->scroll_event = sample_display_scroll_event;
  widget_class->motion_notify_event = sample_display_motion_notify;
  widget_class->enter_notify_event = sample_display_enter_notify;
  widget_class->leave_notify_event = sample_display_leave_notify;
  widget_class->key_press_event = sample_display_key_press;
  widget_class->key_release_event = sample_display_key_release;
  widget_class->focus_in_event = sample_display_focus_in;
  widget_class->focus_out_event = sample_display_focus_out;

  widget_class->destroy_event = sample_display_destroy;

  sample_display_signals[SIG_SELECTION_CHANGED] =
    g_signal_new ("selection-changed",
					 			  G_TYPE_FROM_CLASS (object_class),
	                              G_SIGNAL_RUN_FIRST,
	                              G_STRUCT_OFFSET (SampleDisplayClass, selection_changed),
                                  NULL, 
                                  NULL,                
					 			  g_cclosure_marshal_VOID__VOID,
                                  G_TYPE_NONE, 0);
  

  sample_display_signals[SIG_WINDOW_CHANGED] =
    g_signal_new ("window-changed",
					 			  G_TYPE_FROM_CLASS (object_class),
	                              G_SIGNAL_RUN_FIRST,
	                              G_STRUCT_OFFSET (SampleDisplayClass, window_changed),
                                  NULL, 
                                  NULL,                
					 			  g_cclosure_marshal_VOID__VOID,
                                  G_TYPE_NONE, 0);
  

  sample_display_signals[SIG_MOUSE_OFFSET_CHANGED] =
    g_signal_new ("mouse-offset-changed",
					 			  G_TYPE_FROM_CLASS (object_class),
	                              G_SIGNAL_RUN_FIRST,
	                              G_STRUCT_OFFSET (SampleDisplayClass, mouse_offset_changed),
                                  NULL, 
                                  NULL,                
					 			  g_cclosure_marshal_VOID__VOID,
                                  G_TYPE_NONE, 0);

  class->selection_changed = NULL;
  class->window_changed = NULL;
  class->mouse_offset_changed = NULL;

  for(n = 0, p = default_colors, c = class->colors;
      n < SAMPLE_DISPLAYCOL_LAST; n++, c++) {
    c->red = *p++ * 65535 / 255;
    c->green = *p++ * 65535 / 255;
    c->blue = *p++ * 65535 / 255;
    c->pixel = (glong)((c->red & 0xff00)*256 +
			(c->green & 0xff00) +
			(c->blue & 0xff00)/256);
    gdk_colormap_alloc_color(gdk_colormap_get_system(), c, TRUE, TRUE);
  }

  for(n = 0, p = bg_colors, c = class->bg_colors;
      n < VIEW_COLOR_MAX; n++, c++) {
    c->red = *p++ * 65535 / 255;
    c->green = *p++ * 65535 / 255;
    c->blue = *p++ * 65535 / 255;
    c->pixel = (glong)((c->red & 0xff00)*256 +
			(c->green & 0xff00) +
			(c->blue & 0xff00)/256);
    gdk_colormap_alloc_color(gdk_colormap_get_system(), c, TRUE, TRUE);
  }

  for(n = 0, p = fg_colors, c = class->fg_colors;
      n < VIEW_COLOR_MAX; n++, c++) {
    c->red = *p++ * 65535 / 255;
    c->green = *p++ * 65535 / 255;
    c->blue = *p++ * 65535 / 255;
    c->pixel = (glong)((c->red & 0xff00)*256 +
			(c->green & 0xff00) +
			(c->blue & 0xff00)/256);
    gdk_colormap_alloc_color(gdk_colormap_get_system(), c, TRUE, TRUE);
  }
}

static void
sample_display_init (SampleDisplay *s)
{
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET(s), GTK_CAN_FOCUS);

  s->backing_pixmap = NULL;
  s->view = NULL;
  s->selecting = SELECTING_NOTHING;
  s->selection_mode = SELECTION_MODE_NONE;
  s->marching_tag = 0;
  s->marching = FALSE;
  s->pulsing_tag = 0;
  s->pulse = FALSE;
  s->hand_scroll_tag = 0;
  s->mouse_x = 0;
  s->mouse_offset = 0;
  s->scroll_left_tag = 0;
  s->scroll_right_tag = 0;
}




GType
sample_display_get_type (void)
{
  static GType sample_display_type = 0;

  if (!sample_display_type) 
	{
      static const GTypeInfo sample_display_info =
    {
      sizeof(SampleDisplayClass),
	   NULL, /* base_init */
	   NULL, /* base_finalize */
	   (GClassInitFunc) sample_display_class_init,
	   NULL, /* class_finalize */
	   NULL, /* class_data */
       sizeof (SampleDisplay),		  
	   0,    /* n_preallocs */
	   (GInstanceInitFunc) sample_display_init,
		  
    };

		sample_display_type = g_type_register_static (GTK_TYPE_WIDGET,
												  "SampleDisplay", &sample_display_info, 0);
		
  }

  return sample_display_type;
}

GtkWidget*
sample_display_new (void)
{
  return GTK_WIDGET (g_object_new (sample_display_get_type (), NULL));
}
