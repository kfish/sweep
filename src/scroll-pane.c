/*
 * Sweep, a sound wave editor.
 *
 * Copyright (C) 2000 Conrad Parker
 * Copyright (C) 2006 Radoslaw Korzeniewski
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

#include "scroll-pane.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gtk/gtkrange.h>

#include <sweep/sweep_i18n.h>

#include "sweep_app.h"
#include "sample-display.h"
#include "sample.h"
#include "head.h"
#include "cursors.h"
#include "play.h"
#include "callbacks.h"
#include "edit.h"
#include "undo_dialog.h"

//#define DEBUG

/* Maximum number of samples to consider per pixel */
#define STEP_MAX 64

#define SCROLL_PANE_BORDER_LEFT		3
#define SCROLL_PANE_BORDER_RIGHT	3
#define SCROLL_PANE_BORDER_TOP		2
#define SCROLL_PANE_BORDER_DOWN		2

#define SCROLL_PANE_MIN_HEIGHT			28
#define SCROLL_PANE_MIN_CHANNEL_HEIGHT		7

#define IS_INITIALIZED(s) (s->view != NULL)

static const int default_colors[] = {
  0, 0, 0,  	/* default */
  64, 64, 64,  	/* slider */
  220, 0, 0,  	/* playmaker */
};

G_DEFINE_TYPE (ScrollPane, scroll_pane, GTK_TYPE_HSCROLLBAR)

void
scroll_pane_set_view (ScrollPane *s, sw_view *view)
{
  g_return_if_fail(s != NULL);
  g_return_if_fail(IS_SCROLL_PANE(s));

  s->view = view;
  s->play_offset = -1;
  gtk_widget_queue_draw(GTK_WIDGET(s));
}

static void
scroll_pane_size_request (GtkWidget *widget,
			  GtkRequisition *requisition)
{
  ScrollPane *s;

  requisition->width = 40;

  s = SCROLL_PANE(widget);
  requisition->height = MAX ( SCROLL_PANE_MIN_HEIGHT, 
			      s->view->sample->sounddata->format->channels * SCROLL_PANE_MIN_CHANNEL_HEIGHT 
				+ SCROLL_PANE_BORDER_TOP + SCROLL_PANE_BORDER_DOWN);

#ifdef DEBUG
  g_print("ScrollPane->SIZE_REQUEST w:%i h:%i\n", requisition->width, requisition->height);
#endif

}

static void
scroll_pane_draw_data_channel ( GdkDrawable * win,
				const ScrollPane * s,
        GdkGC *inside_gc,
        GdkGC *outside_gc,
				gint x,
				gint y,
				gint width,
				gint height,
				gint channel)
{
  GdkGC *gc, *gc_slider;
  sw_audio_t d, maxd, mind;
  sw_framecount_t i, step, nr_frames;
  sw_sample * sample;
  GtkWidget *widget;
  gfloat frames_to_pixel;
  gint ypos1, ypos2;
  GtkRange *range;
  gfloat hh = height / 2.0;
  const gint channels = s->view->sample->sounddata->format->channels;

  sample = s->view->sample;

  if(!sample)
	return;

  widget = GTK_WIDGET(s);
  range = GTK_RANGE(s);

  nr_frames = sample->sounddata->nr_frames;

  if (!nr_frames)
	return;

  gc = outside_gc;
        //s->fg_gcs[SCROLL_PANECOL_FG];
  gc_slider = inside_gc ? inside_gc : s->fg_gcs[SCROLL_PANECOL_SLIDER];

  frames_to_pixel = nr_frames / widget->allocation.width;

  step = MAX (1, frames_to_pixel/STEP_MAX);

  while(width > 0) {
    maxd = 2 * SW_AUDIO_T_MIN;
    mind = 2 * SW_AUDIO_T_MAX;

    g_mutex_lock (sample->ops_mutex);

    for (i = frames_to_pixel * (x - widget->allocation.x - 2);
	 i < frames_to_pixel * (x - widget->allocation.x + 1 - 2);
	 i += step) {
      d = ((sw_audio_t *)sample->sounddata->data)[i*channels + channel];
      if (d > maxd) { maxd = d; }
      if (d < mind) { mind = d; }
    }

    g_mutex_unlock (sample->ops_mutex);
   
#define YPOS(v)	(y + hh - (v) * hh)
    ypos1 = YPOS(maxd);
    ypos2 = YPOS(mind);
    gdk_draw_line(win, 
		  (range->slider_start <= (x - widget->allocation.x) && range->slider_end > (x - widget->allocation.x)) ? gc_slider : gc,
		  x, ypos1,
		  x, ypos2);

    x++;
    width--;
  }
}

