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
#include <string.h>
#include <errno.h>
#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_types.h>
#include <sweep/sweep_sample.h>

static GMutex * timeouts_mutex = NULL;

static GList * timeouts = NULL;

static guint sweep_tag = 0;

typedef struct {
  guint sweep_tag;
  guint gtk_tag;
  guint32 interval;
  GtkFunction function;
  gpointer data;
} sweep_timeout_data;

static gint
sweep_timeout_wrapper (gpointer data)
{
  sweep_timeout_data * td = (sweep_timeout_data *)data;

  if (td->function (td->data)) {
    return TRUE;
  } else {
    g_mutex_lock (timeouts_mutex);
    timeouts = g_list_remove (timeouts, td);
    g_mutex_unlock (timeouts_mutex);
    
    g_free (td);

    return FALSE;
  }
}

static gint
sweep_timeouts_handle_pending (gpointer data)
{
  GList * gl, * gl_next;
  sweep_timeout_data * td;

  g_mutex_lock (timeouts_mutex);

  for (gl = timeouts; gl; gl = gl_next) {
    td = (sweep_timeout_data *)gl->data;

    gl_next = gl->next;

    if (td->sweep_tag == -1) {
      g_assert (td->gtk_tag != -1);

      gtk_timeout_remove (td->gtk_tag);
      timeouts = g_list_remove_link (timeouts, gl);
      g_free (td);
    } else if (td->gtk_tag == -1) {
      td->gtk_tag = gtk_timeout_add (td->interval, sweep_timeout_wrapper, td);
    }
  }

  g_mutex_unlock (timeouts_mutex);

  return TRUE;
}

void
sweep_timeouts_init (void)
{
  if (timeouts_mutex == NULL)
    timeouts_mutex = g_mutex_new ();

  g_mutex_lock (timeouts_mutex); /* get some practice in ;) */
  if (timeouts != NULL) g_list_free (timeouts);
  timeouts = NULL;
  g_mutex_unlock (timeouts_mutex);

  gtk_timeout_add ((guint32)500, sweep_timeouts_handle_pending, NULL);
}

guint
sweep_timeout_add (guint32 interval, GtkFunction function, gpointer data)
{
  sweep_timeout_data * td;

  td = g_malloc (sizeof (sweep_timeout_data));

  td->sweep_tag = ++sweep_tag;
  td->gtk_tag = -1;
  td->interval = interval;
  td->function = function;
  td->data = data;

  g_mutex_lock (timeouts_mutex);
  timeouts = g_list_append (timeouts, td);
  g_mutex_unlock (timeouts_mutex);

  return td->sweep_tag;
}

void
sweep_timeout_remove (guint sweep_timeout_handler_id)
{
  GList * gl, * gl_next;
  sweep_timeout_data * td;

  if (sweep_timeout_handler_id == -1)
    return;

  g_mutex_lock (timeouts_mutex);

  for (gl = timeouts; gl; gl = gl_next) {
    td = (sweep_timeout_data *)gl->data;

    gl_next = gl->next;

    if (td->sweep_tag == sweep_timeout_handler_id) {
      if (td->gtk_tag == -1) {
	/* gtk_timeout not yet started */
	timeouts = g_list_remove_link (timeouts, gl);
	g_free (td);
      } else {
	/* need to remove gtk_timeout -- mark this for pending thread */
	td->sweep_tag = -1;
      }
      break;
    }
  }

  g_mutex_unlock (timeouts_mutex);
}
