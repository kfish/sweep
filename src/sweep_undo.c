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
#include <string.h>
#include <glib.h>
#include <pthread.h>

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_undo.h>
#include <sweep/sweep_sample.h>
#include <sweep/sweep_sounddata.h>
#include <sweep/sweep_selection.h>

#include "sweep_app.h"
#include "edit.h"
#include "undo_dialog.h"
#include "head.h"
#include "play.h"
#include "file_dialogs.h"
#include "question_dialogs.h"

#ifdef LIMITED_UNDO
/* Nr. of undo operations remembered */
#define UNDO_LEVELS 99
#endif

/* Within each sample s, maintain:
 *   s->current_undo == s->current_redo->prev
 *   s->current_redo == s->current_undo->next
 */

/*#define DEBUG*/

static void
op_main (sw_sample * sample)
{
  GList * gl;
  sw_op_instance * inst;

#ifdef DEBUG
  g_print ("%d: Hello from op_main %d!\n", getpid(), getpid());
#endif

  while (sample->edit_state != SWEEP_EDIT_STATE_CANCEL &&
	 (gl = sample->pending_ops) != NULL) {

    g_mutex_lock (sample->edit_mutex);
    while (sample->edit_state != SWEEP_EDIT_STATE_PENDING &&
	   sample->edit_state != SWEEP_EDIT_STATE_CANCEL) {
      g_cond_wait (sample->pending_cond, sample->edit_mutex);
    }

    if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
#ifdef DEBUG
      g_print ("Caught an early cancelmoose; pending is %p\n",
	       sample->pending_ops);
      fflush (stdout);
#endif
    } else {
      gboolean was_going = FALSE;
      
      inst = (sw_op_instance *)gl->data;

      g_assert (sample->edit_state == SWEEP_EDIT_STATE_PENDING);
      
      sample->edit_state = SWEEP_EDIT_STATE_BUSY;
      sample->pending_ops = g_list_remove_link (sample->pending_ops, gl);
      
      g_mutex_unlock (sample->edit_mutex);
      
      if (inst->op->edit_mode == SWEEP_EDIT_MODE_ALLOC) {
	g_mutex_lock (sample->play_mutex);
	if ((was_going = sample->play_head->going)) {
	  head_set_stop_offset (sample->play_head, sample->user_offset);
	  head_set_going (sample->play_head, FALSE);
	}
	g_mutex_unlock (sample->play_mutex);
      }

      /* XXX: this is fubar -- change to SweepFunction ?? or change all to
       * have sample as first arg ... */
      inst->op->_do_ ((sw_sample *)inst, (void *)inst);

#if 0 /* XXX: need to tell main thread to start playback; freezes from here */
      if (inst->op->edit_mode == SWEEP_EDIT_MODE_ALLOC && was_going) {
#if 0
	g_mutex_lock (sample->play_mutex);
	head_set_going (sample->play_head, TRUE);
	g_mutex_unlock (sample->play_mutex);
#else
	sample_play (sample);
#endif
      }
#endif 
      g_mutex_lock (sample->edit_mutex);
      
#ifdef DEBUG
      if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
	g_print ("Caught a late cancelmoose; pending is %p\n",
		 sample->pending_ops);
	fflush (stdout);
      }  else {
	g_print ("%d: post-op edit state is %d\n", getpid(),
		 sample->edit_state);
      }
#endif
      
    }

    sample->edit_state = SWEEP_EDIT_STATE_DONE;

    g_mutex_unlock (sample->edit_mutex);
  }


  g_mutex_lock (sample->edit_mutex);

#ifdef DEBUG
  if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
    g_print ("%d: Let me save you from scrubby!.\n", getpid());
    sample->edit_state = SWEEP_EDIT_STATE_DONE;
  } else {
    g_print ("%d: exit edit state is %d\n", getpid(), sample->edit_state);
  }

  g_print ("%d: yada yada that's all!\n", getpid());

#endif

  sample->ops_thread = (pthread_t) -1;

  g_mutex_unlock (sample->edit_mutex);

}