static gint
scroll_pane_expose (GtkWidget      *widget,
                  GdkEventExpose *event)
{
  ScrollPane *scrollbar;
  GtkRange *range;
  GdkRectangle expose_area;
  gint i, channels;
  SampleDisplay *sd;


  range = GTK_RANGE (widget);
  scrollbar = SCROLL_PANE (widget);
  
  if(!IS_INITIALIZED(scrollbar))
    return TRUE;
  
  channels = scrollbar->view->sample->sounddata->format->channels;
  sd = SAMPLE_DISPLAY (scrollbar->view->display);

  expose_area = event->area;


  gdk_draw_rectangle (widget->window, 
                      sd->gcs[SCHEME_ELEMENT_TROUGH_BG],
                      TRUE,
                      widget->allocation.x,
                      widget->allocation.y,
                      widget->allocation.width,
                      widget->allocation.height);
    
  gdk_draw_rectangle (widget->window, 
                      sd->gcs[SCHEME_ELEMENT_BG],
                      TRUE,
                      widget->allocation.x + range->slider_start,
                      widget->allocation.y,
                      range->slider_end - range->slider_start,
                      widget->allocation.height);
    
#ifdef DEBUG
  g_print ("ScrollPane->EXPOSEAREA: x:%i y:%i width:%i height:%i\n", expose_area.x, expose_area.y, expose_area.width, expose_area.height);
  g_print ("ScrollPane->WIDGET: x:%i y:%i width:%i height:%i\n", widget->allocation.x, widget->allocation.y, 
		widget->allocation.width, widget->allocation.height);
  g_print ("ScrollPane->RANGE: x:%i y:%i width:%i height:%i\n", GTK_RANGE(scrollbar)->range_rect.x, GTK_RANGE(scrollbar)->range_rect.y,
		GTK_RANGE(scrollbar)->range_rect.width, GTK_RANGE(scrollbar)->range_rect.height);
  g_print ("ScrollPane->SLIDER: start:%i end:%i\n", range->slider_start, range->slider_end);
#endif

  gfloat sh;
  gfloat wh = (widget->allocation.height - SCROLL_PANE_BORDER_TOP - SCROLL_PANE_BORDER_DOWN) * 1.0 / channels;

  for (i = 0; i < channels; i++){
  	sh = i * wh;
#ifdef DEBUG
        GdkGC *gc;
	g_print("SCROLL_CHANNEL: sh:%f wh:%f\n", sh, wh);

        gc = GTK_WIDGET(widget)->style->fg_gc[3];
	gdk_draw_rectangle(widget->window, gc, FALSE,
			widget->allocation.x + SCROLL_PANE_BORDER_LEFT,
			widget->allocation.y + SCROLL_PANE_BORDER_TOP + sh,
			widget->allocation.width - SCROLL_PANE_BORDER_LEFT - SCROLL_PANE_BORDER_RIGHT,
			wh);
#endif

 	scroll_pane_draw_data_channel(widget->window, 
					scrollbar, 
          sd->gcs[SCHEME_ELEMENT_FG],
          sd->gcs[SCHEME_ELEMENT_TROUGH_FG],
					widget->allocation.x + SCROLL_PANE_BORDER_LEFT,
					widget->allocation.y + SCROLL_PANE_BORDER_TOP + sh, 
					widget->allocation.width - SCROLL_PANE_BORDER_LEFT - SCROLL_PANE_BORDER_RIGHT, 
					wh, i);
  }
  if (scrollbar->play_offset >= 0){
	  gdk_draw_line( widget->window, 
			 scrollbar->fg_gcs[SCROLL_PANECOL_PLAYMAKER],
			 widget->allocation.x + scrollbar->play_offset, 
			 widget->allocation.y + SCROLL_PANE_BORDER_TOP,
			 widget->allocation.x + scrollbar->play_offset, 
			 widget->allocation.y + widget->allocation.height - SCROLL_PANE_BORDER_TOP - SCROLL_PANE_BORDER_DOWN + 1);
  }
    
  gdk_draw_rectangle (widget->window, 
                      widget->style->black_gc,
                      FALSE,
                      widget->allocation.x + range->slider_start,
                      widget->allocation.y,
                      range->slider_end - range->slider_start,
                      widget->allocation.height - 1 );
    
  cairo_t *cr;
  cairo_pattern_t *pattern;
	cr = gdk_cairo_create (widget->window);

  pattern = cairo_pattern_create_linear (widget->allocation.x + range->slider_start,
                                         widget->allocation.y,
                                         widget->allocation.x + range->slider_start,
                                        (widget->allocation.height - 1) + widget->allocation.y);
  cairo_pattern_add_color_stop_rgba (pattern, 0, 1.0, 1.0, 1.0, 0.4);
  cairo_pattern_add_color_stop_rgba (pattern, .5, 1.0, 1.0, 1.0, 0);
  cairo_pattern_add_color_stop_rgba (pattern, 1, 0.0, 0.0, 0.0, .15);

  cairo_set_source (cr, pattern);
  cairo_rectangle (cr, widget->allocation.x + range->slider_start, 
                   widget->allocation.y + 1,
                   range->slider_end - range->slider_start + 1,
                   widget->allocation.height - 1 );
  cairo_fill (cr);
  cairo_pattern_destroy (pattern);

    
  return FALSE;
}

