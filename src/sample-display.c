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

#include <stdlib.h>

#include "sample-display.h"

#include <gtk/gtk.h>

#include "sweep_app.h"

#include "cursors.h"

/* static cursor definitions */
#include "horiz.xpm"
#include "horiz_plus.xpm"


gint current_tool = TOOL_SELECT;

/* Maximum number of samples to consider per pixel */
#define STEP_MAX 32


/* Whether or not to compile in support for
 * drawing the crossing vectors
 */
/* #define DRAW_CROSSING_VECTORS */

#define PIXEL_TO_OFFSET(p) \
  ((sw_framecount_t)(((gdouble)(p)) * (((gdouble)(s->view->end - s->view->start)) / ((gdouble)s->width))))

#define XPOS_TO_OFFSET(x) \
  (s->view->start + PIXEL_TO_OFFSET(x))

#define SAMPLE_TO_PIXEL(n) \
  ((sw_framecount_t)((gdouble)(n) * (gdouble)s->width / (gdouble)(s->view->end - s->view->start)))

#define OFFSET_TO_XPOS(o) \
  SAMPLE_TO_PIXEL((o) - s->view->start)

#define OFFSET_RANGE(l, x) ((x) < 0 ? 0 : ((x) >= (l) ? (l) - 1 : (x)))

