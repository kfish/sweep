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
#include <glib.h>


#include "sample.h"
#include "sweep_typeconvert.h"
#include "sweep_sounddata.h"
#include "sweep_selection.h"
#include "format.h"
#include "view.h"
#include "sweep_undo.h"
#include "sample-display.h"
#include "driver.h"

extern sw_view * last_tmp_view;

static GList * sample_bank = NULL;


/* Sample functions */

void
sample_add_view (sw_sample * s, sw_view * v)
{
 s->views = g_list_append(s->views, v);
}

void
sample_remove_view (sw_sample * s, sw_view * v)
{
 s->views = g_list_remove(s->views, v);

 if(!s->views) {
   sample_bank_remove(s);
 }
}

sw_sample *
sample_new_empty(char * directory, char * filename,
		 gint nr_channels, gint sample_rate, gint sample_length)
{
  sw_sample *s;

  s = g_malloc (sizeof(sw_sample));
  if (!s)
    return NULL;

  s->sounddata = sounddata_new_empty (nr_channels, sample_rate, sample_length);

  s->views = NULL;

  if (directory)
    strncpy(s->directory, directory, MIN(SW_DIR_LEN, strlen(directory)));
  else
    s->directory[0] = '\0';

  if (filename)
    s->filename = strdup(filename);
  else
    s->filename = NULL;

  s->registered_ops = NULL;
  s->current_undo = NULL;
  s->current_redo = NULL;

  s->tmp_sel = NULL;

  return s;
}

sw_sample *
sample_new_copy(sw_sample * s)
{
  sw_sample * sn;

  sn = sample_new_empty(s->directory,
			s->filename,
			s->sounddata->format->channels,
			s->sounddata->format->rate,
			s->sounddata->nr_frames);

  if(!sn) {
    fprintf(stderr, "Unable to allocate new sample.\n");
    return NULL;
  }

  memcpy(sn->sounddata->data, s->sounddata->data,
	 frames_to_bytes(s->sounddata->format, s->sounddata->nr_frames));

  sounddata_copyin_selection (s->sounddata, sn->sounddata);

  return sn;
}

void
sample_destroy (sw_sample * s)
{
  sample_stop_playback (s);

  /* playback holds this mutex, so wait on it before
   * destroying the sounddata.
   */
  g_mutex_lock (s->sounddata->sels_mutex);

  sounddata_destroy (s->sounddata);

  /* XXX: Should do this: */
  /* trim_registered_ops (s, 0); */

  g_free (s);
}

void
sample_set_pathname (sw_sample * s, char * directory, char * filename)
{
  gchar * tmp;
  sw_view * v;
  GList * gl;

  if (directory)
    strncpy(s->directory, directory, MIN(SW_DIR_LEN, strlen(directory)));
  else
    s->directory[0] = '\0';

  if (filename) {
    /* in case filename == s->filename, as in when
     * the default "save" function is called
     */
    tmp = strdup (filename);
    if (s->filename) g_free (s->filename);
    s->filename = tmp;
  } else {
    if (s->filename) g_free (s->filename);
    s->filename = NULL;
  }

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    view_refresh_title (v);
  }
}

void
sample_bank_add (sw_sample * s)
{
  /* Check that sample is not already in sample_bank */
  if (g_list_find (sample_bank, s)) return;

  sample_bank = g_list_append (sample_bank, s);
}

/*
 * sample_bank_remove (s)
 *
 * Takes a sample out of the sample list.
 */
void
sample_bank_remove (sw_sample * s)
{
  sample_destroy(s);
  sample_bank = g_list_remove(sample_bank, s);
}

void
sample_refresh_views (sw_sample * s)
{
  sw_view * v;
  GList * gl;

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    view_refresh (v);
  }
}

void
sample_start_marching_ants (sw_sample * s)
{
  sw_view * v;
  GList * gl;

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    sample_display_start_marching_ants (SAMPLE_DISPLAY(v->display));
  }
}

void
sample_stop_marching_ants (sw_sample * s)
{
  sw_view * v;
  GList * gl;

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    sample_display_stop_marching_ants (SAMPLE_DISPLAY(v->display));
  }
}

void
sample_set_playmarker (sw_sample * s, int offset)
{
  sw_view * v;
  GList * gl;

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    view_set_playmarker (v, offset);
  }
}

/*
 * sample_replace_throughout (os, s)
 *
 * Replaces os with s throughout the program, ie:
 *   - in the sample bank
 *   - in all os's views
 *
 * Destroys os.
 *
 * This function is not needed in general filters because sw_sample
 * pointers are persistent across sounddata modifications.
 * However, this function is still required where the entire sample
 * changes but the view must stay the same, eg. for File->Revert.
 */
void
sample_replace_throughout (sw_sample * os, sw_sample * s)
{
  sw_view * v;
  GList * gl;

  if (os == s) return;

  if ((!os) || (!s)) return;

  s->views = os->views;

  for(gl = s->views; gl; gl = gl->next) {
    v = (sw_view *)gl->data;
    v->sample = s;
    /* view_refresh (v); */
  }

  sample_bank_remove (os);
  sample_bank_add (s);
}


guint
sample_sel_nr_regions (sw_sample * s)
{
  return g_list_length (s->sounddata->sels);
}

void
sample_clear_selection (sw_sample * s)
{
  sounddata_clear_selection (s->sounddata);

  sample_stop_marching_ants (s);
}

static void
sample_normalise_selection (sw_sample * s)
{
  sounddata_normalise_selection (s->sounddata);
}

void
sample_add_selection (sw_sample * s, sw_sel * sel)
{
  if (!s->sounddata->sels)
    sample_start_marching_ants (s);

  sounddata_add_selection (s->sounddata, sel);
}