static void
prepare_op (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;

#define BUF_LEN 128
  gchar buf[BUF_LEN];

  g_snprintf (buf, BUF_LEN, "%s (%%p%%%%)", inst->description);
  sample_set_progress_text (sample, buf);
  
  sample_set_edit_mode (sample, inst->op->edit_mode);
  sample_set_progress_percent (sample, 0);

  if (sample->ops_thread == (pthread_t) -1) {
    pthread_create (&sample->ops_thread, NULL, (void *) (*op_main), sample);
  }
#undef BUF_LEN
}

gint
update_edit_progress (gpointer data)
{
  sw_sample * sample = (sw_sample *)data;
  sw_op_instance * inst;

  if (sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    sample_refresh_progress_percent (sample);
    if (sample->edit_mode == SWEEP_EDIT_MODE_META ||
	sample->edit_mode == SWEEP_EDIT_MODE_FILTER)
      sample_refresh_views (sample);

    return TRUE;
  }

  if (sample->edit_state == SWEEP_EDIT_STATE_DONE) {
    undo_dialog_refresh_history (sample);

    if (sample->sounddata->sels == NULL) {
      sample_stop_marching_ants (sample);
    }

    sample_refresh_views (sample);

    if (sample->pending_ops == NULL)
      sample_set_edit_state (sample, SWEEP_EDIT_STATE_IDLE);
  }

  if (sample->pending_ops != NULL) {
    inst = (sw_op_instance *)sample->pending_ops->data;
    prepare_op (inst);
    sample_set_edit_state (sample, SWEEP_EDIT_STATE_PENDING);
  }

  if (sample->edit_state == SWEEP_EDIT_STATE_IDLE) {
    sample_refresh_views (sample);

    sample->op_progress_tag = -1;
    return FALSE;
  }

  if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
    sample_set_progress_text (sample, "Scrubby has you.");
  }

  return TRUE;
}

static void
sw_op_instance_clear (sw_op_instance * inst)
{
  if (!inst) return;

  if (inst->do_data) {
    if (inst->op->purge_do) inst->op->purge_do (inst->do_data);
  }
  if (inst->undo_data) {
    if (inst->op->purge_undo) inst->op->purge_undo (inst->undo_data);
    else g_free (inst->undo_data);
  }
  if (inst->redo_data && inst->redo_data != inst->undo_data) {
    if (inst->op->purge_redo) inst->op->purge_redo (inst->redo_data);
    else g_free (inst->redo_data);

  }

  inst->do_data = NULL;
  inst->undo_data = NULL;
  inst->redo_data = NULL;
}

sw_op_instance *
sw_op_instance_new (sw_sample * sample, char * desc, sw_operation * op)
{
  sw_op_instance * inst;

  inst = g_malloc (sizeof(sw_op_instance));
  inst->sample = sample;
  inst->description = strdup (desc);
  inst->op = op;
  inst->do_data = NULL;
  inst->undo_data = NULL;
  inst->redo_data = NULL;

  return inst;
}

void
trim_registered_ops (sw_sample * s, int length)
{
  GList * gl;

  while (g_list_length (s->registered_ops) > length) {
    gl = g_list_first (s->registered_ops);
    s->registered_ops = g_list_remove_link (s->registered_ops, gl);

    sw_op_instance_clear (gl->data);
    g_list_free (gl);
  }

  if (s->registered_ops == NULL)
    s->current_undo = NULL;
}

static void
schedule_operation_do (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;

  g_mutex_lock (sample->edit_mutex);
  sample->pending_ops = g_list_append (sample->pending_ops, inst);
  g_mutex_unlock (sample->edit_mutex);

  if (sample->op_progress_tag == -1) {
    sample->op_progress_tag =
      gtk_timeout_add (30, (GtkFunction)update_edit_progress,
		       (gpointer)sample);
  }
}

static void
schedule_operation_ok_cb (GtkWidget * widget, gpointer data)
{
  sw_op_instance * inst = (sw_op_instance *)data;
  sw_sample * sample = inst->sample;

  sample->edit_ignore_mtime = TRUE;

  schedule_operation_do (inst);
}

