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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>

#include "sweep_types.h"
#include "file_ops.h"
#include "sweep_sample.h"
#include "sample-display.h"

static gchar* load_current_file = NULL; /* last visited dir for loading */
static gchar* save_current_file = NULL; /* last visited dir for saving */

static void
sample_load_ok_cb(GtkWidget * widget, gpointer data)
{
  sw_sample * s;
  sw_view * v;
  gchar *dir;

  dir = gtk_file_selection_get_filename(GTK_FILE_SELECTION(data));
  g_free(load_current_file);
  load_current_file = g_strdup(dir);

  s = sample_load(dir);
  if(s) {
      v = view_new_all (s, 1.0);
      sample_add_view (s, v);
      sample_bank_add(s);
  }

  gtk_widget_destroy(GTK_WIDGET(data));
}

static void
sample_load_cancel_cb(GtkWidget * widget, gpointer data)
{
  gtk_widget_destroy(GTK_WIDGET(data));
}

void
sample_load_cb(GtkWidget * wiget, gpointer data)
{
  GtkWidget *filesel;

  filesel = gtk_file_selection_new("Load Sample");
  if(load_current_file) {
      gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel), 
				      load_current_file);
  }

  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filesel)->ok_button),
		     "clicked", GTK_SIGNAL_FUNC(sample_load_ok_cb), filesel);

  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filesel)->cancel_button),
		"clicked", GTK_SIGNAL_FUNC(sample_load_cancel_cb), filesel);

  gtk_widget_show(filesel);
}

void
sample_revert_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * s = SAMPLE_DISPLAY(data);
  sw_sample * sample, * orig_sample;
  char pathname [SW_DIR_LEN];

  sample = s->view->sample;
  snprintf (pathname, SW_DIR_LEN,
	    "%s/%s", sample->directory,
	    sample->filename);

  g_print ("Attempting to revert >>%s<<\n", pathname);

  orig_sample = sample_load (pathname);

  g_print ("Loaded into %p\n", orig_sample);

  sample_replace_throughout (sample, orig_sample);

  sample_refresh_views (orig_sample);
}

static void
sample_save_as_ok_cb(GtkWidget * widget, gpointer data)
{
  SampleDisplay * s = SAMPLE_DISPLAY(data);
  sw_sample * sample;
  GtkWidget * filesel;
  gchar *dn, * fn;

  sample = s->view->sample;
  filesel = gtk_widget_get_toplevel (widget);

  dn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel));
  save_current_file = g_strdup(dn);

  /* remove filename from dir */
  fn = strrchr (dn, '/');
  if (fn) {
    *fn++ = '\0';
  } else {
    fn = dn;
    dn = NULL;
  }

  sample_set_pathname (sample, dn, fn);

  g_free(save_current_file);
  sample_save(sample);

  gtk_widget_destroy(GTK_WIDGET(filesel));
}

static void
sample_save_as_cancel_cb(GtkWidget * widget, gpointer data)
{
  gtk_widget_destroy(GTK_WIDGET(data));
}

void
sample_save_as_cb(GtkWidget * widget, gpointer data)
{
  SampleDisplay * s = SAMPLE_DISPLAY(data);
  sw_sample * sample;
  GtkWidget * filesel;

  sample = s->view->sample;

  chdir(sample->directory);

  filesel = gtk_file_selection_new("Save Sample");
  if(save_current_file)
      gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel), 
				      save_current_file);

  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filesel)->ok_button),
		     "clicked", GTK_SIGNAL_FUNC(sample_save_as_ok_cb), data);

  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filesel)->cancel_button),
		"clicked", GTK_SIGNAL_FUNC(sample_save_as_cancel_cb), filesel);

  gtk_widget_show(filesel);
}

void
sample_save_cb(GtkWidget * widget, gpointer data)
{
  SampleDisplay * s = SAMPLE_DISPLAY(data);
  sw_sample * sample;

  sample = s->view->sample;

  if (!sample->filename) {
    sample_save_as_cb (widget, data);
  } else {
    sample_save (sample);
  }
}