#define SET_CURSOR(w, c) \
  gdk_window_set_cursor (##w##->window, SAMPLE_DISPLAY_CLASS(GTK_OBJECT(##w##)->klass)->##c##)

#define MARCH_INTERVAL 300

extern sw_view * last_tmp_view;

static int last_button; /* button index which started the last selection;
			 * This is global to allow comparison for
			 * last_tmp_view
			 */

#ifdef _MUSHROOM_BROWN
static const int default_colors[] = {
  114, 114, 98,  /* bg */
  234, 234, 25,  /* fg */
  230, 0, 0,     /* mixerpos */
  40, 40, 0,     /* zero */
  240, 230, 240, /* sel */
  255, 255, 255, /* tmp_sel */
  10, 40, 10,    /* crossing */
  170, 169, 134, /* minmax */
  224, 220, 172, /* highlight */
  43, 42, 33,    /* lowlight */
};
#else
static const int default_colors[] = {
  182, 178, 182,  /* bg */
  199, 203, 158,  /* fg */
  230, 0, 0,     /* mixerpos */
  40, 40, 0,     /* zero */
  240, 230, 240, /* sel */
  255, 255, 255, /* tmp_sel */
  10, 40, 10,    /* crossing */
  174, 186, 174, /* minmax */
  215, 219, 215, /* highlight */
  81, 101, 81,   /* lowlight */
};
#endif


/* Values for s->selecting */
enum {
  SELECTING_NOTHING = 0,
  SELECTING_SELECTION_START,
  SELECTING_SELECTION_END,
  SELECTING_PAN_WINDOW,
};

enum {
  SELECTION_MODE_NONE = 0, /* Not selecting; used for consistency check. */
  SELECTION_MODE_REPLACE,
  SELECTION_MODE_INTERSECT,
};


enum {
  SIG_SELECTION_CHANGED,
  SIG_WINDOW_CHANGED,
  SIG_MOUSE_OFFSET_CHANGED,
  LAST_SIGNAL
};

#define IS_INITIALIZED(s) (s->view != NULL)

static guint sample_display_signals[LAST_SIGNAL] = { 0 };

static gchar sel_dash_list[2] = { 4, 4 }; /* Equivalent to GDK's default
					  *  dash list.
					  */

void
sample_display_refresh (SampleDisplay *s)
{
  gtk_widget_queue_draw(GTK_WIDGET(s));
}

void
sample_display_set_view (SampleDisplay *s, sw_view *view)
{
  g_return_if_fail(s != NULL);
  g_return_if_fail(IS_SAMPLE_DISPLAY(s));

  s->view = view;
  s->old_mixerpos = -1;
  s->mixerpos = -1;
	
  /*  gtk_signal_emit(GTK_OBJECT(s), sample_display_signals[SIG_WINDOW_CHANGED], s->view->start, s->view->start + (s->view->end - s->view->start));*/
	
#if 0
  s->old_ss = s->old_se = -1;
#endif
  s->selecting = SELECTING_NOTHING;
  s->selection_mode = SELECTION_MODE_NONE;

  gtk_widget_queue_draw(GTK_WIDGET(s));
}

void
sample_display_set_playmarker (SampleDisplay *s,
			       int offset)
{
  g_return_if_fail(s != NULL);
  g_return_if_fail(IS_SAMPLE_DISPLAY(s));

  if(!IS_INITIALIZED(s))
    return;

  if(offset != s->mixerpos) {
    s->mixerpos = offset;
    gtk_widget_queue_draw(GTK_WIDGET(s));
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

  len = s->view->sample->soundfile->nr_frames;
  vlen = end - start;

  g_return_if_fail(end > start);

  /* Align to middle if entire length of sample is visible */
  if (vlen > len) {
    start = (len - vlen) / 2;
    end = start + vlen;
  }

  s->view->start = start;
  s->view->end = end;

  gtk_signal_emit(GTK_OBJECT(s), sample_display_signals[SIG_WINDOW_CHANGED]);

  s->mouse_offset = XPOS_TO_OFFSET (s->mouse_x);
  gtk_signal_emit(GTK_OBJECT(s),
		  sample_display_signals[SIG_MOUSE_OFFSET_CHANGED]);

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

    len = s->view->sample->soundfile->nr_frames;
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

    gtk_signal_emit(GTK_OBJECT(s), sample_display_signals[SIG_WINDOW_CHANGED]);
  }

 stretch:
  
  s->width = w;
  s->height = h;

  if(s->backing_pixmap) {
    gdk_pixmap_unref(s->backing_pixmap);
  }
  window = GTK_WIDGET(s)->window;
  visual = gdk_window_get_visual(window);

  s->backing_pixmap = gdk_pixmap_new (GTK_WIDGET(s)->window,  
                                      w, h, visual->depth);
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
  attributes.event_mask = gtk_widget_get_events (widget)
    | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
    | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK
    | GDK_LEAVE_NOTIFY_MASK;

  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);

  widget->style = gtk_style_attach (widget->style, widget->window);

  s->width = 0;

  s->bg_gc = gdk_gc_new(widget->window);
  gdk_gc_set_foreground(s->bg_gc, &SAMPLE_DISPLAY_CLASS(GTK_OBJECT(widget)->klass)->colors[SAMPLE_DISPLAYCOL_BG]);

  s->fg_gc = gdk_gc_new(widget->window);
  gdk_gc_set_foreground(s->fg_gc, &SAMPLE_DISPLAY_CLASS(GTK_OBJECT(widget)->klass)->colors[SAMPLE_DISPLAYCOL_FG]);

  s->zeroline_gc = gdk_gc_new(widget->window);
  gdk_gc_set_foreground(s->zeroline_gc, &SAMPLE_DISPLAY_CLASS(GTK_OBJECT(widget)->klass)->colors[SAMPLE_DISPLAYCOL_ZERO]);

  s->mixerpos_gc = gdk_gc_new(widget->window);
  gdk_gc_set_foreground(s->mixerpos_gc, &SAMPLE_DISPLAY_CLASS(GTK_OBJECT(widget)->klass)->colors[SAMPLE_DISPLAYCOL_MIXERPOS]);

  s->sel_gc = gdk_gc_new(widget->window);
  gdk_gc_set_foreground(s->sel_gc, &SAMPLE_DISPLAY_CLASS(GTK_OBJECT(widget)->klass)->colors[SAMPLE_DISPLAYCOL_SEL]);
  gdk_gc_set_line_attributes(s->sel_gc, 1 /* line width */,
			     GDK_LINE_DOUBLE_DASH,
			     GDK_CAP_BUTT,
			     GDK_JOIN_MITER);

  s->tmp_sel_gc = gdk_gc_new(widget->window);
  gdk_gc_set_foreground(s->tmp_sel_gc, &SAMPLE_DISPLAY_CLASS(GTK_OBJECT(widget)->klass)->colors[SAMPLE_DISPLAYCOL_TMP_SEL]);
  gdk_gc_set_function (s->tmp_sel_gc, GDK_XOR);

  s->crossing_gc = gdk_gc_new(widget->window);
  gdk_gc_set_foreground(s->crossing_gc, &SAMPLE_DISPLAY_CLASS(GTK_OBJECT(widget)->klass)->colors[SAMPLE_DISPLAYCOL_CROSSING]);

  s->minmax_gc = gdk_gc_new (widget->window);
  gdk_gc_set_foreground(s->minmax_gc, &SAMPLE_DISPLAY_CLASS(GTK_OBJECT(widget)->klass)->colors[SAMPLE_DISPLAYCOL_MINMAX]);

  s->highlight_gc = gdk_gc_new (widget->window);
  gdk_gc_set_foreground(s->highlight_gc, &SAMPLE_DISPLAY_CLASS(GTK_OBJECT(widget)->klass)->colors[SAMPLE_DISPLAYCOL_HIGHLIGHT]);

  s->lowlight_gc = gdk_gc_new (widget->window);
  gdk_gc_set_foreground(s->lowlight_gc, &SAMPLE_DISPLAY_CLASS(GTK_OBJECT(widget)->klass)->colors[SAMPLE_DISPLAYCOL_LOWLIGHT]);


  sample_display_init_display(s, attributes.width, attributes.height);

  SET_CURSOR(widget, crosshair_cr);

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
  GdkGC *gc;
  sw_audio_intermediate_t totpos, totneg;
  sw_audio_t d, maxpos, avgpos, minneg, avgneg;
  sw_audio_t prev_maxpos, prev_minneg;
  sw_framecount_t i, step, nr_pos, nr_neg;
  sw_sample * sample;
  const int channels = s->view->sample->soundfile->format->channels;

  sample = s->view->sample;

  gdk_draw_rectangle(win, s->bg_gc,
		     TRUE, x, y, width, height);

  gdk_draw_line(win, s->zeroline_gc,
		x, y + height/2,
		x + width - 1, y + height/2);

  totpos = totneg = 0.0;
  maxpos = minneg = prev_maxpos = prev_minneg = 0.0;

  while(width >= 0) {
    nr_pos = nr_neg = 0;
    totpos = totneg = 0;
    
    maxpos = minneg = 0;

    /* 'step' ensures that no more than STEP_MAX values get looked at
     * per pixel */
    step = MAX (1, PIXEL_TO_OFFSET(1)/STEP_MAX);

    for (i = OFFSET_RANGE(sample->soundfile->nr_frames, XPOS_TO_OFFSET(x));
	 i < OFFSET_RANGE(sample->soundfile->nr_frames, XPOS_TO_OFFSET(x+1));
	 i+=step) {
      d = ((sw_audio_t *)sample->soundfile->data)[i*channels + channel];
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

#define YPOS(v) (y + ((((v) - SW_AUDIO_T_MIN) * height) \
		       / (SW_AUDIO_T_MAX - SW_AUDIO_T_MIN)))

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
	      

    gdk_draw_line(win, s->fg_gc,
		  x, YPOS(avgpos),
		  x, YPOS(avgneg));

    prev_maxpos = maxpos;
    prev_minneg = minneg;

    x++;
    width--;
  }

}

