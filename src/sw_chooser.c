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

#include "sw_chooser.h"

typedef struct {
  int number;
  char * name;
} sw_choice;

/* first choice must be _("Custom") */

static sw_choice samplerate_choices[] = {
  {     -1, N_("Custom") },
  { 192000, N_("192000 Hz (Studio quality)") },
  {  96000, N_(" 96000 Hz (High quality)") },
  {  48000, N_(" 48000 Hz (DAT quality)") },
  {  44100, N_(" 44100 Hz (CD quality)") },
  {  32000, N_(" 32000 Hz (Ultra-wideband voice quality)") },
  {  22050, N_(" 22050 Hz") },
  {  16000, N_(" 16000 Hz (Wideband voice quality)") },
  {  11025, N_(" 11025 Hz") },
  {   8000, N_("  8000 Hz (Narrowband voice quality)") },
  {   4000, N_("  4000 Hz (Low quality)") },
  {      0, NULL }
};

static sw_choice channelcount_choices[] = {
  { -1, N_("Custom") },
  {  1, N_("Mono") },
  {  2, N_("Stereo") },
  {  4, N_("Quadraphonic") },
  /*  {  6, N_("5.1") },*/
  {  0, NULL }
};

enum {
  NUMBER_CHANGED_SIGNAL,
  LAST_SIGNAL
};

static gint sw_chooser_signals[LAST_SIGNAL] = { 0 };

static void
sw_chooser_class_init(SWChooserClass * class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass *) class;

  sw_chooser_signals[NUMBER_CHANGED_SIGNAL] =
    gtk_signal_new("number-changed", GTK_RUN_FIRST, object_class->type,
                   GTK_SIGNAL_OFFSET(SWChooserClass, number_changed),
                   gtk_marshal_NONE__INT, GTK_TYPE_NONE, 1, GTK_TYPE_INT);

  gtk_object_class_add_signals(object_class, sw_chooser_signals, LAST_SIGNAL);

  class->number_changed = NULL;
}

static void
sw_chooser_init (GtkWidget * chooser)
{
}

guint
sw_chooser_get_type()
{
  static guint sw_chooser_type = 0;

  if (!sw_chooser_type) {
    GtkTypeInfo sw_chooser_info =
    {
      "SWChooser",
      sizeof(SWChooser),
      sizeof(SWChooserClass),
      (GtkClassInitFunc) sw_chooser_class_init,
      (GtkObjectInitFunc) sw_chooser_init,
      (GtkArgSetFunc) NULL,
      (GtkArgGetFunc) NULL,
    };

    sw_chooser_type = gtk_type_unique(gtk_frame_get_type(), &sw_chooser_info);
  }

  return sw_chooser_type;
}


static int
chooser_get_number (GtkWidget * chooser)
{
  return
    GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT(chooser), "number"));
}

static int
chooser_set_number_direct (GtkWidget * chooser, int number)
{
  GtkWidget * direct_entry;
#define BUF_LEN 16
  char buf[BUF_LEN];

  direct_entry =
    GTK_WIDGET (gtk_object_get_data (GTK_OBJECT(chooser), "direct_entry"));

  /*
   * Print the number in the direct_entry, but leave it blank for zero.
   * (otherwise, if zero is printed as '0', the GtkEntry behaves wierdly).
   */
  if (number > 0) {
    snprintf (buf, BUF_LEN, "%d", number);
  } else {
    buf[0] = '\0';
  }

  gtk_entry_set_text (GTK_ENTRY(direct_entry), buf);

  gtk_object_set_data (GTK_OBJECT(chooser), "number",
		       GINT_TO_POINTER(number));

  gtk_signal_emit (GTK_OBJECT(chooser),
		   sw_chooser_signals[NUMBER_CHANGED_SIGNAL], number);

  return number;
}

static int
chooser_set_number (GtkWidget * chooser, int number, sw_choice * choices)
{
  GtkWidget * combo_entry;
  int i;

  combo_entry =
    GTK_WIDGET (gtk_object_get_data (GTK_OBJECT(chooser), "combo_entry"));

  for (i = 0; choices[i].name != NULL; i++) {
    if (number == choices[i].number) {
      gtk_entry_set_text (GTK_ENTRY(combo_entry), _(choices[i].name));
      return number;
    }
  }

  /* not in the entry -- assume first choice is "Custom" and set that */
  gtk_entry_set_text (GTK_ENTRY(combo_entry), _(choices[0].name));

  return chooser_set_number_direct (chooser, number);;
}

static void
chooser_combo_changed_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * chooser = (GtkWidget *)data;
  sw_choice * choices;
  GtkWidget * direct_hbox;
  gchar * text;
  int i, number = -1;

  /* find what number the chosen menu string corresponds to */
  
  text = gtk_entry_get_text (GTK_ENTRY(widget));

  choices = gtk_object_get_data (GTK_OBJECT(chooser), "choices");

  for (i = 0; choices[i].name != NULL; i++) {
    if (strcmp (text, _(choices[i].name)) == 0) {
      number = choices[i].number;
      break;
    }
  }

  /* set the direct hbox sensitive if "Custom", else insensitive */

  direct_hbox =
    GTK_WIDGET(gtk_object_get_data (GTK_OBJECT(chooser), "direct_hbox"));

  if (number == -1) {
    gtk_widget_set_sensitive (direct_hbox, TRUE);
  } else {
    gtk_widget_set_sensitive (direct_hbox, FALSE);

    /* change the direct_entry to reflect the new value */
    chooser_set_number_direct (chooser, number);
  }
}