sw_sel *
sample_add_selection_1 (sw_sample * s, sw_framecount_t start, sw_framecount_t end)
{
  return sounddata_add_selection_1 (s->sounddata, start, end);
}

void
sample_set_selection (sw_sample * s, GList * gl)
{
  sample_clear_selection(s);

  s->sounddata->sels = sels_copy (gl);
}

sw_sel *
sample_set_selection_1 (sw_sample * s, sw_framecount_t start, sw_framecount_t end)
{
  return sounddata_set_selection_1 (s->sounddata, start, end);
}

void
sample_selection_modify (sw_sample * s, sw_sel * sel,
			 sw_framecount_t new_start, sw_framecount_t new_end)
{
  sel->sel_start = new_start;
  sel->sel_end = new_end;

  sample_normalise_selection (s);
}

static void
ss_invert (sw_sample * s, sw_param_set unused, gpointer unused2)
{
  GList * gl;
  GList * osels;
  sw_sel * osel, * sel;

  g_mutex_lock (s->sounddata->sels_mutex);

  if (!s->sounddata->sels) {
    sample_set_selection_1 (s, 0, s->sounddata->nr_frames);
    goto out;
  }

  gl = osels = s->sounddata->sels;
  s->sounddata->sels = NULL;

  sel = osel = (sw_sel *)gl->data;
  if (osel->sel_start > 0) {
    sample_add_selection_1 (s, 0, osel->sel_start - 1);
  }

  gl = gl->next;

  for (; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;
    sample_add_selection_1 (s, osel->sel_end, sel->sel_start - 1);
    osel = sel;
  }

  if (sel->sel_end != s->sounddata->nr_frames) {
    sample_add_selection_1 (s, sel->sel_end, s->sounddata->nr_frames);
  }

  g_list_free (osels);

out:
  g_mutex_unlock (s->sounddata->sels_mutex);
}

void
sample_selection_invert (sw_sample * s)
{
  sample_register_sel_op (s, "Invert selection", ss_invert, NULL, NULL);
}

static void
ss_select_all (sw_sample * s, sw_param_set unused, gpointer unused2)
{
  g_mutex_lock (s->sounddata->sels_mutex);

  sample_set_selection_1 (s, 0, s->sounddata->nr_frames);

  g_mutex_unlock (s->sounddata->sels_mutex);
}

void
sample_selection_select_all (sw_sample * s)
{
  sample_register_sel_op (s, "Select all", ss_select_all, NULL, NULL);
}

static void
ss_select_none (sw_sample * s, sw_param_set unused, gpointer unused2)
{
  g_mutex_lock (s->sounddata->sels_mutex);

  sample_clear_selection (s);

  g_mutex_unlock (s->sounddata->sels_mutex);
}

void
sample_selection_select_none (sw_sample * s)
{
  sample_register_sel_op (s, "Select none", ss_select_none, NULL, NULL);
}

/*
 * Functions to handle the temporary selection
 */

void
sample_clear_tmp_sel (sw_sample * s)
{
 if (s->tmp_sel) g_free (s->tmp_sel);
 s->tmp_sel = NULL;
 last_tmp_view = NULL;
}


/* 
 * sample_set_tmp_sel (s, tsel)
 *
 * sets the tmp_sel of sample s to a list containing only tsel.
 * If tsel was part of the actual selection of s, it is first
 * removed from the selection.
 */
void
sample_set_tmp_sel (sw_sample * s, sw_view * tview, sw_sel * tsel)
{
  GList * gl;
  sw_sel * sel;

  /* XXX: Dump old tmp_sel */
  sample_clear_tmp_sel (s);

  s->tmp_sel =
    sel_copy (tsel); /* XXX: try to do this without copying? */
  last_tmp_view = tview;

  g_mutex_lock (s->sounddata->sels_mutex);

  for(gl = s->sounddata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    if(sel == tsel) {
      s->sounddata->sels = g_list_remove(s->sounddata->sels, sel);
    }
  }

  g_mutex_unlock (s->sounddata->sels_mutex);
}

void
sample_set_tmp_sel_1 (sw_sample * s, sw_view * tview, sw_framecount_t start, sw_framecount_t end)
{
  sw_sel * tsel;

  tsel = sel_new (start, end);

  sample_set_tmp_sel (s, tview, tsel);
}

static void
ssits (sw_sample * s, sw_param_set unused, gpointer unused2)
{
  g_mutex_lock (s->sounddata->sels_mutex);

  sample_add_selection (s, s->tmp_sel);
  s->tmp_sel = NULL;
  last_tmp_view = NULL;
  sample_normalise_selection (s);

  g_mutex_unlock (s->sounddata->sels_mutex);
}

void
sample_selection_insert_tmp_sel (sw_sample * s)
{
#define BUF_LEN 64
  gchar buf[BUF_LEN];

  snprintf (buf, BUF_LEN, "Insert selection [%d - %d]",
	    s->tmp_sel->sel_start, s->tmp_sel->sel_end);

  sample_register_sel_op (s, buf, ssits, NULL, NULL);
}

static void
ssrwts (sw_sample * s, sw_param_set unused, gpointer unused2)
{
  g_mutex_lock (s->sounddata->sels_mutex);

  sample_clear_selection (s);

  sample_add_selection (s, s->tmp_sel);

  g_mutex_unlock (s->sounddata->sels_mutex);

  s->tmp_sel = NULL;
  last_tmp_view = NULL;
}

void
sample_selection_replace_with_tmp_sel (sw_sample * s)
{
#define BUF_LEN 64
  gchar buf[BUF_LEN];

  snprintf (buf, BUF_LEN, "Selection [%d - %d]",
	    s->tmp_sel->sel_start, s->tmp_sel->sel_end);

  sample_register_sel_op (s, buf, ssrwts, NULL, NULL);
}
