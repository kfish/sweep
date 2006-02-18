/*
 * Sweep, a sound wave editor.
 *
 * time_ruler, modified from hruler in GTK+ 1.2.x
 * by Conrad Parker 2000 for Sweep.
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
 * GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "time_ruler.h"

#include <sweep/sweep_typeconvert.h>

#include "print.h"

#define RULER_HEIGHT          14
#define MINIMUM_INCR          5
#define MAXIMUM_SUBDIVIDE     5
#define MAXIMUM_SCALES        21

#define ROUND(x) ((int) ((x) + 0.5))


static void time_ruler_class_init    (TimeRulerClass *klass);
static void time_ruler_init          (TimeRuler      *time_ruler);
static gint time_ruler_button_press  (GtkWidget * widget,
				      GdkEventButton * event);
static gint time_ruler_motion_notify (GtkWidget      *widget,
				      GdkEventMotion *event);
static void time_ruler_draw_ticks    (GtkRuler       *ruler);
static void time_ruler_draw_pos      (GtkRuler       *ruler);

GType
time_ruler_get_type (void)
{
  static GType time_ruler_type = 0;

  if (!time_ruler_type)
    {
      static const GTypeInfo time_ruler_info =
      {
		  
	sizeof (TimeRulerClass),
	NULL, /* base_init */
	NULL, /* base_finalize */
	(GClassInitFunc) time_ruler_class_init,
	NULL, /* class_finalize */
	NULL, /* class_data */
	sizeof (TimeRuler),
	0,    /* n_preallocs */
	(GInstanceInitFunc) time_ruler_init,	  

      };

      time_ruler_type = g_type_register_static (GTK_TYPE_RULER, "TimeRuler", &time_ruler_info, 0);
    }

  return time_ruler_type;
}

static void
time_ruler_class_init (TimeRulerClass *klass)
{
  GtkWidgetClass *widget_class;
  GtkRulerClass *ruler_class;

  widget_class = (GtkWidgetClass*) klass;
  ruler_class = (GtkRulerClass*) klass;

  /*  widget_class->realize = time_ruler_realize;*/
  widget_class->button_press_event = time_ruler_button_press;
  widget_class->motion_notify_event = time_ruler_motion_notify;

  ruler_class->draw_ticks = time_ruler_draw_ticks;
  ruler_class->draw_pos = time_ruler_draw_pos;
}

static gfloat ruler_scale[MAXIMUM_SCALES] =
{ 0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1,
  2.5, 5, 10, 15, 30, 60, 300, 600, 1800, 3600, 18000, 36000 };

static gint subdivide[MAXIMUM_SUBDIVIDE] = { 1, 2, 5, 10, 100 };

static void
time_ruler_init (TimeRuler *time_ruler)
{
  GtkWidget *widget;

  time_ruler->samplerate = 44100;

  widget = GTK_WIDGET (time_ruler);
  widget->requisition.width = widget->style->xthickness * 2 + 1;
  widget->requisition.height = widget->style->ythickness * 2 + RULER_HEIGHT;
}

GtkWidget*
time_ruler_new (void)
{
  return GTK_WIDGET (g_object_new (time_ruler_get_type (), NULL));
}