static void
sample_display_draw_data (GdkDrawable *win,
			  const SampleDisplay *s,
			  int x,
			  int width)
{
  const int sh = s->height;
  int start_x, end_x;
  const int channels = s->view->sample->soundfile->format->channels;

  if(width == 0)
    return;

  g_return_if_fail(x >= 0);
  g_return_if_fail(x + width <= s->width);

#if 0
  g_print("draw_data: view %u --> %u, drawing x=%d, width=%d\n",
	  s->view->start, s->view->end, x, width);
#endif

  start_x = OFFSET_TO_XPOS(0);
  end_x = OFFSET_TO_XPOS(s->view->sample->soundfile->nr_frames);

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

  if (channels == 1) {
    sample_display_draw_data_channel (win, s, x, 0, width, sh, 0);
  } else if (channels == 2) {
    sample_display_draw_data_channel (win, s,
				      x, 0,
				      width, (sh/2)-1, 0);
    sample_display_draw_data_channel (win, s,
				      x, sh-((sh/2)-1),
				      width, (sh/2)-1, 1);

    gtk_style_apply_default_background (GTK_WIDGET(s)->style, win,
					TRUE, GTK_STATE_NORMAL,
					NULL,
					x, (sh/2)-1,
					width, 2);
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
  cx2 = ( x < sample->soundfile->nr_frames - VRAD ? VRAD : 0 );

  cy1 = ((sw_audio_t *)sample->soundfile->data)[OFFSET_RANGE(sample->soundfile->nr_frames, XPOS_TO_OFFSET(x)) - cx1];
  cy2 = ((sw_audio_t *)sample->soundfile->data)[OFFSET_RANGE(sample->soundfile->nr_frames, XPOS_TO_OFFSET(x)) + cx2];
  
  gdk_draw_line(s->backing_pixmap, s->crossing_gc,
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

  gtk_widget_queue_draw (GTK_WIDGET(s));

  return TRUE;
}

void
sample_display_start_marching_ants (SampleDisplay * s)
{
  s->marching_tag = gtk_timeout_add (MARCH_INTERVAL,
				     (GtkFunction)sd_march_ants,
				     s);
}

void
sample_display_stop_marching_ants (SampleDisplay * s)
{
  if (s->marching_tag > 0)
    gtk_timeout_remove (s->marching_tag);

  s->marching_tag = 0;
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
  for (gl = sample->soundfile->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    x = OFFSET_TO_XPOS(sel->sel_start);
    l_end = (x >= x_min) && (x <= x_max);
    
    x2 = OFFSET_TO_XPOS(sel->sel_end);
    r_end = (x2 >= x_min) && (x2 <= x_max);
    
    /* draw the selection */
    sample_display_draw_sel_box(s->backing_pixmap, s->sel_gc,
				s, x, x2 - x - 1,
				l_end, r_end /* draw_ends */);


  }

  /* Draw temporary selection */
  sel = sample->tmp_sel;

  if (sel) {
    x = OFFSET_TO_XPOS(sel->sel_start);
    l_end = (x >= x_min) && (x <= x_max);
  
    x2 = OFFSET_TO_XPOS(sel->sel_end);
    r_end = (x2 >= x_min) && (x2 <= x_max);
  
    /* draw the selection */
    sample_display_draw_sel_box(s->backing_pixmap, s->tmp_sel_gc,
				s, x, x2 - x - 1,
				l_end, r_end /* draw_ends */);
  }
}


/*** PLAY MARKER ***/

#if 0
static int
sample_display_startoffset_to_xpos (SampleDisplay *s,
				    int offset)
{
  int d = offset - s->view->start;

  if(d < 0)
    return 0;
  if(d >= (s->view->end - s->view->start))
    return s->width;

  return d * s->width / (s->view->end - s->view->start);
}


static int
sample_display_endoffset_to_xpos (SampleDisplay *s,
				  int offset)
{
  if((s->view->end - s->view->start) < s->width) {
    return sample_display_startoffset_to_xpos(s, offset);
  } else {
    int d = offset - s->view->start;
    int l = (1 - (s->view->end - s->view->start)) / s->width;

    /* you get these tests by setting the complete formula below
     * equal to 0 or s->width, respectively, and then resolving
     * towards d.
     */
    if(d < l)
      return 0;
    if(d > (s->view->end - s->view->start) + l)
      return s->width;

    return (d * s->width + (s->view->end - s->view->start) - 1) / (s->view->end - s->view->start);
  }
}
#endif /* _{start,end}offset_to_xpos */

static void
sample_display_do_marker_line (GdkDrawable *win,
			       GdkGC *gc,
			       SampleDisplay *s,
			       int endoffset,
			       int offset,
			       int x_min,
			       int x_max)
{
  int x;

  if(offset >= s->view->start && offset <= s->view->end) {
#if 0
    if(!endoffset)
      x = sample_display_startoffset_to_xpos(s, offset);
    else
      x = sample_display_endoffset_to_xpos(s, offset);
#endif
    x = OFFSET_TO_XPOS (offset);

    if(x >= x_min && x < x_max) {
      gdk_draw_line(win, gc,
		    x, 0, 
		    x, s->height);
    }
  }
}


/*** DRAW_MAIN ***/

static void
sample_display_draw_main (GtkWidget *widget,
			  GdkRectangle *area)
{
  SampleDisplay *s = SAMPLE_DISPLAY(widget);

  g_return_if_fail(area->x >= 0);

  if(area->width == 0)
    return;

  if(area->x + area->width > s->width)
    return;

  if(!IS_INITIALIZED(s)) {
    gtk_style_apply_default_background (GTK_WIDGET(s)->style, widget->window,
					TRUE, GTK_STATE_NORMAL,
					NULL,
					area->x, area->y,
					area->width, area->height);
  } else {
    const int x_min = area->x;
    const int x_max = area->x + area->width;

    /* draw the sample graph */
    sample_display_draw_data(s->backing_pixmap, s, x_min, x_max - x_min);

    sample_display_draw_sel (s->backing_pixmap, s, x_min, x_max);
      
    if(s->mixerpos != -1) {
      sample_display_do_marker_line(s->backing_pixmap, s->mixerpos_gc,
				    s,
				    0, s->mixerpos, x_min, x_max);

    }

    gdk_draw_pixmap(widget->window, s->fg_gc, s->backing_pixmap,
                    area->x, area->y,
                    area->x, area->y,
                    area->width, area->height);

  }
}

static void
sample_display_draw (GtkWidget *widget,
		     GdkRectangle *area)
{
#if 0
  SampleDisplay *s = SAMPLE_DISPLAY(widget);
  GdkRectangle area2 = { 0, 0, 0, s->height };
  int i, x;
  const int x_min = area->x;
  const int x_max = area->x + area->width;
#endif

  sample_display_draw_main(widget, area);
  return;

#if 0
  if(s->complete_redraw) {
    s->complete_redraw = 0;
    sample_display_draw_main(widget, area);
    return;
  }

  if(s->view->sample->sel_start != s->old_ss || s->view->sample->sel_end != s->old_se) {
    if(s->view->sample->sel_start == -1 || s->old_ss == -1) {
      sample_display_draw_main(widget, area);
    } else {
      if(s->view->sample->sel_start < s->old_ss) {
	/* repaint left additional side */
	x = sample_display_startoffset_to_xpos(s, s->view->sample->sel_start);
	area2.x = MIN(x_max, MAX(x_min, x));
	x = sample_display_startoffset_to_xpos(s, s->old_ss);
	area2.width = MIN(x_max, MAX(x_min, x+2)) - area2.x;
      } else {
	/* repaint left removed side */
	x = sample_display_startoffset_to_xpos(s, s->old_ss);
	area2.x = MIN(x_max, MAX(x_min, x));
	x = sample_display_startoffset_to_xpos(s, s->view->sample->sel_start);
	area2.width = MIN(x_max, MAX(x_min, x)) - area2.x;
      }
      sample_display_draw_main(widget, &area2);

      if(s->view->sample->sel_end < s->old_se) {
	/* repaint right removed side */
	x = sample_display_endoffset_to_xpos(s, s->view->sample->sel_end);
	area2.x = MIN(x_max, MAX(x_min, x));
	x = sample_display_endoffset_to_xpos(s, s->old_se);
	area2.width = MIN(x_max, MAX(x_min, x+1)) - area2.x;
      } else {
	/* repaint right additional side */
	x = sample_display_endoffset_to_xpos(s, s->old_se);
	area2.x = MIN(x_max, MAX(x_min, x-2));
	x = sample_display_endoffset_to_xpos(s, s->view->sample->sel_end);
	area2.width = MIN(x_max, MAX(x_min, x)) - area2.x;
      }
      sample_display_draw_main(widget, &area2);
    }

    s->old_ss = s->view->sample->sel_start;
    s->old_se = s->view->sample->sel_end;
  }

  if(s->mixerpos != s->old_mixerpos) {
    for(i = 0; i < 2; i++) {
      if(s->old_mixerpos >= s->view->start &&
	 s->old_mixerpos < s->view->start + (s->view->end - s->view->start)) {
	x = sample_display_startoffset_to_xpos(s, s->old_mixerpos);
	area2.x = MIN(x_max - 1, MAX(x_min, x - 3));
	area2.width = 7;
	sample_display_draw_main(widget, &area2);
      }
      s->old_mixerpos = s->mixerpos;
    }
  }
#endif

}


/*** EVENT HANDLERS ***/

static gint
sample_display_expose (GtkWidget *widget,
		       GdkEventExpose *event)
{
  sample_display_draw_main(widget, &event->area);
  return FALSE;
}

static void
sample_display_handle_motion (SampleDisplay *s,
			      int x,
			      int y,
			      int just_clicked)
{
  sw_sample * sample;
  sw_sel * sel;
  int ol, or;
  int ss, se;

  if(!s->selecting)
    return;

  if (!s->view->sample)
    return;

  if (!s->view->sample->tmp_sel)
    return;

  sample = s->view->sample;

  sel = sample->tmp_sel;

  ss = sel->sel_start;
  se = sel->sel_end;

  /* XXX: Could scroll here ... which should be hooked up to a timeout */
  if(x < 0)
    x = 0;
  else if(x >= s->width)
    x = s->width - 1;

  /* Set: ol to the offset of the current mouse position x
   * Set: or to the next useful value: if we're zoomed in beyond
   *         dot-for-sample, then the next sample value;
   *         otherwise the offset corresponding to the next pixel x+1.
   */
  ol = XPOS_TO_OFFSET(x);
  if((s->view->end - s->view->start) < s->width) {
    or = XPOS_TO_OFFSET(x) + 1;
  } else {
    or = XPOS_TO_OFFSET(x + 1);
  }

  if (ol < 0 || ol >= s->view->sample->soundfile->nr_frames) return;
  if (or < 0 || or >= s->view->sample->soundfile->nr_frames) return;
#if 0
  g_return_if_fail(ol >= 0 && ol < s->view->sample->soundfile->nr_frames);
  g_return_if_fail(or > 0 && or <= s->view->sample->soundfile->nr_frames);
#endif

  g_return_if_fail(ol < or);

  switch(s->selecting) {
  case SELECTING_SELECTION_START:
    if(just_clicked) {
#if 0
      if(ss != -1 && ol < se) {
	ss = ol;
      } else {
#endif
	ss = ol;
	se = ol + 1;
#if 0
      }
#endif
    } else {
      if(ol < se) {
	ss = ol;
      } else { /* swap start,end */
	ss = se - 1;
	se = or;
	s->selecting = SELECTING_SELECTION_END;
      }
    }
    break;
  case SELECTING_SELECTION_END:
    if(just_clicked) {
      if(ss != -1 && or > ss) {
	se = or;
      } else {
	ss = or - 1;
	se = or;
      }
    } else {
      if(or > ss) {
	se = or;
      } else { /* swap end,start */
	se = ss + 1;
	ss = ol;
	s->selecting = SELECTING_SELECTION_START;
      }
    }
    break;
  default:
    g_assert_not_reached();
    break;
  }

  if(sel->sel_start != ss || sel->sel_end != se || just_clicked) {
    sel->sel_start = ss;
    sel->sel_end = se;
    gtk_signal_emit(GTK_OBJECT(s),
		    sample_display_signals[SIG_SELECTION_CHANGED]);
  }
}

/* Handle middle mousebutton display window panning */
static void
sample_display_handle_motion_2 (SampleDisplay *s,
				int x,
				int y)
{
  int new_win_start =
    s->selecting_wins0 + (s->selecting_x0 - x) *
    (s->view->end - s->view->start) / s->width;

  new_win_start = CLAMP(new_win_start, 0,
			s->view->sample->soundfile->nr_frames -
			(s->view->end - s->view->start));

  if(new_win_start != s->view->start) {
    sample_display_set_window (s,
			       new_win_start,
			       new_win_start +
			       (s->view->end - s->view->start));
  }
}

void
sample_display_clear_sel (SampleDisplay * s)
{
  GtkWidget * widget = GTK_WIDGET(s);

  sample_clear_tmp_sel (s->view->sample);
  s->selecting = SELECTING_NOTHING;
  s->selection_mode = SELECTION_MODE_NONE;
  SET_CURSOR(widget, crosshair_cr);
  gtk_signal_emit(GTK_OBJECT(s),
		  sample_display_signals[SIG_SELECTION_CHANGED]);
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
    SET_CURSOR(widget, horiz_plus_cr);
  } else {
    SET_CURSOR(widget, horiz_cr);
  }
}

static gint
sample_display_button_press (GtkWidget      *widget,
			     GdkEventButton *event)
{
  SampleDisplay *s;
  GdkModifierType state;
  GList * gl;
  sw_sel * sel;
  sw_sample * sample;
  int x, y;
  int xss, xse;
  int o, vlen;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (IS_SAMPLE_DISPLAY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  s = SAMPLE_DISPLAY(widget);
  
  if(!IS_INITIALIZED(s))
    return TRUE;

  sample = s->view->sample;

  /* Deal with buttons 4 & 5 (mouse wheel) separately */
  if (event->button == 4) {
    /* mouse wheel up */
    view_volume_increase (s->view);
    return TRUE;
  } else if (event->button == 5) {
    /* mouse wheel down */
    view_volume_decrease (s->view);
    return TRUE;
  }


  if (current_tool == TOOL_ZOOM) {
    gdk_window_get_pointer (event->window, &x, &y, &state);
    o = XPOS_TO_OFFSET(x);
    vlen = s->view->end - s->view->start;
    if (state & GDK_SHIFT_MASK) {
      view_set_ends (s->view, o - 3*vlen/4, o + 3*vlen/4);
    } else {
      view_set_ends (s->view, o - vlen/4, o + vlen/4);
    }
    return TRUE;
  }

  /* Cancel the current operation if a different button is pressed. */
  if(s->selecting && event->button != last_button) {
    sample_display_clear_sel (s);
  } else if (last_tmp_view && last_tmp_view != s->view &&
	     event->button != last_button) {
    view_clear_last_tmp_view ();
  } else {
    last_button = event->button;
    gdk_window_get_pointer (event->window, &x, &y, &state);
    
    if(last_button == 1) {

      if (XPOS_TO_OFFSET(x) < 0 ||
	  XPOS_TO_OFFSET(x) > sample->soundfile->nr_frames)
	return TRUE;

      for (gl = sample->soundfile->sels; gl; gl = gl->next) {
	sel = (sw_sel *)gl->data;
	
	xss = OFFSET_TO_XPOS(sel->sel_start);
	xse = OFFSET_TO_XPOS(sel->sel_end);
	
	/* If the cursor is near the current start or end of
	   * the selection, move that.
	   */
	if(abs(x-xss) < 3) {
	  sample_set_tmp_sel (sample, s->view, sel);
	  s->selecting = SELECTING_SELECTION_START;
	  s->selection_mode = SELECTION_MODE_INTERSECT;
	  sample_display_set_intersect_cursor (s);
	  sample_display_handle_motion(s, x, y, FALSE);
	  return TRUE;
	} else if(abs(x-xse) < 3) {
	  sample_set_tmp_sel (sample, s->view, sel);
	  s->selecting = SELECTING_SELECTION_END;
	  s->selection_mode = SELECTION_MODE_INTERSECT;
	  sample_display_set_intersect_cursor (s);
	  sample_display_handle_motion(s, x, y, FALSE);
	  return TRUE;
	}
      }

      /* Otherwise, start a new selection. */

      sample_set_tmp_sel_1(sample, s->view,
			   XPOS_TO_OFFSET(x),
			   XPOS_TO_OFFSET(x)+1);

      s->selecting = SELECTING_SELECTION_START;

      if(state & GDK_SHIFT_MASK) {
	s->selection_mode = SELECTION_MODE_INTERSECT;
	sample_display_set_intersect_cursor (s);
      } else {
	s->selection_mode = SELECTION_MODE_REPLACE;
	SET_CURSOR(widget, horiz_cr);
      }

      sample_display_handle_motion(s, x, y, TRUE);
    } else if(last_button == 2) {
      s->selecting = SELECTING_PAN_WINDOW;
      gdk_window_get_pointer (event->window, &s->selecting_x0, NULL, NULL);
      s->selecting_wins0 = s->view->start;
      SET_CURSOR(widget, move_cr);
    } else if(last_button == 3) {
      if(s->view && s->view->menu) {
	gtk_menu_popup(GTK_MENU(s->view->menu),
		       NULL, NULL, NULL,
		       NULL, 3, event->time);
      }
    }
  }
	    
  return TRUE;
}

void
sample_display_sink_tmp_sel (SampleDisplay * s)
{
  sw_sample * sample = s->view->sample;
  
  if (!(s->selecting == SELECTING_SELECTION_START ||
	s->selecting == SELECTING_SELECTION_END)) {
    return;
  }
  s->selecting = SELECTING_NOTHING;

  if(s->selection_mode == SELECTION_MODE_REPLACE) {
    sample_selection_replace_with_tmp_sel (sample);
  } else {
    sample_selection_insert_tmp_sel (sample);
  }
  s->selection_mode = SELECTION_MODE_NONE;

  gtk_signal_emit(GTK_OBJECT(s),
		  sample_display_signals[SIG_SELECTION_CHANGED]);
}

static gint
sample_display_button_release (GtkWidget      *widget,
			       GdkEventButton *event)
{
  SampleDisplay *s;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (IS_SAMPLE_DISPLAY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
    
  s = SAMPLE_DISPLAY(widget);

  switch (current_tool) {
  case TOOL_SELECT:
    
    /* If the user has released the button they were selecting with,
     * sink this sample's temporary selection.
     */
    if (s->selecting && event->button == last_button) {
      sample_display_sink_tmp_sel (s);
    }
    
    /* If the user has released the button in a sweep window different
     * to that used for selection, then sink the appropriate temporary
     * selection.
     */
    if (last_tmp_view != s->view) {
      view_sink_last_tmp_view();
    }

    break;
  case TOOL_MOVE:
    break;
  case TOOL_ZOOM:
    break;
  default:
    break;
  }

  SET_CURSOR(widget, crosshair_cr);
    
  return FALSE;
}

static gint
sample_display_on_sel (SampleDisplay * s, gint x, gint y)
{
  GList * gl;
  sw_sel * sel;
  int xss, xse;

  for (gl = s->view->sample->soundfile->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    xss = OFFSET_TO_XPOS(sel->sel_start);
    xse = OFFSET_TO_XPOS(sel->sel_end);

    if(abs(x-xss) < 3 || abs(x-xse) < 3)
      return TRUE;    
  }

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
  gtk_signal_emit(GTK_OBJECT(s),
		  sample_display_signals[SIG_MOUSE_OFFSET_CHANGED]);

  if(!s->selecting) {
    if (current_tool == TOOL_SELECT && sample_display_on_sel (s, x, y)) {
      SET_CURSOR(widget, horiz_cr);
    } else {
      if (o > 0 && o < s->view->sample->soundfile->nr_frames)
	SET_CURSOR(widget, crosshair_cr);
      else
	gdk_window_set_cursor (widget->window, NULL);
    }

#if 0
    /* Consistency check: without this, some button release events
     * are lost if they occur during motion. Here, we ensure that
     * if this sample_display s is not selecting yet has a selection
     */
    if(s->selection_mode && s->view->sample->tmp_sel) {
      sample_display_sink_tmp_sel (s);
    }
#endif

    return FALSE;
  }

  /* ignore non select motion for now */
  if (current_tool != TOOL_SELECT) return FALSE;

  if((state & GDK_BUTTON1_MASK) && last_button == 1) {
    sample_display_handle_motion(s, x, y, 0);
  } else if((state & GDK_BUTTON2_MASK) && last_button == 2) {
    sample_display_handle_motion_2(s, x, y);
  } else {
    /*    sample_display_clear_sel (s);*/
    sample_display_sink_tmp_sel (s);
    /* XXX: Need to sink_tmp_sel here instead for consistency ??? 
     *  It seems to be clearing fast tmp_sels now, but at least not
     * leaving them lying around.*/
  }
    
  return FALSE;
}

static gint
sample_display_leave_notify (GtkWidget *widget,
			     GdkEventCrossing *event)
{
  SampleDisplay *s;
 
  s = SAMPLE_DISPLAY(widget);
  s->mouse_offset = -1;
  gtk_signal_emit(GTK_OBJECT(s),
		  sample_display_signals[SIG_MOUSE_OFFSET_CHANGED]);

  return TRUE;
}

static void
sample_display_class_init (SampleDisplayClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  int n;
  const int *p;
  GdkColor *c;
  GdkBitmap * bitmap;
  GdkBitmap * mask;
  GdkColor white = {0, 0xffff, 0xffff, 0xffff};
  GdkColor black = {0, 0x0000, 0x0000, 0x0000};

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  widget_class->realize = sample_display_realize;
  widget_class->size_allocate = sample_display_size_allocate;
  widget_class->expose_event = sample_display_expose;
  widget_class->draw = sample_display_draw;
  widget_class->size_request = sample_display_size_request;
  widget_class->button_press_event = sample_display_button_press;
  widget_class->button_release_event = sample_display_button_release;
  widget_class->motion_notify_event = sample_display_motion_notify;
  widget_class->leave_notify_event = sample_display_leave_notify;

  sample_display_signals[SIG_SELECTION_CHANGED] =
    gtk_signal_new ("selection-changed",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET(SampleDisplayClass, selection_changed),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);

  sample_display_signals[SIG_WINDOW_CHANGED] =
    gtk_signal_new ("window-changed",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET(SampleDisplayClass, window_changed),
		    gtk_marshal_NONE__INT_INT,
		    GTK_TYPE_NONE, 0);

  sample_display_signals[SIG_MOUSE_OFFSET_CHANGED] =
    gtk_signal_new ("mouse-offset-changed",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET(SampleDisplayClass,
				      mouse_offset_changed),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals(object_class, sample_display_signals,
			       LAST_SIGNAL);
    
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
    gdk_color_alloc(gdk_colormap_get_system(), c);
  }

  class->crosshair_cr = gdk_cursor_new(GDK_CROSSHAIR);
  class->move_cr = gdk_cursor_new(GDK_FLEUR);

  create_bitmap_and_mask_from_xpm (&bitmap, &mask, horiz_xpm);
  
  class->horiz_cr = gdk_cursor_new_from_pixmap (bitmap, mask,
						&white, &black,
						8, 8);

  create_bitmap_and_mask_from_xpm (&bitmap, &mask, horiz_plus_xpm);
  
  class->horiz_plus_cr = gdk_cursor_new_from_pixmap (bitmap, mask,
						     &white, &black,
						     8, 8);

}

static void
sample_display_init (SampleDisplay *s)
{
  s->backing_pixmap = NULL;
  s->view = NULL;
  s->selecting = SELECTING_NOTHING;
  s->selection_mode = SELECTION_MODE_NONE;
  s->marching_tag = 0;
  s->mouse_x = 0;
  s->mouse_offset = 0;
}

guint
sample_display_get_type (void)
{
  static guint sample_display_type = 0;

  if (!sample_display_type) {
    GtkTypeInfo sample_display_info =
    {
      "SampleDisplay",
      sizeof(SampleDisplay),
      sizeof(SampleDisplayClass),
      (GtkClassInitFunc) sample_display_class_init,
      (GtkObjectInitFunc) sample_display_init,
      (GtkArgSetFunc) NULL,
      (GtkArgGetFunc) NULL,
    };

    sample_display_type =
      gtk_type_unique (gtk_widget_get_type (), &sample_display_info);
  }

  return sample_display_type;
}

GtkWidget*
sample_display_new (void)
{
  SampleDisplay *s = SAMPLE_DISPLAY(gtk_type_new(sample_display_get_type()));

  return GTK_WIDGET(s);
}

