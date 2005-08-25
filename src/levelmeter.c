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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtk.h>

#include "levelmeter.h"

#define LEVELMETER_DEFAULT_WIDTH 4
#define LEVELMETER_DEFAULT_HEIGHT 20

/* Number of divisions */
#define LEVELMETER_DEFAULT_DIVISIONS 10

/* Start of 'high' [red] divisions */
#define LEVELMETER_DEFAULT_HIGH 7

/* Local data */

static GtkWidgetClass *parent_class = NULL;

static GdkColor col_red, col_green;
static GtkStyle *levelmeter_style = NULL;


guint
levelmeter_get_level(LevelMeter * levelmeter)
{
  g_return_val_if_fail(levelmeter != NULL, 0);
  g_return_val_if_fail(IS_LEVELMETER(levelmeter), 0);

  return levelmeter->level;
}

void
levelmeter_set_level(LevelMeter * levelmeter, guint level)
{
  g_return_if_fail(levelmeter != NULL);
  g_return_if_fail(IS_LEVELMETER(levelmeter));

  levelmeter->level = level;
/*
  gtk_widget_queue_draw_area(GTK_WIDGET(levelmeter), 
									        gint x,
                                             gint y,
                                             gint width,
                                             gint height) */
}

static void
levelmeter_realize(GtkWidget * widget)
{
  LevelMeter *levelmeter;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(IS_LEVELMETER(widget));

  GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
  levelmeter = LEVELMETER(widget);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;	/* XXX */
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = gtk_widget_get_events(widget) |
    GDK_EXPOSURE_MASK
#if 0
    | GDK_BUTTON_PRESS_MASK |
    GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
    GDK_POINTER_MOTION_HINT_MASK
#endif
    ;
  attributes.visual = gtk_widget_get_visual(widget);
  attributes.colormap = gtk_widget_get_colormap(widget);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new(widget->parent->window, &attributes,
				  attributes_mask);

  widget->style = gtk_style_attach(widget->style, widget->window);

  gdk_window_set_user_data(widget->window, widget);

  gtk_style_set_background(widget->style, widget->window, GTK_STATE_ACTIVE);
}

static void
levelmeter_size_request(GtkWidget * widget, GtkRequisition * requisition)
{
  requisition->width = LEVELMETER_DEFAULT_WIDTH;
  requisition->height = LEVELMETER_DEFAULT_HEIGHT;
}

static void
levelmeter_size_allocate(GtkWidget * widget, GtkAllocation * allocation)
{
  LevelMeter *levelmeter;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(IS_LEVELMETER(widget));
  g_return_if_fail(allocation != NULL);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED(widget)) {
    levelmeter = LEVELMETER(widget);

    gdk_window_move_resize(widget->window,
       allocation->x, allocation->y, allocation->width, allocation->height);
  }
}

static void
levelmeter_draw (GtkWidget * widget, GdkRectangle * area)
{
  gint i, levelmeter_height;

  levelmeter_height = widget->allocation.height / LEVELMETER_DEFAULT_DIVISIONS;

  gdk_window_clear_area(widget->window, 0, 0,
			widget->allocation.width, widget->allocation.height);

  if (!levelmeter_style) {
    levelmeter_style = gtk_style_new();
    levelmeter_style->fg_gc[GTK_STATE_NORMAL] =
      gdk_gc_new(widget->window);
  }
  gdk_gc_set_foreground(levelmeter_style->fg_gc[GTK_STATE_NORMAL], &col_green);

  for (i = 0; i < LEVELMETER_DEFAULT_HIGH && i <= LEVELMETER(widget)->level; i++) {
    gdk_draw_rectangle(widget->window, levelmeter_style->fg_gc[widget->state],
		       TRUE, 0, (LEVELMETER_DEFAULT_DIVISIONS - i) * levelmeter_height, widget->allocation.width, levelmeter_height - 1);
  }

  gdk_gc_set_foreground(levelmeter_style->fg_gc[GTK_STATE_NORMAL], &col_red);

  for (; i <= LEVELMETER_DEFAULT_DIVISIONS && i <= LEVELMETER(widget)->level;
       i++) {
    gdk_draw_rectangle(widget->window, levelmeter_style->fg_gc[widget->state],
		       TRUE, 0, (LEVELMETER_DEFAULT_DIVISIONS - i) * levelmeter_height, widget->allocation.width, levelmeter_height - 1);
  }
}

static gint
levelmeter_expose(GtkWidget * widget, GdkEventExpose * event)
{
 //@@ gtk_widget_draw (widget, NULL);
  levelmeter_draw(widget, NULL);
	//@@ fix this

  return FALSE;
}


static void
levelmeter_destroy(GtkObject * object)
{
  LevelMeter *levelmeter;

  g_return_if_fail(object != NULL);
  g_return_if_fail(IS_LEVELMETER(object));

  levelmeter = LEVELMETER(object);

  /* unref contained widgets */

  if (GTK_OBJECT_CLASS(parent_class)->destroy)
    (*GTK_OBJECT_CLASS(parent_class)->destroy) (object);
}

static void
levelmeter_class_init(LevelMeterClass * class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;

  parent_class = gtk_type_class(gtk_widget_get_type());

  object_class->destroy = levelmeter_destroy;

  widget_class->realize = levelmeter_realize;
  widget_class->expose_event = levelmeter_expose;
 //@@ widget_class->draw = levelmeter_draw;
  widget_class->size_request = levelmeter_size_request;
  widget_class->size_allocate = levelmeter_size_allocate;
/*
   widget_class->button_press_event = levelmeter_button_press;
   widget_class->button_release_event = levelmeter_button_release;
   widget_class->motion_notify_event = levelmeter_motion_notify;
 */

  col_green.red = 0xFFFF;
  col_green.green = 0xFFFF;
  col_green.blue = 0;
  gdk_colormap_alloc_color        (gdk_colormap_get_system(),
                                             &col_green,
                                             TRUE,
                                             TRUE);
  col_red.red = 0xFFFF;
  col_red.green = 0;
  col_red.blue = 0;
  gdk_colormap_alloc_color        (gdk_colormap_get_system(),
                                             &col_red,
                                             TRUE,
                                             TRUE);

}

static void
levelmeter_init(LevelMeter * levelmeter)
{
  levelmeter->level = 0;
}

GType
levelmeter_get_type()
{
  static GType levelmeter_type = 0;

  if (!levelmeter_type) {
    static const GTypeInfo levelmeter_info =
    {
		  
      sizeof(LevelMeterClass),
	NULL, /* base_init */
	NULL, /* base_finalize */
	(GClassInitFunc) levelmeter_class_init,
	NULL, /* class_finalize */
	NULL, /* class_data */
	sizeof (LevelMeter),
	0,    /* n_preallocs */
	(GInstanceInitFunc) levelmeter_init,	  

    };

      levelmeter_type = g_type_register_static (GTK_TYPE_WIDGET, "LevelMeter", &levelmeter_info, 0);
  }
	
  return levelmeter_type;
}

GtkWidget *
levelmeter_new(guint level)
{
  LevelMeter *levelmeter;

    levelmeter = LEVELMETER (g_object_new (levelmeter_get_type (), NULL));	
  levelmeter_set_level(levelmeter, level);

  return GTK_WIDGET(levelmeter);
}