static void
chooser_entry_changed_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * chooser = (GtkWidget *)data;
  GtkWidget * combo_entry;
  sw_choice * choices;

  gchar * text;
  int number = -1;

  text = gtk_entry_get_text (GTK_ENTRY(widget));
  number = atoi (text);

  choices = gtk_object_get_data (GTK_OBJECT(chooser), "choices");

  combo_entry =
    GTK_WIDGET (gtk_object_get_data (GTK_OBJECT(chooser), "combo_entry"));

  gtk_signal_handler_block_by_data (GTK_OBJECT(widget), chooser);
  gtk_signal_handler_block_by_data (GTK_OBJECT(combo_entry), chooser);

  chooser_set_number (chooser, number, choices);

  gtk_signal_handler_unblock_by_data (GTK_OBJECT(combo_entry), chooser);
  gtk_signal_handler_unblock_by_data (GTK_OBJECT(widget), chooser);

}

static void
sw_chooser_build (GtkWidget * chooser)
{
  GtkWidget * frame;
  GtkWidget * vbox;  
  GList * choice_names = NULL;
  GtkWidget * combo;
  GtkWidget * hbox;
  GtkWidget * combo_entry, * direct_entry;
  GtkWidget * label;
  sw_choice * choices;

  int i;

  frame = chooser;
  gtk_frame_set_label (GTK_FRAME (frame), SW_CHOOSER(chooser)->title);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(frame), vbox);
  gtk_container_set_border_width (GTK_CONTAINER(vbox), 8);
  gtk_widget_show (vbox);

  choices = (sw_choice *) (SW_CHOOSER(chooser)->choices);

  choice_names = NULL;
  for (i = 0; choices[i].name != NULL; i++) {
    choice_names = g_list_append (choice_names, _(choices[i].name));
  }
  
  combo = gtk_combo_new ();
  gtk_combo_set_popdown_strings (GTK_COMBO(combo), choice_names);
  gtk_combo_set_value_in_list (GTK_COMBO(combo), TRUE, FALSE);

  combo_entry = GTK_COMBO(combo)->entry;

  gtk_entry_set_editable (GTK_ENTRY(combo_entry), FALSE);

  gtk_box_pack_start (GTK_BOX (vbox), combo, TRUE, FALSE, 4);
  gtk_widget_show (combo);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Custom: "));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
  gtk_widget_show (label);

  direct_entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), direct_entry, FALSE, FALSE, 4);
  gtk_widget_show (direct_entry);

  if (SW_CHOOSER(chooser)->units != NULL) {
    label = gtk_label_new (SW_CHOOSER(chooser)->units);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
    gtk_widget_show (label);
  }

  gtk_signal_connect (GTK_OBJECT(combo_entry), "changed",
		      GTK_SIGNAL_FUNC(chooser_combo_changed_cb), chooser);
  
  gtk_signal_connect (GTK_OBJECT(direct_entry), "changed",
		      GTK_SIGNAL_FUNC(chooser_entry_changed_cb), chooser);

  gtk_object_set_data (GTK_OBJECT(chooser), "combo_entry", combo_entry);
  gtk_object_set_data (GTK_OBJECT(chooser), "direct_entry", direct_entry);
  gtk_object_set_data (GTK_OBJECT(chooser), "direct_hbox", hbox);
  gtk_object_set_data (GTK_OBJECT(chooser), "choices", choices);

  /* fake a change event to set the number data */
  chooser_combo_changed_cb (combo_entry, chooser);
}


GtkWidget *
samplerate_chooser_new (gchar * title)
{
  SWChooser * chooser = SW_CHOOSER (gtk_type_new (sw_chooser_get_type ()));

  chooser->title = title ? title : _("Sampling rate");
  chooser->choices = (gpointer)samplerate_choices;
  chooser->units = _("Hz");

  sw_chooser_build (GTK_WIDGET (chooser));

  return GTK_WIDGET (chooser);
}

int
samplerate_chooser_get_rate (GtkWidget * chooser)
{
  return chooser_get_number (chooser);
}

int
samplerate_chooser_set_rate (GtkWidget * chooser, int rate)
{
  return chooser_set_number (chooser, rate, samplerate_choices);
}

GtkWidget *
channelcount_chooser_new (gchar * title)
{
  SWChooser * chooser = SW_CHOOSER (gtk_type_new (sw_chooser_get_type ()));

  chooser->title = title ? title : _("Channels");
  chooser->choices = channelcount_choices;
  chooser->units = _("channels");

  sw_chooser_build (GTK_WIDGET (chooser));

  return GTK_WIDGET (chooser);
}

int
channelcount_chooser_get_count (GtkWidget * chooser)
{
  return chooser_get_number (chooser);
}

int
channelcount_chooser_set_count (GtkWidget * chooser, int count)
{
  return chooser_set_number (chooser, count, channelcount_choices);
}
