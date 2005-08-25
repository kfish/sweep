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

#ifndef __TIME_RULER_H__
#define __TIME_RULER_H__


#include <gdk/gdk.h>
#include <gtk/gtkruler.h>

#include <sweep/sweep_types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define TIME_RULER(obj)          GTK_CHECK_CAST (obj, time_ruler_get_type (), TimeRuler)
#define TIME_RULER_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, time_ruler_get_type (), TimeRulerClass)
#define GTK_IS_TIME_RULER(obj)       GTK_CHECK_TYPE (obj, time_ruler_get_type ())


typedef struct _TimeRuler       TimeRuler;
typedef struct _TimeRulerClass  TimeRulerClass;

struct _TimeRuler
{
  GtkRuler ruler;

  gint samplerate;
};

struct _TimeRulerClass
{
  GtkRulerClass parent_class;
};


GType      time_ruler_get_type (void);
GtkWidget* time_ruler_new      (void);
void       time_ruler_set_format (TimeRuler * time_ruler, sw_format * f);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __TIME_RULER_H__ */
