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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_types.h>
#include <sweep/sweep_sample.h>

#include "interface.h"

#include "db_slider.h"

enum {
  VALUE_CHANGED_SIGNAL,
  LAST_SIGNAL
};

static gint db_slider_signals[LAST_SIGNAL] = { 0 };

static void
db_slider_class_init(DbSliderClass * class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass *) class;

  db_slider_signals[VALUE_CHANGED_SIGNAL] =
    gtk_signal_new("value-changed", GTK_RUN_FIRST, object_class->type,
                   GTK_SIGNAL_OFFSET(DbSliderClass, value_changed),
                   gtk_marshal_NONE__INT, GTK_TYPE_NONE, 1, GTK_TYPE_FLOAT);

  gtk_object_class_add_signals(object_class, db_slider_signals, LAST_SIGNAL);

  class->value_changed = NULL;
}

static void
db_slider_init (GtkWidget * slider)
{
}

guint
db_slider_get_type()
{
  static guint db_slider_type = 0;

  if (!db_slider_type) {
    GtkTypeInfo db_slider_info =
    {
      "DbSlider",
      sizeof(DbSlider),
      sizeof(DbSliderClass),
      (GtkClassInitFunc) db_slider_class_init,
      (GtkObjectInitFunc) db_slider_init,
      (GtkArgSetFunc) NULL,
      (GtkArgGetFunc) NULL,
    };

    db_slider_type = gtk_type_unique(gtk_event_box_get_type(),
				     &db_slider_info);
  }

  return db_slider_type;
}

#if 0
static int
slider_get_value (GtkWidget * slider)
{
  return
    GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT(slider), "value"));
}

static int
slider_set_value (GtkWidget * slider, int value)
{
  GtkWidget * combo_entry;
  int i;

  combo_entry =
    GTK_WIDGET (gtk_object_get_data (GTK_OBJECT(slider), "combo_entry"));

  for (i = 0; choices[i].name != NULL; i++) {
    if (value == choices[i].value) {
      gtk_entry_set_text (GTK_ENTRY(combo_entry), choices[i].name);
      return value;
    }
  }

  /* not in the entry -- assume first choice is "Custom" and set that */
  gtk_entry_set_text (GTK_ENTRY(combo_entry), choices[0].name);

  return slider_set_value_direct (slider, value);;
}
#endif

#define VALUE_TO_DB(v) (20 * log10(v))
#define DB_TO_VALUE(d) (pow (10, ((d) / 20)))

#define SQR(x) ((x)*(x))
#define ADJ_TO_VALUE(a) SQR((100.0 - (a)) / 100.0)
#define VALUE_TO_ADJ(v) ((1.0 - sqrt(v)) * 100.0)


gfloat
db_slider_get_value (DbSlider * slider)
{
  return ADJ_TO_VALUE(GTK_ADJUSTMENT(slider->adj)->value);
}

void
db_slider_set_value (DbSlider * slider, gfloat value)
{
  gtk_adjustment_set_value (GTK_ADJUSTMENT(slider->adj),
			    VALUE_TO_ADJ(value));
}

static void
db_slider_value_changed_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * slider = (GtkWidget *)data;
  gfloat value, db_value;
  gchar * db_text;

  value = db_slider_get_value (DB_SLIDER(slider));
  db_value = VALUE_TO_DB (value);

  if (db_value > -10.0) {
    db_text = g_strdup_printf ("%1.1f dB", db_value);
  } else {
    db_text = g_strdup_printf ("%2.0f dB", db_value);
  }

  gtk_label_set_text (GTK_LABEL(DB_SLIDER(slider)->db_label), db_text);

  g_free (db_text);

  gtk_signal_emit (GTK_OBJECT(slider), db_slider_signals[VALUE_CHANGED_SIGNAL],
		   value);
}

static void
db_slider_build (GtkWidget * slider, gchar * title, gfloat value)
{
  GtkWidget * vbox;
  GtkWidget * label;
  GtkWidget * vscale;

  GtkObject * adj;

  gchar * range_text;

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(slider), vbox);
  gtk_widget_show (vbox);

  label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  DB_SLIDER(slider)->db_label = label;

  adj = gtk_adjustment_new (VALUE_TO_ADJ(value), /* value */
			    VALUE_TO_ADJ(DB_SLIDER(slider)->upper), /* lower */
			    VALUE_TO_ADJ(DB_SLIDER(slider)->lower), /* upper */
			    0.3,  /* step incr */
			    3.0,  /* page incr */
			    0.0   /* page size */
			    );

  vscale = gtk_vscale_new (GTK_ADJUSTMENT(adj));
  gtk_scale_set_draw_value (GTK_SCALE(vscale), FALSE);
  gtk_range_set_update_policy (GTK_RANGE(vscale), GTK_UPDATE_CONTINUOUS);
  gtk_widget_set_usize (vscale, -1, gdk_screen_height() / 8);
  gtk_box_pack_start (GTK_BOX(vbox), vscale, TRUE, TRUE, 0);
  gtk_widget_show (vscale);

  gtk_signal_connect (GTK_OBJECT(adj), "value_changed",
		      GTK_SIGNAL_FUNC(db_slider_value_changed_cb), slider);

  DB_SLIDER(slider)->adj = adj;

  db_slider_value_changed_cb (NULL, slider);

  range_text = g_strdup_printf ("%s\n[%2.0f to %2.0f dB]",
				title,
				VALUE_TO_DB(DB_SLIDER(slider)->lower),
				VALUE_TO_DB(DB_SLIDER(slider)->upper));

  label = gtk_label_new (range_text);
  gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);  
}


GtkWidget *
db_slider_new (gchar * title, gfloat value, gfloat lower, gfloat upper)
{
  DbSlider * slider = DB_SLIDER (gtk_type_new (db_slider_get_type ()));

  slider->lower = lower;
  slider->upper = upper;

  db_slider_build (GTK_WIDGET (slider), title, value);

  return GTK_WIDGET (slider);
}
