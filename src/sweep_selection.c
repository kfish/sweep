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
GList *
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
  sample_set_selection (s, sr->sels);

  sample_refresh_views (s);
}

static sw_operation sel_op = {
  (SweepCallback)do_by_sel_replace,
  (SweepFunction)sel_replace_data_destroy,
  (SweepCallback)do_by_sel_replace,
  (SweepFunction)sel_replace_data_destroy
};

sw_op_instance *
register_selection_op (sw_sample * s, char * desc, SweepModify func,
		       sw_param_set pset, gpointer custom_data)
{
  sw_op_instance * inst;
  GList * sels;

  inst = sw_op_instance_new (desc, &sel_op);

  sels = sels_copy (s->sounddata->sels);
  inst->undo_data = sel_replace_data_new (sels);

  func (s, pset, custom_data);

  sels = sels_copy (s->sounddata->sels);
  inst->redo_data = sel_replace_data_new (sels);

  register_operation (s, inst);

  return inst;
}
