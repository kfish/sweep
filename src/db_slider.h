/*
 * Sweep, a sound wave editor.
 *
 * Copyright (C) 2000 Conrad Parker
 * Copyright (C) 2002 Commonwealth Scientific and Industrial Research
 * Organisation (CSIRO), Australia
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

#ifndef __DB_SLIDER_H__
#define __DB_SLIDER_H__

#define DB_SLIDER(obj) GTK_CHECK_CAST (obj, db_slider_get_type (), DbSlider)
#define DB_SLIDER_CLASS(klass) \
  G_TYPE_CHECK_CLASS_CAST (klass, db_slider_get_type (), DbSliderClass)
#define IS_DB_SLIDER(obj) GTK_CHECK_TYPE (obj, db_slider_get_type ())

typedef struct _DbSlider DbSlider;
typedef struct _DbSliderClass DbSliderClass;

struct _DbSlider {
  GtkEventBox ebox;

  GtkObject * adj;
  GtkWidget * db_label;

  gfloat lower;
  gfloat upper;
};

struct _DbSliderClass {
  GtkVBoxClass parent_class;
  
  void (*value_changed) (DbSlider * slider, gfloat value);
};

GType db_slider_get_type (void);
GtkWidget * db_slider_new (gchar * title, gfloat value, gfloat lower,
			   gfloat upper);
gfloat db_slider_get_value (DbSlider * slider);
void db_slider_set_value (DbSlider * slider, gfloat value);

#endif /* __DB_SLIDER_H__ */
