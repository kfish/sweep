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

#include "sweep_undo.h"
#include "edit.h"
#include "sample.h"
#include "sweep_sounddata.h"

/* Nr. of undo operations remembered */
#define UNDO_LEVELS 7

#if 0
GList * registered_ops = NULL;

/* Maintain:
 *   current_undo == current_redo->prev
 *   current_redo == current_undo->next
 */
GList * current_undo = NULL;
GList * current_redo = NULL;
#endif

static void
sw_op_instance_clear (sw_op_instance * inst)
{
  if (!inst) return;

  if (inst->undo_data) {
    if (inst->op->purge_undo) inst->op->purge_undo (inst->undo_data);
    else g_free (inst->undo_data);
  }
  if (inst->redo_data && inst->redo_data != inst->undo_data) {
    if (inst->op->purge_redo) inst->op->purge_redo (inst->redo_data);
    else g_free (inst->redo_data);
  }
}

sw_op_instance *
sw_op_instance_new (char * desc, sw_operation * op)
{
  sw_op_instance * inst;

  inst = g_malloc (sizeof(sw_op_instance));
  inst->description = strdup (desc);
  inst->op = op;
  inst->undo_data = NULL;
  inst->redo_data = NULL;

  return inst;
}

static void
trim_registered_ops (sw_sample * s, int length)
{
  GList * gl;

  while (g_list_length (s->registered_ops) > length) {
    gl = g_list_first (s->registered_ops);
    s->registered_ops = g_list_remove_link (s->registered_ops, gl);

    sw_op_instance_clear (gl->data);
    g_list_free (gl);
  }
}

void
register_operation (sw_sample * s, sw_op_instance * inst)
{
  GList * trash, * gl;
  sw_op_instance * inst2;

#ifdef DEBUG
  printf("Registering %s\n", inst->description);
#endif

  if (s->registered_ops && s->current_undo && s->current_redo) {
    /* Split the list here -- keep everything up to and
     * including current_undo, and trash the rest.
     */
    s->current_redo->prev = NULL;
    s->current_undo->next = NULL;

    trash = s->current_redo;
    s->current_redo = NULL;

    /* Free up the rest of the list */
    for (gl = trash; gl; gl = gl->next) {
      inst2 = (sw_op_instance *)gl->data;
      sw_op_instance_clear (inst2);
    }

    /* Trash it */
    g_list_free (trash);
  }

  s->registered_ops = g_list_append (s->registered_ops, inst);

  trim_registered_ops (s, UNDO_LEVELS);

  s->current_undo = g_list_find (s->registered_ops, inst);
}

static void
undo_operation (sw_sample * s, sw_op_instance * inst)
{
#ifdef DEBUG
  printf("Undoing %s\n", inst->description);
#endif
  inst->op->undo(s, inst->undo_data);
}

static void
redo_operation (sw_sample * s, sw_op_instance * inst)
{
#ifdef DEBUG
  printf("Redoing %s\n", inst->description);
#endif
  inst->op->redo(s, inst->redo_data);
}

void
undo_current (sw_sample * s)
{
  sw_op_instance * inst;

  if (!s->current_undo) return;

  inst = s->current_undo->data;
  undo_operation (s, inst);

  s->current_redo = s->current_undo;
  s->current_undo = s->current_undo->prev;
}

void
redo_current (sw_sample * s)
{
  sw_op_instance * inst;

  if (!s->current_redo) return;

  inst = s->current_redo->data;
  redo_operation (s, inst);

  s->current_undo = s->current_redo;
  s->current_redo = s->current_redo->next;
}


/* Standard undo/redo functions */

#if 0
replace_data *
replace_data_new (sw_sample * old_sample, sw_sample * new_sample)
{
  replace_data * r;

  r = g_malloc (sizeof(replace_data));

  r->old_sample = old_sample;
  r->new_sample = new_sample;

  return r;
};

void
undo_by_replace (replace_data * r)
{
  sample_replace_throughout (r->new_sample, r->old_sample);

  sample_refresh_views (r->new_sample);
}

void
redo_by_replace (replace_data * r)
{
  sample_replace_throughout (r->old_sample, r->new_sample);

  sample_refresh_views (r->new_sample);
}
#endif

paste_over_data *
paste_over_data_new (sw_edit_buffer * old_eb, sw_edit_buffer * new_eb)
{
  paste_over_data * p;

  p = g_malloc (sizeof(paste_over_data));

  p->old_eb = old_eb;
  p->new_eb = new_eb;

  return p;
};

void
paste_over_data_destroy (paste_over_data * p)
{
  edit_buffer_destroy (p->old_eb);
  if (p->new_eb != p->old_eb)
    edit_buffer_destroy (p->new_eb);

  g_free (p);
}

void
undo_by_paste_over (sw_sample * s, paste_over_data * p)
{
  paste_over (s, p->old_eb);

  sample_refresh_views (s);
}

void
redo_by_paste_over (sw_sample * s, paste_over_data * p)
{
  paste_over (s, p->new_eb);

  sample_refresh_views (s);
}

splice_data *
splice_data_new (sw_edit_buffer * eb)
{
  splice_data * s;

  s = g_malloc (sizeof(splice_data));

  s->eb = eb;

  return s;
}

void
splice_data_destroy (splice_data * s)
{
  edit_buffer_destroy (s->eb);
  g_free (s);
}

void
undo_by_splice_in (sw_sample * s, splice_data * sp)
{
  sw_sounddata * out;

  out = splice_in_eb (s->sounddata, sp->eb);

  sounddata_destroy (s->sounddata);
  s->sounddata = out;

  sample_refresh_views (s);
}

void
redo_by_splice_out (sw_sample * s, splice_data * sp)
{
  sw_sounddata * out;

  out = splice_out_sel (s->sounddata);

  sounddata_destroy (s->sounddata);
  s->sounddata = out;

  sample_refresh_views (s);
}

void
undo_by_splice_out (sw_sample * s, splice_data * sp)
{
  sw_sounddata * out;

  out = splice_out_sel (s->sounddata);

  sounddata_destroy (s->sounddata);
  s->sounddata = out;

  sample_refresh_views (s);
}

void
redo_by_splice_in (sw_sample * s, splice_data * sp)
{
  sw_sounddata * out;

  out = splice_in_eb (s->sounddata, sp->eb);

  sounddata_destroy (s->sounddata);
  s->sounddata = out;

  sample_refresh_views (s);
}