static void
scroll_pane_realize (GtkWidget *widget)
{
  gint i;
  ScrollPane *s;

  (* GTK_WIDGET_CLASS (scroll_pane_parent_class)->realize) (widget);

  s = SCROLL_PANE(widget);

  for (i = 0; i < SCROLL_PANECOL_LAST; i++) {
    s->fg_gcs[i] = gdk_gc_new (widget->window);
    gdk_gc_set_foreground(s->fg_gcs[i], &SCROLL_PANE_CLASS(GTK_WIDGET_GET_CLASS(widget))->colors[i]);
  }

}

static void
scroll_pane_style_set  (GtkWidget *widget,
                          GtkStyle  *previous)
{
  GtkRange *range;

  (* GTK_WIDGET_CLASS (scroll_pane_parent_class)->style_set) (widget, previous);

  range = GTK_RANGE (widget);

  range->has_stepper_a = FALSE;
  range->has_stepper_b = FALSE;
  range->has_stepper_c = FALSE;
  range->has_stepper_d = FALSE;

  range->min_slider_size = 8;
}

void
scroll_pane_refresh (ScrollPane *s)
{
//  gfloat frames_to_pixel = s->view->sample->sounddata->nr_frames / GTK_WIDGET(s)->allocation.width;;

  if(!IS_INITIALIZED(s))
    return;
/*
  s->play_offset = s->view->sample->play_head->offset * frames_to_pixel;
  g_print("OFFSET: %i FRAMES_TO_PIXEL: %f PLAY_OFFSET %i\n", s->view->sample->play_head->offset, frames_to_pixel, s->play_offset);
*/
  gtk_widget_queue_draw (GTK_WIDGET(s));
}


static void
scroll_pane_class_init (ScrollPaneClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  gint n;
  const int *p;
  GdkColor *c;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  widget_class->expose_event = scroll_pane_expose;
  widget_class->size_request = scroll_pane_size_request;
  widget_class->realize = scroll_pane_realize;

  widget_class->style_set = scroll_pane_style_set;

  for(n = 0, p = default_colors, c = class->colors;
      n < SCROLL_PANECOL_LAST; n++, c++) {
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
scroll_pane_init (ScrollPane *s)
{
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET(s), GTK_CAN_FOCUS);

  s->view = NULL;
}

GtkWidget*
scroll_pane_new (GtkAdjustment *adjustment)
{
  GtkWidget *scrollbar;
  
  scrollbar = g_object_new (scroll_pane_get_type (), 
			"adjustment", adjustment, NULL );
  return scrollbar;
}
