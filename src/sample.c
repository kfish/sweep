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
#include "format.h"
#include "view.h"
#include "sweep_undo.h"
#include "sample-display.h"
#include "driver.h"

extern sw_view * last_tmp_view;

static GList * sample_bank = NULL;


/* SData functions */

sw_sdata *
sdata_new_empty(char * directory, char * filename,
		gint nr_channels,
		gint sample_rate, gint sample_length)
{
  sw_sdata *s;
  glong len;

  s = g_malloc (sizeof(sw_sdata));
  if (!s)
    return NULL;

  if (directory)
    strncpy(s->directory, directory, MIN(SW_DIR_LEN, strlen(directory)));
  else
    s->directory[0] = '\0';

  if (filename)
    s->filename = strdup(filename);
  else
    s->filename = NULL;

  s->format = format_new (nr_channels, sample_rate);

  s->s_length = (glong) sample_length;

  len = frames_to_bytes (s->format, sample_length);
  s->data = g_malloc0 (len);
  if (!(s->data)) {
    fprintf(stderr, "Unable to allocate %ld bytes for sample data.\n", len);
    g_free(s);
    return NULL;
  }

  s->sels = NULL;
  s->tmp_sel = NULL;

  s->sels_mutex = g_mutex_new();

  return s;
}

void
sdata_destroy (sw_sdata * sdata)
{
  g_free (sdata->data);
  sdata_clear_selection (sdata);
  g_free (sdata);
}

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

  s->sdata = sdata_new_empty (directory, filename,
			      nr_channels, sample_rate, sample_length);

  s->views = NULL;

  s->registered_ops = NULL;
  s->current_undo = NULL;
  s->current_redo = NULL;

  return s;
}

sw_sample *
sample_new_copy(sw_sample * s)
{
  sw_sample * sn;

  sn = sample_new_empty(s->sdata->directory,
			s->sdata->filename,
			s->sdata->format->channels,
			s->sdata->format->rate,
			s->sdata->s_length);

  if(!sn) {
    fprintf(stderr, "Unable to allocate new sample.\n");
    return NULL;
  }

  memcpy(sn->sdata->data, s->sdata->data,
	 frames_to_bytes(s->sdata->format, s->sdata->s_length));

  sdata_copyin_selection (s->sdata, sn->sdata);

  return sn;
}

void
sample_destroy (sw_sample * s)
{
  sample_stop_playback (s);

  /* playback holds this mutex, so wait on it before
   * destroying the sdata.
   */
  g_mutex_lock (s->sdata->sels_mutex);

  sdata_destroy (s->sdata);

  /* XXX: Should do this: */
  /* trim_registered_ops (s, 0); */

  g_free (s);
}

