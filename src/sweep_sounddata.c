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
#include <glib.h>

#if 0
#include <unistd.h>
#include <sys/mman.h>
#endif

#include <sweep/sweep_types.h>
#include <sweep/sweep_typeconvert.h>
#include <sweep/sweep_selection.h>
#include <sweep/sweep_undo.h>

#include "edit.h"
#include "format.h"
#include "view.h"
#include "sample-display.h"
#include "driver.h"

sw_sounddata *
sounddata_new_empty(gint nr_channels, gint sample_rate, gint sample_length)
{
  sw_sounddata *s;
  sw_framecount_t len;

  s = g_malloc (sizeof(sw_sounddata));
  if (!s)
    return NULL;

  s->refcount = 1;

  s->format = format_new (nr_channels, sample_rate);

  s->nr_frames = (sw_framecount_t) sample_length;

  if (sample_length > 0) {
    len = frames_to_bytes (s->format, sample_length);

#if 1
    s->data = g_malloc0 ((size_t)len);
#else
    s->data = sweep_large_alloc_zero (len, PROT_READ|PROT_WRITE);
#endif


    if (!(s->data)) {
#if 0
#if (SIZEOF_OFF_T == 8)
      fprintf(stderr, "Unable to allocate %lld bytes for sample data.\n", len);
#else
      fprintf(stderr, "Unable to allocate %d bytes for sample data.\n", len);
#endif
#else
      fprintf(stderr, "Unable to allocate %d bytes for sample data.\n", len);
#endif
      g_free(s);
      return NULL;
#ifdef DEBUG
    } else {
      g_print ("g_malloc0'd %d bytes for new sounddata\n", len);
#endif
    }
  } else {
    s->data = NULL;
  }

  s->sels = NULL;
  s->sels_mutex = g_mutex_new();
  s->data_mutex = g_mutex_new();

  return s;
}

void
sounddata_clear_selection (sw_sounddata * sounddata)
{
  GList * gl;
  sw_sel * sel;

  for (gl = sounddata->sels; gl; gl = gl->next){
          sel = (sw_sel*)gl->data;
          sel_free(sel);
  }

  g_list_free(sounddata->sels);

  sounddata->sels = NULL;
};

void
sounddata_destroy (sw_sounddata * sounddata)
{
  /*size_t len;*/

  sounddata->refcount--;

  if (sounddata->refcount <= 0) {
#if 1
    g_free (sounddata->data);
#else
    len = frames_to_bytes (sounddata->format, sounddata->nr_frames);
    munmap (sounddata->data, len);
#endif
    sounddata_clear_selection (sounddata);
    g_free (sounddata);
    g_mutex_free(sounddata->data_mutex);
  }
}

void
sounddata_lock_selection (sw_sounddata * sounddata)
{
  g_mutex_lock (sounddata->sels_mutex);
}

void
sounddata_unlock_selection (sw_sounddata * sounddata)
{
  g_mutex_unlock (sounddata->sels_mutex);
}

guint
sounddata_selection_nr_regions (sw_sounddata * sounddata)
{
  return g_list_length (sounddata->sels);
}

static gint
sounddata_sel_needs_normalising (sw_sounddata *sounddata)
{
  GList * gl;
  sw_sel * osel = NULL, * sel;
  sw_framecount_t nr_frames;

  if(!sounddata->sels) return FALSE;
  
  nr_frames = sounddata->nr_frames;

  /* Seed osel with 'fake' iteration of following loop */
  gl = sounddata->sels;
  osel = (sw_sel *)gl->data;
  if (osel->sel_start < 0 || osel->sel_end > nr_frames)
    return TRUE;

  gl = gl->next;
  for(; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    if (sel->sel_start < 0 || sel->sel_end > nr_frames)
      return TRUE;

    if(osel->sel_end >= sel->sel_start) {
      return TRUE;
    }

    if(osel->sel_end < sel->sel_end) {
      osel = sel;
    }
  }

  return FALSE;
}

/*
 * sounddata_normalise_selection(sounddata)
 *
 * normalise the selection of sounddata, ie. make sure there's
 * no overlaps and merge adjoining sections.
 */

