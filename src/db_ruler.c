/*
 * Sweep, a sound wave editor.
 *
 * db_ruler, modified from hruler in GTK+ 1.2.x
 * by Conrad Parker 2002 for Sweep.
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
#include "db_ruler.h"

#include <sweep/sweep_typeconvert.h>

#include "print.h"

#define RULER_WIDTH           32
#define RULER_HEIGHT          14
#define MINIMUM_INCR          2
#define MAXIMUM_SUBDIVIDE     3
#define MAXIMUM_SCALES        21

#define ROUND(x) ((int) ((x) + 0.5))


static void db_ruler_class_init    (DbRulerClass *klass);
static void db_ruler_init          (DbRuler      *db_ruler);
static void db_ruler_realize (GtkWidget * widget);
static gint 
db_ruler_button_press  (GtkWidget * widget, GdkEventButton * event);
static gint
db_ruler_button_release  (GtkWidget * widget, GdkEventButton * event);
static gint
db_ruler_motion_notify (GtkWidget *widget, GdkEventMotion *event);
static gint
db_ruler_leave_notify (GtkWidget * widget, GdkEventCrossing * event);

static void db_ruler_draw_ticks    (GtkRuler       *ruler);
static void db_ruler_draw_pos      (GtkRuler       *ruler);

static GtkWidgetClass * parent_class = NULL;

enum {
  CHANGED_SIGNAL,
  LAST_SIGNAL
};

static gint db_ruler_signals[LAST_SIGNAL] = { 0 };

GType
db_ruler_get_type (void)
{
  static GType db_ruler_type = 0;

  if (!db_ruler_type)
    {
      static const GTypeInfo db_ruler_info =
      {
		  
	sizeof (DbRulerClass),
	NULL, /* base_init */
	NULL, /* base_finalize */
	(GClassInitFunc) db_ruler_class_init,
	NULL, /* class_finalize */
	NULL, /* class_data */
	sizeof (DbRuler),
	0,    /* n_preallocs */
	(GInstanceInitFunc) db_ruler_init,	  

      };

      db_ruler_type = g_type_register_static (GTK_TYPE_RULER, "DbRuler", &db_ruler_info, 0);
    }

  return db_ruler_type;
}

static void
db_ruler_class_init (DbRulerClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkRulerClass *ruler_class;

  object_class = (GtkObjectClass *) klass;
  widget_class = (GtkWidgetClass*) klass;
  ruler_class = (GtkRulerClass*) klass;

  widget_class->button_press_event = db_ruler_button_press;
  widget_class->motion_notify_event = db_ruler_motion_notify;
  widget_class->button_release_event = db_ruler_button_release;
  widget_class->leave_notify_event = db_ruler_leave_notify;

  ruler_class->draw_ticks = db_ruler_draw_ticks;
  ruler_class->draw_pos = db_ruler_draw_pos;

  parent_class = gtk_type_class (gtk_widget_get_type ());

  db_ruler_signals[CHANGED_SIGNAL] = g_signal_new ("changed",
					 			  G_TYPE_FROM_CLASS (klass),
	                              G_SIGNAL_RUN_FIRST,
	                              G_STRUCT_OFFSET (DbRulerClass, changed),
                                  NULL, 
                                  NULL,                
					 			  g_cclosure_marshal_VOID__VOID,
                                  G_TYPE_NONE, 0);
  klass->changed = NULL;
}

static gfloat ruler_scale[MAXIMUM_SCALES] =
{ 0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1,
  3, 6, 12, 15, 30, 60, 300, 600, 1800, 3600, 18000, 36000 };

static gint subdivide[MAXIMUM_SUBDIVIDE] = { 1, 5, 20 };

static void
db_ruler_init (DbRuler *db_ruler)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (db_ruler);
  widget->requisition.width = widget->style->xthickness * 2 + RULER_WIDTH;
  widget->requisition.height = widget->style->ythickness * 2 + 1;

  DB_RULER(db_ruler)->dragging = FALSE;
}

GtkWidget*
db_ruler_new (void)
{
  return GTK_WIDGET (g_object_new (db_ruler_get_type (), NULL));
}