void
schedule_operation (sw_sample * sample, char * description,
		    sw_operation * operation, void * do_data)
{
  sw_op_instance * inst;
#undef BUF_LEN
#define BUF_LEN 512
  char buf[BUF_LEN];

  inst = sw_op_instance_new (sample, description, operation);
  inst->do_data = do_data;

  if (operation->edit_mode != SWEEP_EDIT_MODE_META &&
      !sample->edit_ignore_mtime && sample_mtime_changed (sample)) {

    snprintf (buf, BUF_LEN,
	      _("%s\n has changed on disk.\n\n"
		"Do you want to continue editing this buffer?"),
	      sample->pathname);
    
    question_dialog_new (sample, _("File modified"), buf,
			 _("Continue editing"), _("Reread from disk"),
			 schedule_operation_ok_cb, inst,
			 sample_revert_ok_cb, sample,
			 SWEEP_EDIT_MODE_ALLOC);
  } else {
    schedule_operation_do (inst);
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

  g_mutex_lock (s->ops_mutex);

  if (s->registered_ops && s->current_redo) {
    /* Split the list here -- keep everything up to and
     * including current_undo, and trash the rest.
     */
    s->current_redo->prev = NULL;

    if (s->current_undo)
      s->current_undo->next = NULL;

    if (s->registered_ops == s->current_redo)
      s->registered_ops = NULL;

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

#ifdef LIMITED_UNDO
  trim_registered_ops (s, UNDO_LEVELS);
#endif

  s->current_undo = g_list_find (s->registered_ops, inst);

  s->active_op = NULL;

  if (inst->op->edit_mode != SWEEP_EDIT_MODE_META) {
    s->modified = TRUE;
  }

  g_mutex_unlock (s->ops_mutex);
}

static void
undo_operation (sw_sample * s, sw_op_instance * inst)
{
  if (inst && inst->op->undo) {
    inst->op->undo(s, inst->undo_data);
  }
}

static void
redo_operation (sw_sample * s, sw_op_instance * inst)
{
  if (inst && inst->op->redo) {
    inst->op->redo(s, inst->redo_data);
  }
}

static void
do_undo_current_thread (sw_op_instance * inst)
{
  sw_sample * s = inst->sample;

  if (s == NULL || s->current_undo == NULL) goto noop;

  undo_operation (s, inst->do_data);
  
  g_mutex_lock (s->ops_mutex);
  if (s->edit_state == SWEEP_EDIT_STATE_BUSY) {
    s->current_redo = s->current_undo;
    s->current_undo = s->current_undo->prev;
  }
  g_mutex_unlock (s->ops_mutex);

  return;

 noop:
  sample_set_tmp_message (s, _("Nothing to undo"));
}

static sw_operation undo_filter_op = {
  SWEEP_EDIT_MODE_FILTER,
  (SweepCallback)do_undo_current_thread,
  (SweepFunction)NULL,
  (SweepCallback)NULL,
  (SweepFunction)NULL,
  (SweepCallback)NULL,
  (SweepFunction)NULL,
};

static sw_operation undo_alloc_op = {
  SWEEP_EDIT_MODE_ALLOC,
  (SweepCallback)do_undo_current_thread,
  (SweepFunction)NULL,
  (SweepCallback)NULL,
  (SweepFunction)NULL,
  (SweepCallback)NULL,
  (SweepFunction)NULL,
};

static void
schedule_undo_inst (sw_sample * sample, sw_op_instance * inst)
{
  sw_operation * op;
#undef BUF_LEN
#define BUF_LEN 128
  gchar buf[BUF_LEN];

  g_snprintf (buf, BUF_LEN, "Undo %s", inst->description);

  if (inst->op->edit_mode == SWEEP_EDIT_MODE_FILTER)
    op = &undo_filter_op;
  else
    op = &undo_alloc_op;

  schedule_operation (sample, buf, op, inst);
}

void
undo_current (sw_sample * sample)
{
  sw_op_instance * inst;

  if (sample == NULL) return;
  if (sample->edit_state != SWEEP_EDIT_STATE_IDLE) return;
  if (sample->current_undo == NULL) goto noop;

  inst = sample->current_undo->data;

  if (inst->op->undo != NULL)
    schedule_undo_inst (sample, inst);

  return;

 noop:
  sample_set_tmp_message (sample, _("Nothing to undo"));
  sample_set_progress_ready (sample);
}

static void
do_redo_current_thread (sw_op_instance * inst)
{
  sw_sample * s = inst->sample;

  if (s == NULL || s->current_redo == NULL) goto noop;

  redo_operation (s, inst->do_data);

  g_mutex_lock (s->ops_mutex);
  if (s->edit_state == SWEEP_EDIT_STATE_BUSY) {
    s->current_undo = s->current_redo;
    s->current_redo = s->current_redo->next;
  }
  g_mutex_unlock (s->ops_mutex);

  return;

 noop:
  sample_set_tmp_message (s, _("Nothing to redo"));
}

static sw_operation redo_filter_op = {
  SWEEP_EDIT_MODE_FILTER,
  (SweepCallback)do_redo_current_thread,
  (SweepFunction)NULL,
  (SweepCallback)NULL,
  (SweepFunction)NULL,
  (SweepCallback)NULL,
  (SweepFunction)NULL,
};

static sw_operation redo_alloc_op = {
  SWEEP_EDIT_MODE_ALLOC,
  (SweepCallback)do_redo_current_thread,
  (SweepFunction)NULL,
  (SweepCallback)NULL,
  (SweepFunction)NULL,
  (SweepCallback)NULL,
  (SweepFunction)NULL,
};

static void
schedule_redo_inst (sw_sample * sample, sw_op_instance * inst)
{
  sw_operation * op;

#define BUF_LEN 128
  gchar buf[BUF_LEN];

  g_snprintf (buf, BUF_LEN, "Redo %s", inst->description);

  if (inst->op->edit_mode == SWEEP_EDIT_MODE_FILTER)
    op = &redo_filter_op;
  else
    op = &redo_alloc_op;

  schedule_operation (sample, buf, op, inst);
}

void
redo_current (sw_sample * sample)
{
  sw_op_instance * inst;

  if (sample == NULL) return;
  if (sample->edit_state != SWEEP_EDIT_STATE_IDLE) return;
  if (sample->current_redo == NULL) goto noop;

  inst = sample->current_redo->data;

  if (inst->op->redo != NULL)
    schedule_redo_inst (sample, inst);

 noop:
  sample_set_tmp_message (sample, _("Nothing to redo"));
  sample_set_progress_ready (sample);
}

void
revert_op (sw_sample * sample, GList * op_gl)
{
  GList * gl = NULL;
  sw_op_instance * inst;
  gboolean need_undo = FALSE;

  if (sample == NULL) return;

  for (gl = g_list_last (sample->registered_ops); gl; gl = gl->prev) {
    inst = (sw_op_instance *)gl->data;

    if (gl == sample->current_undo) {
      need_undo = TRUE;
    }

    if (gl == op_gl) break;
  }

  if (need_undo) {
    for (gl = sample->current_undo; gl != op_gl; gl = gl->prev) {
      inst = (sw_op_instance *)gl->data;
      schedule_undo_inst (sample, inst);
    }
  } else {
    for (gl = sample->current_redo; gl != op_gl; gl = gl->next) {
      inst = (sw_op_instance *)gl->data;
      schedule_redo_inst (sample, inst);
    }
    if (gl != NULL) {
      inst = (sw_op_instance *)gl->data;
      schedule_redo_inst (sample, inst);
    }
  }
}


void
set_active_op (sw_sample * s, sw_op_instance * inst)
{
  g_mutex_lock (s->ops_mutex);

  g_assert (s->active_op == NULL);

  s->active_op = inst;

  g_mutex_unlock (s->ops_mutex);
}

#ifdef TRYCANCEL
static gint
try_cancel_active_op (gpointer data)
{
  sw_sample * s = (sw_sample *)data;

  if (g_mutex_trylock (s->ops_mutex)) {

    if (s->active_op) {
      undo_operation (s, s->active_op);
    }
    
    s->active_op = NULL;
    
    sample_set_edit_state (s, SWEEP_EDIT_STATE_CANCEL);

    g_mutex_unlock (s->ops_mutex);

    return FALSE;
  } else {
    return TRUE;
  }
}
#endif

void
cancel_active_op (sw_sample * s)
{
#ifdef TRYCANCEL

  gtk_idle_add ((GtkFunction)try_cancel_active_op, (gpointer)s);

#else

  g_mutex_lock (s->ops_mutex);

    
  if (s->active_op) {
    undo_operation (s, s->active_op);

    sample_set_tmp_message (s, "%s CANCELLED", s->active_op->description);

    /* XXX: does this leak s->active_op here? */
  }
  s->active_op = NULL;
  
  g_mutex_lock (s->edit_mutex);

  /*
   * Signal to the ops thread to cancel or not perform the current operation.
   * It is possible that the ops thread has already exited, in which case
   * s->ops_thread will have been set to -1 on its departure, also within
   * s->edit_mutex, in which case the signalled CANCEL would never be cleared.
   */
  if (s->ops_thread != (pthread_t) -1) {
    s->edit_state = SWEEP_EDIT_STATE_CANCEL;
  }

  if (s->pending_ops) {
    GList * gltmp = s->pending_ops;

    s->pending_ops = NULL;

    g_print ("XXX: cancel: need to remove pending ops\n");
    g_list_free (gltmp);
  }

  g_mutex_unlock (s->edit_mutex);

  /*  sample_set_edit_state (s, SWEEP_EDIT_STATE_CANCEL);*/
  
  g_mutex_unlock (s->ops_mutex);

#endif
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

sounddata_replace_data *
sounddata_replace_data_new (sw_sample * sample,
			    sw_sounddata * old_sounddata,
			    sw_sounddata * new_sounddata)
{
  sounddata_replace_data * sr;

  sr = g_malloc (sizeof(sounddata_replace_data));

  sr->sample = sample;
  sr->old_sounddata = old_sounddata;
  sr->new_sounddata = new_sounddata;

  old_sounddata->refcount++;
  new_sounddata->refcount++;

  return sr;
}

void
sounddata_replace_data_destroy (sounddata_replace_data * sr)
{
  sounddata_destroy (sr->old_sounddata);
  sounddata_destroy (sr->new_sounddata);

  g_free (sr);
}

void
undo_by_sounddata_replace (sw_sample * s, sounddata_replace_data * sr)
{
  g_mutex_lock (s->ops_mutex);
  s->sounddata = sr->old_sounddata;
  g_mutex_unlock (s->ops_mutex);
}

void
redo_by_sounddata_replace (sw_sample * s, sounddata_replace_data * sr)
{
  g_mutex_lock (s->ops_mutex);
  s->sounddata = sr->new_sounddata;
  g_mutex_unlock (s->ops_mutex);
}


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
}

void
redo_by_paste_over (sw_sample * s, paste_over_data * p)
{
  paste_over (s, p->new_eb);
}

splice_data *
splice_data_new (sw_edit_buffer * eb, GList * sels)
{
  splice_data * s;

  s = g_malloc (sizeof(splice_data));

  s->eb = eb;
  s->sels = sels_copy (sels);

  return s;
}

void
splice_data_destroy (splice_data * s)
{
  edit_buffer_destroy (s->eb);
  g_list_free (s->sels);
  g_free (s);
}

void
undo_by_splice_in (sw_sample * s, splice_data * sp)
{
  splice_in_eb (s, sp->eb);
}

void
redo_by_splice_out (sw_sample * s, splice_data * sp)
{
  splice_out_sel (s);
}

void
undo_by_splice_out (sw_sample * s, splice_data * sp)
{
  splice_out_sel (s);

  s->sounddata->sels = sels_copy (sp->sels);
}

void
redo_by_splice_in (sw_sample * s, splice_data * sp)
{
  splice_in_eb (s, sp->eb);
}

void
undo_by_splice_over (sw_sample * s, splice_data * sp)
{
  s->sounddata->sels = sels_copy (sp->sels);
  paste_over (s, sp->eb);
}

void
redo_by_splice_over (sw_sample * s, splice_data * sp)
{
  s->sounddata->sels = sels_copy (sp->sels);
  paste_over (s, sp->eb);
}

void
undo_by_crop_in (sw_sample * s, splice_data * sp)
{
  crop_in (s, sp->eb);
}

void
redo_by_crop_out (sw_sample * s, splice_data * sp)
{
  crop_out (s);
}
