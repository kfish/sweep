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

#include <sweep/sweep_sample.h>
#include <sweep/sweep_typeconvert.h>
#include <sweep/sweep_undo.h>

#include "format.h"
#include "view.h"
#include "sample-display.h"
#include "driver.h"

sw_sel *
sel_new (sw_framecount_t start, sw_framecount_t end)
{
  sw_framecount_t s, e;
  sw_sel * sel;

  if (end>start) { s = start; e = end; }
  else { s = end; e = start; }

  sel = g_malloc (sizeof(sw_sel));
  sel->sel_start = s;
  sel->sel_end = e;

  return sel;
}

sw_sel *
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
gint
sel_cmp (sw_sel * s1, sw_sel * s2)
{
  if (s1->sel_start > s2->sel_start) return 1;
  else return 0;
}

static void
dump_sels (GList * sels, char * desc)
{
#ifdef DEBUG
  GList * gl;
  sw_sel * sel;

  printf ("<dump %s (%p)>\n", desc, sels);
  for (gl = sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;
    printf ("\t[%ld - %ld]\n", sel->sel_start, sel->sel_end);
  }
  printf ("</dump>\n");
#endif
}


/*
 * sels_copy (sels)
 *
 * returns a copy of sels
 */
GList *
sels_copy (GList * sels)
{
  GList * gl, * nsels = NULL;
  sw_sel * sel, * nsel;

  for (gl = sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;
    nsel = sel_copy (sel);
    nsels = g_list_insert_sorted(nsels, nsel, (GCompareFunc)sel_cmp);
  }

  return nsels;
}

GList *
sels_add_selection (GList * sels, sw_sel * sel)
{
  return g_list_insert_sorted (sels, sel, (GCompareFunc)sel_cmp);
}

GList *
sels_add_selection_1 (GList * sels, sw_framecount_t start, sw_framecount_t end)
{
  sw_sel * sel;

  sel = sel_new (start, end);

  return sels_add_selection (sels, sel);
}

/*
 * sels_invert (sels, nr_frames)
 *
 * inverts sels in place
 */
GList *
sels_invert (GList * sels, sw_framecount_t nr_frames)
{
  GList * gl;
  GList * osels;
  sw_sel * osel, * sel;

  if (!sels) {
    g_list_free (sels);
    sel = sel_new (0, nr_frames);
    sels = NULL;
    sels = g_list_append (sels, sel);
    return sels;
  }

  gl = osels = sels;
  sels = NULL;

  sel = osel = (sw_sel *)gl->data;
  if (osel->sel_start > 0) {
    sels = sels_add_selection_1 (sels, 0, osel->sel_start - 1);
  }

  gl = gl->next;

  for (; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;
    sels = sels_add_selection_1 (sels, osel->sel_end, sel->sel_start - 1);
    osel = sel;
  }

  if (sel->sel_end != nr_frames) {
    sels = sels_add_selection_1 (sels, sel->sel_end, nr_frames);
  }

  g_list_free (osels);

  return sels;
}

/*
 * sel_replace: undo/redo functions for changing selection
 */

typedef struct _sel_replace_data sel_replace_data;

struct _sel_replace_data {
  GList * sels;
};

static sel_replace_data *
sel_replace_data_new (GList * sels)
{
  sel_replace_data * s;

  s = g_malloc (sizeof(sel_replace_data));

  s->sels = sels;

  return s;
}

static void
sel_replace_data_destroy (sel_replace_data * s)
{
  g_list_free (s->sels);
  g_free (s);
}

static void
do_by_sel_replace (sw_sample * s, sel_replace_data * sr)
{
  dump_sels (sr->sels, "at replace action");
  sample_set_selection (s, sr->sels);

  /*  sample_refresh_views (s);*/
}

static void
do_selection_op_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  sw_perform_data * pd = (sw_perform_data *)inst->do_data;
  
  SweepFilter func = (SweepFilter)pd->func;
  sw_param_set pset = pd->pset;
  void * custom_data = pd->custom_data;

  GList * sels;

  sels = sels_copy (sample->sounddata->sels);
  dump_sels (sels, "copied for undo data");
  inst->undo_data = sel_replace_data_new (sels);
  set_active_op (sample, inst);

  func (sample, pset, custom_data);

  if (sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    sels = sels_copy (sample->sounddata->sels);
    dump_sels (sels, "copied for redo data");
    inst->redo_data = sel_replace_data_new (sels);

    register_operation (sample, inst);
  }

#if 0
  sample_set_edit_state (sample, SWEEP_EDIT_STATE_DONE);
#endif
}

static sw_operation selection_op = {
  SWEEP_EDIT_MODE_META,
  (SweepCallback)do_selection_op_thread,
  (SweepFunction)NULL,
  (SweepCallback)do_by_sel_replace,
  (SweepFunction)sel_replace_data_destroy,
  (SweepCallback)do_by_sel_replace,
  (SweepFunction)sel_replace_data_destroy
};

sw_op_instance *
perform_selection_op (sw_sample * sample, char * desc, SweepFilter func,
		      sw_param_set pset, gpointer custom_data)
{
  sw_perform_data * pd = (sw_perform_data *)g_malloc (sizeof(*pd));

  pd->func = (SweepFunction)func;
  pd->pset = pset;
  pd->custom_data = custom_data;

  schedule_operation (sample, desc, &selection_op, pd);

  return NULL;
}