void
sample_set_pathname (sw_sample * s, char * directory, char * filename)
{
  sw_sdata * sd = s->sdata;
  gchar * tmp;
  sw_view * v;
  GList * gl;

  if (directory)
    strncpy(sd->directory, directory, MIN(SW_DIR_LEN, strlen(directory)));
  else
    sd->directory[0] = '\0';

  if (filename) {
    /* in case filename == s->filename, as in when
     * the default "save" function is called
     */
    tmp = strdup (filename);
    if (sd->filename) g_free (sd->filename);
    sd->filename = tmp;
  } else {
    if (sd->filename) g_free (sd->filename);
    sd->filename = NULL;
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
 * This function is ot needed in general filters because sw_sample
 * pointers are persistent across sdata modifications.
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



/* 
 * Selection handling
 */

static sw_sel *
sel_new (glong start, glong end)
{
  glong s, e;
  sw_sel * sel;

  if (end>start) { s = start; e = end; }
  else { s = end; e = start; }

  sel = g_malloc (sizeof(sw_sel));
  sel->sel_start = s;
  sel->sel_end = e;

  return sel;
}

static sw_sel *
sel_copy (sw_sel * sel)
{
  sw_sel * nsel;

  nsel = sel_new (sel->sel_start, sel->sel_end);

  return nsel;
}

/*
 * sel_cmp (s1, s2)
 *
 * Compares two sw_sel's for g_list_insert_sorted() --
 * return > 0 if s1 comes after s2 in the sort order.
 */
static gint
sel_cmp (sw_sel * s1, sw_sel * s2)
{
  if (s1->sel_start > s2->sel_start) return 1;
  else return 0;
}

#ifdef DEBUG
static void
dump_sels (GList * sels, char * desc)
{
  GList * gl;
  sw_sel * sel;

  printf ("<dump %s>\n", desc);
  for (gl = sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;
    printf ("\t[%ld - %ld]\n", sel->sel_start, sel->sel_end);
  }
  printf ("</dump>\n");

}
#endif

/*
 * sels_copy (sels)
 *
 * returns a copy of sels
 */
static GList *
sels_copy (GList * sels)
{
  GList * gl, * nsels = NULL;
  sw_sel * sel, * nsel;

  for (gl = sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;
    nsel = sel_copy (sel);
    nsels = g_list_insert_sorted(nsels, sel, (GCompareFunc)sel_cmp);
  }

  return nsels;
}

/*
 * sel_replace: undo/redo functions for changing selection
 */

sel_replace_data *
sel_replace_data_new (GList * sels)
{
  sel_replace_data * s;

  s = g_malloc (sizeof(sel_replace_data));

  s->sels = sels;

  return s;
}

void
sel_replace_data_destroy (sel_replace_data * s)
{
  g_list_free (s->sels);
}

void
do_by_sel_replace (sw_sample * s, sel_replace_data * sr)
{
  sample_set_selection (s, sr->sels);

  sample_refresh_views (s);
}

static sw_operation sel_op = {
  (SweepCallback)do_by_sel_replace,
  (SweepFunction)sel_replace_data_destroy,
  (SweepCallback)do_by_sel_replace,
  (SweepFunction)sel_replace_data_destroy
};

static void
sample_register_sel_op (sw_sample * s, char * desc, SweepModify func)
{
  sw_op_instance * inst;
  GList * sels;

  inst = sw_op_instance_new (desc, &sel_op);

  sels = sels_copy (s->sdata->sels);
  inst->undo_data = sel_replace_data_new (sels);

  func (s);

  sels = sels_copy (s->sdata->sels);
  inst->redo_data = sel_replace_data_new (sels);

  register_operation (s, inst);  
}


guint
sample_sel_nr_regions (sw_sample * s)
{
  return g_list_length (s->sdata->sels);
}

void
sdata_clear_selection (sw_sdata * sdata)
{
  g_list_free(sdata->sels);

  sdata->sels = NULL;
}

void
sample_clear_selection (sw_sample * s)
{
  sdata_clear_selection (s->sdata);

  sample_stop_marching_ants (s);
}

static gint
sdata_sel_needs_normalising (sw_sdata *sdata)
{
  GList * gl;
  sw_sel * osel = NULL, * sel;

  if(!sdata->sels) return FALSE;
  
  /* Seed osel with 'fake' iteration of following loop */
  gl = sdata->sels;
  osel = (sw_sel *)gl->data;
  gl = gl->next;
  for(; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    if(osel->sel_end > sel->sel_start) {
      return TRUE;
    }

    if(osel->sel_end < sel->sel_end) {
      osel = sel;
    }
  }

  return FALSE;
}

/*
 * sdata_normalise_selection(sdata)
 *
 * normalise the selection of sdata, ie. make sure there's
 * no overlaps and merge adjoining sections.
 */

static void
sdata_normalise_selection (sw_sdata * sdata)
{
  GList * gl;
  GList * nsels = NULL;
  sw_sel * osel = NULL, * sel; 

  if (!sdata_sel_needs_normalising(sdata)) return;

  /* Seed osel with 'fake' iteration of following loop */
  gl = sdata->sels;
  osel = sel_copy((sw_sel *)gl->data);
  gl = gl->next;

  for (; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    /* Check for an overlap */
    if(osel->sel_end > sel->sel_start) {

      /* If sel is completely contained in osel, ignore it. */
      if(osel->sel_end > sel->sel_end) {
	continue;
      }

      /* Set: osel = osel INTERSECT sel
       * we already know osel->sel_start <= sel->sel_start */
      osel->sel_end = sel->sel_end;

    } else {
      /* No more overlaps with osel; insert it in nsels, and
       * reset osel. */
      nsels = g_list_insert_sorted(nsels, osel, (GCompareFunc)sel_cmp);
      osel = sel_copy(sel);
    }
  }

  /* Insert the last created osel */
  nsels = g_list_insert_sorted(nsels, osel, (GCompareFunc)sel_cmp);

  /* Clear the old selection */
  g_list_free (sdata->sels);

  /* Set the newly created (normalised) selection */
  sdata->sels = nsels;

}

static void
sample_normalise_selection (sw_sample * s)
{
  sdata_normalise_selection (s->sdata);
}

void
sdata_add_selection (sw_sdata * sdata, sw_sel * sel)
{
  sdata->sels =
    g_list_insert_sorted(sdata->sels, sel, (GCompareFunc)sel_cmp);
}

void
sample_add_selection (sw_sample * s, sw_sel * sel)
{
  if (!s->sdata->sels)
    sample_start_marching_ants (s);

  sdata_add_selection (s->sdata, sel);
}

sw_sel *
sdata_add_selection_1 (sw_sdata * sdata, glong start, glong end)
{
  sw_sel * sel;

  sel = sel_new (start, end);

  sdata_add_selection(sdata, sel);

  return sel;
}

sw_sel *
sample_add_selection_1 (sw_sample * s, glong start, glong end)
{
  return sdata_add_selection_1 (s->sdata, start, end);
}

void
sample_set_selection (sw_sample * s, GList * gl)
{
  sample_clear_selection(s);

  s->sdata->sels = sels_copy (gl);
}

sw_sel *
sdata_set_selection_1 (sw_sdata * sdata, glong start, glong end)
{
  sdata_clear_selection (sdata);

  return sdata_add_selection_1 (sdata, start, end);
}

sw_sel *
sample_set_selection_1 (sw_sample * s, glong start, glong end)
{
  return sdata_set_selection_1 (s->sdata, start, end);
}

void
sel_modify (sw_sample * s, sw_sel * sel, glong new_start, glong new_end)
{
  sel->sel_start = new_start;
  sel->sel_end = new_end;

  sample_normalise_selection (s);
}

static void
ss_invert (sw_sample * s)
{
  GList * gl;
  GList * osels;
  sw_sel * osel, * sel;

  g_mutex_lock (s->sdata->sels_mutex);

  if (!s->sdata->sels) {
    sample_set_selection_1 (s, 0, s->sdata->s_length);
    goto out;
  }

  gl = osels = s->sdata->sels;
  s->sdata->sels = NULL;

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

  if (sel->sel_end != s->sdata->s_length) {
    sample_add_selection_1 (s, sel->sel_end, s->sdata->s_length);
  }

  g_list_free (osels);

out:
  g_mutex_unlock (s->sdata->sels_mutex);
}

void
sample_selection_invert (sw_sample * s)
{
  sample_register_sel_op (s, "Invert selection", ss_invert);
}

static void
ss_select_all (sw_sample * s)
{
  g_mutex_lock (s->sdata->sels_mutex);

  sample_set_selection_1 (s, 0, s->sdata->s_length);

  g_mutex_unlock (s->sdata->sels_mutex);
}

void
sample_selection_select_all (sw_sample * s)
{
  sample_register_sel_op (s, "Select all", ss_select_all);
}

static void
ss_select_none (sw_sample * s)
{
  g_mutex_lock (s->sdata->sels_mutex);

  sample_clear_selection (s);

  g_mutex_unlock (s->sdata->sels_mutex);
}

void
sample_selection_select_none (sw_sample * s)
{
  sample_register_sel_op (s, "Select none", ss_select_none);
}


gint
sdata_selection_length (sw_sdata * sdata)
{
  gint length = 0;
  GList * gl;
  sw_sel * sel;

  for (gl = sdata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    length += sel->sel_end - sel->sel_start;
  }

  return length;
}

void
sdata_selection_translate (sw_sdata * sdata, gint delta)
{
  GList * gl;
  sw_sel * sel;

  for (gl = sdata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    sel->sel_start += delta;
    sel->sel_end += delta;
  }
}


/*
 * sdata_copyin_selection (sdata1, sdata2)
 *
 * copies the selection of sdata1 into sdata2. If sdata2 previously
 * had a selection, the two are merged.
 */
void
sdata_copyin_selection (sw_sdata * sdata1, sw_sdata * sdata2)
{
  GList * gl;
  sw_sel * sel, *sel2;

  for (gl = sdata1->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    sel2 = sel_copy (sel);
    sdata_add_selection (sdata2, sel2);
  }

  sdata_normalise_selection (sdata2);
}


/*
 * Functions to handle the temporary selection
 */

void
sample_clear_tmp_sel (sw_sample * s)
{
 if (s->sdata->tmp_sel) g_free (s->sdata->tmp_sel);
 s->sdata->tmp_sel = NULL;
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

  s->sdata->tmp_sel =
    sel_copy (tsel); /* XXX: try to do this without copying? */
  last_tmp_view = tview;

  g_mutex_lock (s->sdata->sels_mutex);

  for(gl = s->sdata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    if(sel == tsel) {
      s->sdata->sels = g_list_remove(s->sdata->sels, sel);
    }
  }

  g_mutex_unlock (s->sdata->sels_mutex);
}

void
sample_set_tmp_sel_1 (sw_sample * s, sw_view * tview, glong start, glong end)
{
  sw_sel * tsel;

  tsel = sel_new (start, end);

  sample_set_tmp_sel (s, tview, tsel);
}

static void
ssits (sw_sample * s)
{
  g_mutex_lock (s->sdata->sels_mutex);

  sample_add_selection (s, s->sdata->tmp_sel);
  s->sdata->tmp_sel = NULL;
  last_tmp_view = NULL;
  sample_normalise_selection (s);

  g_mutex_unlock (s->sdata->sels_mutex);
}

void
sample_selection_insert_tmp_sel (sw_sample * s)
{
#define BUF_LEN 64
  gchar buf[BUF_LEN];

  snprintf (buf, BUF_LEN, "Insert selection [%ld - %ld]",
	    s->sdata->tmp_sel->sel_start, s->sdata->tmp_sel->sel_end);

  sample_register_sel_op (s, buf, ssits);
}

static void
ssrwts (sw_sample * s)
{
  g_mutex_lock (s->sdata->sels_mutex);

  sample_clear_selection (s);

  sample_add_selection (s, s->sdata->tmp_sel);

  g_mutex_unlock (s->sdata->sels_mutex);

  s->sdata->tmp_sel = NULL;
  last_tmp_view = NULL;
}

void
sample_selection_replace_with_tmp_sel (sw_sample * s)
{
#define BUF_LEN 64
  gchar buf[BUF_LEN];

  snprintf (buf, BUF_LEN, "Selection [%ld - %ld]",
	    s->sdata->tmp_sel->sel_start, s->sdata->tmp_sel->sel_end);

  sample_register_sel_op (s, buf, ssrwts);
}