void
sounddata_normalise_selection (sw_sounddata * sounddata)
{
  GList * gl;
  GList * nsels = NULL;
  sw_sel * osel = NULL, * sel; 
  sw_framecount_t nr_frames;

  if (!sounddata_sel_needs_normalising(sounddata)) return;

  nr_frames = sounddata->nr_frames;

  /* Seed osel with 'fake' iteration of following loop */
  gl = sounddata->sels;
  sel = (sw_sel *)gl->data;
  sel->sel_start = CLAMP(sel->sel_start, 0, nr_frames);
  sel->sel_end = CLAMP(sel->sel_end, 0, nr_frames);
  
  osel = sel_copy(sel);

  gl = gl->next;

  for (; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;
    sel->sel_start = CLAMP(sel->sel_start, 0, nr_frames);
    sel->sel_end = CLAMP(sel->sel_end, 0, nr_frames);

    /* Check for an overlap */
    if(osel->sel_end >= sel->sel_start) {

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
      if (osel->sel_start == osel->sel_end) {
	g_free (osel);
      } else {
	nsels = g_list_insert_sorted(nsels, osel, (GCompareFunc)sel_cmp);
      }
      osel = sel_copy(sel);
    }
  }

  /* Insert the last created osel */
  if (osel->sel_start == osel->sel_end) {
    g_free (osel);
  } else {
    nsels = g_list_insert_sorted(nsels, osel, (GCompareFunc)sel_cmp);
  }

  /* Clear the old selection */
  sounddata_clear_selection (sounddata);

  /* Set the newly created (normalised) selection */
  sounddata->sels = nsels;

}

void
sounddata_add_selection (sw_sounddata * sounddata, sw_sel * sel)
{
  sounddata->sels =
    g_list_insert_sorted(sounddata->sels, sel, (GCompareFunc)sel_cmp);
}

sw_sel *
sounddata_add_selection_1 (sw_sounddata * sounddata, sw_framecount_t start, sw_framecount_t end)
{
  sw_sel * sel;

  sel = sel_new (start, end);

  sounddata_add_selection(sounddata, sel);

  return sel;
}

sw_sel *
sounddata_set_selection_1 (sw_sounddata * sounddata, sw_framecount_t start, sw_framecount_t end)
{
  sounddata_clear_selection (sounddata);

  return sounddata_add_selection_1 (sounddata, start, end);
}

gint
sounddata_selection_nr_frames (sw_sounddata * sounddata)
{
  gint nr_frames = 0;
  GList * gl;
  sw_sel * sel;

  for (gl = sounddata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    nr_frames += sel->sel_end - sel->sel_start;
  }

  return nr_frames;
}

sw_framecount_t
sounddata_selection_width (sw_sounddata * sounddata)
{
  GList * gl;
  sw_sel * sel;
  sw_framecount_t start, end;

  if ((gl = sounddata->sels) == NULL) return 0;
  sel = (sw_sel *)gl->data;
  start = sel->sel_start;

  gl = g_list_last (sounddata->sels);
  sel = (sw_sel *)gl->data;
  end = sel->sel_end;

  return (end - start);
}

void
sounddata_selection_translate (sw_sounddata * sounddata, gint delta)
{
  GList * gl;
  sw_sel * sel;

  for (gl = sounddata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    sel->sel_start += delta;
    sel->sel_end += delta;
  }

  /* XXX: Crop any regions outside of [0, nr_frames] */
  sounddata_normalise_selection (sounddata);
}

void
sounddata_selection_scale (sw_sounddata * sounddata, gfloat scale)
{
  GList * gl;
  sw_sel * sel;
  sw_framecount_t sels_start;

  if ((gl = sounddata->sels) == NULL) return;
  
  sel = (sw_sel *)gl->data;
  sels_start = sel->sel_start;

  for (gl = sounddata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    sel->sel_start = sels_start + ((sel->sel_start - sels_start) * scale);
    sel->sel_end = sels_start + ((sel->sel_end - sels_start) * scale);
  }

  /* XXX: Crop any regions outside of [0, nr_frames] */
  sounddata_normalise_selection (sounddata);
}

/*
 * sounddata_copyin_selection (sounddata1, sounddata2)
 *
 * copies the selection of sounddata1 into sounddata2. If sounddata2 previously
 * had a selection, the two are merged.
 */
void
sounddata_copyin_selection (sw_sounddata * sounddata1, sw_sounddata * sounddata2)
{
  GList * gl;
  sw_sel * sel, *sel2;

  for (gl = sounddata1->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    sel2 = sel_copy (sel);
    sounddata_add_selection (sounddata2, sel2);
  }

  sounddata_normalise_selection (sounddata2);
}