static gint
db_ruler_motion_notify (GtkWidget *widget, GdkEventMotion *event)
{
  GtkRuler *ruler;
  gfloat delta;
  GdkModifierType state;
  gint y, ydelta;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_DB_RULER (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  ruler = GTK_RULER (widget);

  if (event->is_hint)
    gdk_window_get_pointer (widget->window, NULL, &y, &state);
  else
    y = event->y;

  if (DB_RULER(widget)->dragging && (state & GDK_BUTTON1_MASK)) {
    ydelta = DB_RULER(widget)->y - y;

    delta = (((ruler->lower - ruler->upper) * ydelta) / widget->allocation.height);

    gtk_ruler_set_range (ruler, ruler->lower + delta, ruler->upper + delta,
			 (ruler->upper - ruler->lower)/2.0, 2.0);

    g_signal_emit_by_name (ruler, "changed");

    gtk_ruler_draw_ticks (ruler);
  }

  DB_RULER(widget)->y = y;

  ruler->position =
    ((ruler->lower - ruler->upper) * y) / widget->allocation.height
    - ruler->lower;

  /*  Make sure the ruler has been allocated already  */
  if (ruler->backing_store != NULL)
    gtk_ruler_draw_pos (ruler);

#if 0
  printf ("%f\t%f dB\n", ruler->position, 20 * log10 (fabs(ruler->position)));
#endif

  return FALSE;
}

static gint
db_ruler_button_press (GtkWidget * widget, GdkEventButton * event)
{
  GtkRuler * ruler = GTK_RULER(widget);
  int y;
  float delta;

  gdk_window_get_pointer (event->window, NULL, &y, NULL);
  
  switch (event->button) {
  case 1:
    DB_RULER(widget)->y = y;
    DB_RULER(widget)->dragging = TRUE;
    break;
  case 4:
    delta = ruler->upper - ruler->lower;
    gtk_ruler_set_range (ruler, ruler->lower + delta/8, ruler->upper - delta/8,
			 (ruler->upper - ruler->lower)/2.0, 2.0);    
    g_signal_emit_by_name (ruler, "changed");
    break;
  case 5:
    delta = ruler->upper - ruler->lower;
    gtk_ruler_set_range (ruler, ruler->lower - delta/8, ruler->upper + delta/8,
			 (ruler->upper - ruler->lower)/2.0, 2.0);    
    g_signal_emit_by_name (ruler, "changed");
    break;
  default:
    break;
  }

  return TRUE;
}

static gint
db_ruler_button_release (GtkWidget * widget, GdkEventButton * event)
{
  DB_RULER(widget)->dragging = FALSE;

  return TRUE;
}

static gint
db_ruler_leave_notify (GtkWidget * widget, GdkEventCrossing * event)
{
  DB_RULER(widget)->dragging = FALSE;

  return TRUE;
}

static void
db_ruler_draw_ticks (GtkRuler *ruler)
{
  GtkWidget *widget;
  GdkGC *gc, *bg_gc;
  gint i;
  gint width, height;
  gint xthickness;
  gint ythickness;
  gint length, ideal_length;
  gfloat lower, upper;		/* Upper and lower limits, in ruler units */
  gfloat increment, abs_increment; /* Number of pixels per unit */
  gint scale;			/* Number of units per major unit */
  gfloat subd_incr;
  gfloat start, end, cur;
#define UNIT_STR_LEN 32
  gchar unit_str[UNIT_STR_LEN];
  gint digit_height;
  gint digit_offset;
  gint text_height;
  gint pos;
  PangoLayout *layout;
  PangoRectangle logical_rect, ink_rect;

  g_return_if_fail (ruler != NULL);
  g_return_if_fail (GTK_IS_DB_RULER (ruler));

  if (!GTK_WIDGET_DRAWABLE (ruler)) 
    return;

  widget = GTK_WIDGET (ruler);

  gc = widget->style->fg_gc[GTK_STATE_NORMAL];
  bg_gc = widget->style->bg_gc[GTK_STATE_NORMAL];

  xthickness = widget->style->xthickness;
  ythickness = widget->style->ythickness;

  width = widget->allocation.width - ( xthickness * 2) ;
  /*  height = widget->allocation.height - ythickness * 2;*/
  height = widget->allocation.height;

  layout = gtk_widget_create_pango_layout (widget, "012456789dBinf-");
  pango_layout_get_extents (layout, &ink_rect, &logical_rect);
  
  digit_height = PANGO_PIXELS (ink_rect.height) + 2;
  digit_offset = ink_rect.y;

  gtk_paint_box (widget->style, ruler->backing_store,
		 GTK_STATE_NORMAL, GTK_SHADOW_OUT, 
		 NULL, widget, "db_ruler",
		 0, 0, 
		 widget->allocation.width, widget->allocation.height);

  gdk_draw_line (ruler->backing_store, gc,
		 width + xthickness,
		 ythickness,
		 width + xthickness,
		 height - ythickness);

upper = ruler->upper / ruler->metric->pixels_per_unit;
lower = ruler->lower / ruler->metric->pixels_per_unit;

//   upper = ruler->upper;
//   lower = ruler->lower;

  if ((upper - lower) == 0) 
    return;

  increment = (gfloat) height / (upper - lower);
  abs_increment = (gfloat) fabs((double)increment);

// * strlen (unit_str)
text_height =  digit_height + 1;

  for (scale = 0; scale < MAXIMUM_SCALES; scale++)
 /*   if (ruler->metric->ruler_scale[scale] * fabs(increment) > 2 * text_height)
      break;*/
if (ruler_scale[scale] * abs_increment > 2 * digit_height)
      break;
  if (scale == MAXIMUM_SCALES)
    scale = MAXIMUM_SCALES - 1;

  /* drawing starts here */

  pango_layout_set_text (layout, "dB", 2);
  pango_layout_get_extents (layout, NULL, &logical_rect);

  gtk_paint_layout (widget->style,
                  ruler->backing_store,
                  GTK_WIDGET_STATE (widget),
				  FALSE,
                  NULL,
                  widget,
                  "vruler",
                  2,
                  (digit_height/2),
                  layout);

  length = 0;
  for (i = MAXIMUM_SUBDIVIDE - 1; i >= 0; i--)
    {
      subd_incr = (gfloat) ruler_scale[scale] / 
	          (gfloat) subdivide[i];
      if (subd_incr * fabs(increment) <= MINIMUM_INCR) 
	continue;

      /* Calculate the length of the tickmarks. Make sure that
       * this length increases for each set of ticks
       */
      ideal_length = 8 / (i + 1) - 1;
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
	  pos = height - ROUND ((cur - lower) * increment);

	  gdk_draw_line (ruler->backing_store, gc,
			 width + xthickness, pos,
			 width - length + xthickness, pos);

	  /* draw label */
	  if (i == 0 && cur < upper && cur > lower) {
	    float a_cur = fabs(cur), db_cur;
	    
	    /* ensure inf. stays as 'inf.', not nearby large values */
	    if (a_cur < subd_incr/2) a_cur = 0.0;
	    db_cur = 20 * log10 (a_cur);
	    
	    if (db_cur > -10.0) {
	      snprintf (unit_str, UNIT_STR_LEN, "%1.1f", db_cur);
	    } else {
	      snprintf (unit_str, UNIT_STR_LEN, "%2.0f", db_cur);
	    }
	  
		pango_layout_set_text (layout, unit_str, -1);

  		gtk_paint_layout (widget->style,
                  ruler->backing_store,
                  GTK_WIDGET_STATE (widget),
				  FALSE,
                  NULL,
                  widget,
                  "vruler",
                  2,
                  pos - (digit_height),
                  layout);
	    }
	}
	
    }
	
}

static void
db_ruler_draw_pos (GtkRuler *ruler)
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
  g_return_if_fail (GTK_IS_DB_RULER (ruler));

  if (GTK_WIDGET_DRAWABLE (ruler))
    {
      widget = GTK_WIDGET (ruler);

      gc = widget->style->fg_gc[GTK_STATE_NORMAL];
      xthickness = widget->style->xthickness;
      ythickness = widget->style->ythickness;
      width = widget->allocation.width - xthickness * 2;
      height = widget->allocation.height;

      bs_height = 7;
      bs_width = 4;

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

	  increment = (gfloat) height / (ruler->upper - ruler->lower);

	  x = (width - bs_width) + xthickness - 2;
	  y = DB_RULER(ruler)->y;

	  for (i = 0; i < bs_width; i++)
	    gdk_draw_line (widget->window, gc,
			   x + i, y + i,
			   x + i, y + bs_height - 1 - i);

	  ruler->xsrc = x;
	  ruler->ysrc = y;
	}
    }
}
