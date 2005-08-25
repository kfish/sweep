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

#ifndef __LEVELMETER_H__
#define __LEVELMETER_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LEVELMETER(obj)	GTK_CHECK_CAST(obj, levelmeter_get_type (), LevelMeter)
#define LEVELMETER_CLASS(klass)	GTK_CHECK_CLASS_CAST(klass, levelmeter_get_type (), LevelMeterClass)
#define IS_LEVELMETER(obj) GTK_CHECK_TYPE(obj, levelmeter_get_type())

typedef struct _LevelMeter LevelMeter;
typedef struct _LevelMeterClass LevelMeterClass;

struct _LevelMeter {
  GtkWidget widget;

  guint level;
};

struct _LevelMeterClass {
  GtkWidgetClass parent_class;
};

GtkWidget *levelmeter_new(guint state);
GType levelmeter_get_type(void);
guint levelmeter_get_level(LevelMeter * levelmeter);
void levelmeter_set_level(LevelMeter * levelmeter, guint level);

#ifdef __cplusplus
}
#endif

#endif /* __LEVELMETER_H__ */
