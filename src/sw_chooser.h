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

#ifndef __SW_CHOOSER_H__
#define __SW_CHOOSER_H__

#define SW_CHOOSER(obj) GTK_CHECK_CAST (obj, sw_chooser_get_type (), SWChooser)
#define SW_CHOOSER_CLASS(klass) \
  GTK_CHECK_CLASS_CAST (klass, sw_chooser_get_type (), SWChooserClass)
#define IS_SW_CHOOSER(obj) GTK_CHECK_TYPE (obj, sw_chooser_get_type ())

typedef struct _SWChooser SWChooser;
typedef struct _SWChooserClass SWChooserClass;

struct _SWChooser {
  GtkFrame frame;

  gchar * title;
  gchar * units;
  gpointer choices;
};

struct _SWChooserClass {
  GtkFrameClass parent_class;
  
  void (*number_changed) (SWChooser * chooser, int number);
};

GtkType sw_chooser_get_type (void);

GtkWidget * samplerate_chooser_new (gchar * title);
int samplerate_chooser_get_rate (GtkWidget * chooser);
int samplerate_chooser_set_rate (GtkWidget * chooser, int rate);

GtkWidget * channelcount_chooser_new (gchar * title);
int channelcount_chooser_get_count (GtkWidget * chooser);
int channelcount_chooser_set_count (GtkWidget * chooser, int count);

#endif /* __SW_CHOOSER_H__ */