static gint
time_ruler_motion_notify (GtkWidget *widget, GdkEventMotion *event)
{
  GtkRuler *ruler;
  gint x;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TIME_RULER (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  ruler = GTK_RULER (widget);

  if (event->is_hint)
    gdk_window_get_pointer (widget->window, &x, NULL, NULL);
  else
    x = event->x;

  ruler->position = ruler->lower + ((ruler->upper - ruler->lower) * x) / widget->allocation.width;

  /*  Make sure the ruler has been allocated already  */
  if (ruler->backing_store != NULL)
    gtk_ruler_draw_pos (ruler);

  return FALSE;
}

static gint
time_ruler_button_press (GtkWidget * widget, GdkEventButton * event)
{
  GdkModifierType state;
  int x, y;

  gdk_window_get_pointer (event->window, &x, &y, &state);

  return TRUE;
}

static void
time_ruler_draw_ticks (GtkRuler *ruler)
{
  GtkWidget *widget;
  GdkGC *gc, *bg_gc;
  gint i;
  gint width, height;
  gint xthickness;
  gint ythickness;
  gint length, ideal_length;
  gdouble lower, upper;		/* Upper and lower limits, in ruler units */
  gdouble increment, abs_increment; /* Number of pixels per unit */
  gint scale;			/* Number of units per major unit */
  gdouble subd_incr;
  gdouble start, end, cur;
#define UNIT_STR_LEN 32
  gchar unit_str[UNIT_STR_LEN];
  gint digit_height;
  gint digit_offset;
  gint text_width;
  gint pos;
  PangoLayout *layout;
  PangoRectangle logical_rect, ink_rect;

  g_return_if_fail (ruler != NULL);
  g_return_if_fail (GTK_IS_TIME_RULER (ruler));

  if (!GTK_WIDGET_DRAWABLE (ruler)) 
    return;

  widget = GTK_WIDGET (ruler);

  gc = widget->style->fg_gc[GTK_STATE_NORMAL];
  bg_gc = widget->style->bg_gc[GTK_STATE_NORMAL];

  xthickness = widget->style->xthickness;
  ythickness = widget->style->ythickness;

  width = widget->allocation.width;
  height = widget->allocation.height - ythickness * 2;

  layout = gtk_widget_create_pango_layout (widget, "012456789");
  pango_layout_get_extents (layout, &ink_rect, &logical_rect);
  
  digit_height = PANGO_PIXELS (ink_rect.height) + 2;
  digit_offset = ink_rect.y;

   
  gtk_paint_box (widget->style, ruler->backing_store,
		 GTK_STATE_NORMAL, GTK_SHADOW_OUT, 
		 NULL, widget, "time_ruler",
		 0, 0, 
		 widget->allocation.width, widget->allocation.height);


  gdk_draw_line (ruler->backing_store, gc,
		 xthickness,
		 height + ythickness,
		 widget->allocation.width - xthickness,
		 height + ythickness);

  upper = ruler->upper / TIME_RULER(ruler)->samplerate;
  lower = ruler->lower / TIME_RULER(ruler)->samplerate;
  printf("upper = %f. lower = %f.\n", upper, lower);

  if ((upper - lower) == 0) 
    return;

  increment = (gdouble) width / (upper - lower);
  abs_increment = (gdouble) fabs((double)increment);

  /* determine the scale
   *  We calculate the text size as for the vruler instead of using
   *  text_width = gdk_string_width(font, unit_str), so that the result
   *  for the scale looks consistent with an accompanying vruler
   */
  scale = ceil (ruler->max_size / TIME_RULER(ruler)->samplerate);
  snprint_time (unit_str, UNIT_STR_LEN, (sw_time_t)scale);
  /*  snprint_time_smpte (unit_str, UNIT_STR_LEN, (sw_time_t)scale, 10.0);*/

 text_width = strlen (unit_str) * digit_height + 1;

  for (scale = 0; scale < MAXIMUM_SCALES; scale++)
    if (ruler_scale[scale] * abs_increment > 2 * text_width)
      break;

  if (scale == MAXIMUM_SCALES)
    scale = MAXIMUM_SCALES - 1;

  /* drawing starts here */
  length = 0;
  for (i = MAXIMUM_SUBDIVIDE - 1; i >= 0; i--)
    {
      subd_incr = (gdouble) ruler_scale[scale] / 
	          (gdouble) subdivide[i];
      if (subd_incr * fabs(increment) <= MINIMUM_INCR) 
	continue;
	    printf("subd_incr = %f.\n", subd_incr);

      /* Calculate the length of the tickmarks. Make sure that
       * this length increases for each set of ticks
       */
      ideal_length = height / (i + 1) - 1;
      if (ideal_length > ++length)
	length = ideal_length;

      if (lower < upper)
	{
	  start = floor (lower / subd_incr) * subd_incr;
	  end   = ceil  (upper / subd_incr) * subd_incr;
	}
      else
	{
	  start = floor (upper / subd_incr) * subd_incr;
	  end   = ceil  (lower / subd_incr) * subd_incr;
	}

  
      for (cur = start; cur <= end; cur += subd_incr)
	{
	  pos = ROUND ((cur - lower) * increment);

	  gdk_draw_line (ruler->backing_store, gc,
			 pos, height + ythickness, 
			 pos, height - length + ythickness);

	  /* draw label */ 
	  if (i == 0)
	    {
#if 1
	      snprint_time (unit_str, UNIT_STR_LEN, (sw_time_t)cur);
#else
	      snprint_time_smpte (unit_str, UNIT_STR_LEN, (sw_time_t)cur, 10.0);
#endif
  		  pango_layout_set_text (layout, unit_str, -1);
 		  pango_layout_get_extents (layout, NULL, &logical_rect);

 		  gtk_paint_layout (widget->style,
                  ruler->backing_store,
                  GTK_WIDGET_STATE (widget),
				  FALSE,
                  NULL,
                  widget,
                  "vruler",
                  pos +2,
                  2,
                  layout);
	    }
	 }
   }
}

static void
time_ruler_draw_pos (GtkRuler *ruler)
{
  GtkWidget *widget;
  GdkGC *gc;
  int i;
  gint x, y;
  gint width, height;
  gint bs_width, bs_height;
  gint xthickness;
  gint ythickness;
  gfloat increment;

  g_return_if_fail (ruler != NULL);
  g_return_if_fail (GTK_IS_TIME_RULER (ruler));

  if (GTK_WIDGET_DRAWABLE (ruler))
    {
      widget = GTK_WIDGET (ruler);

      gc = widget->style->fg_gc[GTK_STATE_NORMAL];
      xthickness = widget->style->xthickness;
      ythickness = widget->style->ythickness;
      width = widget->allocation.width;
      height = widget->allocation.height - ythickness * 2;

      bs_width = height / 2;
      bs_width |= 1;  /* make sure it's odd */
      bs_height = bs_width / 2 + 1;

      if ((bs_width > 0) && (bs_height > 0))
	{
	  /*  If a backing store exists, restore the ruler  */
	  if (ruler->backing_store && ruler->non_gr_exp_gc)
	    gdk_draw_drawable (ruler->widget.window,
			     ruler->non_gr_exp_gc,
			     ruler->backing_store,
			     ruler->xsrc, ruler->ysrc,
			     ruler->xsrc, ruler->ysrc,
			     bs_width, bs_height);

	  increment = (gfloat) width / (ruler->upper - ruler->lower);

	  x = ROUND ((ruler->position - ruler->lower) * increment) +
	    (xthickness - bs_width) / 2 - 1;
	  y = (height + bs_height) / 2 + ythickness;

	  for (i = 0; i < bs_height; i++)
	    gdk_draw_line (widget->window, gc,
			   x + i, y + i,
			   x + bs_width - 1 - i, y + i);


	  ruler->xsrc = x;
	  ruler->ysrc = y;
	}
    }
}

void
time_ruler_set_format (TimeRuler * time_ruler, sw_format * f)
{
  time_ruler->samplerate = f->rate;
}
