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

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_types.h>
#include <sweep/sweep_typeconvert.h>
#include <sweep/sweep_undo.h>
#include <sweep/sweep_filter.h>
#include <sweep/sweep_sounddata.h>
#include <sweep/sweep_sample.h>

#include "sweep_app.h"
#include "edit.h"


static void
do_filter_regions (sw_sample * sample, SweepFilterRegion func,
		   sw_param_set pset, gpointer custom_data)
{
  sw_sounddata * sounddata = sample->sounddata;
  sw_format * f = sounddata->format;
  GList * gl;
  sw_sel * sel;
  sw_framecount_t sel_total, run_total;
  sw_framecount_t offset, remaining, n;
  gpointer d;
  gint percent;

  gboolean active = TRUE;

  sel_total = sounddata_selection_nr_frames (sounddata) / 100;
  if (sel_total == 0) sel_total = 1;
  run_total = 0;

  for (gl = sounddata->sels; active && gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    offset = 0;
    remaining = sel->sel_end - sel->sel_start;

    while (active && remaining > 0) {
      g_mutex_lock (&sample->ops_mutex);

      if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
	active = FALSE;
      } else {
	d = sounddata->data + (int)frames_to_bytes (f, sel->sel_start + offset);

	n = MIN(remaining, 1024);

	func (d, sounddata->format, n, pset, custom_data);

	remaining -= n;
	offset += n;

	run_total += n;
	percent = run_total / sel_total;
	sample_set_progress_percent (sample, percent);

#ifdef DEBUG
	g_print ("completed %d / %d frames, %d%%\n", run_total, sel_total,
		 percent);
#endif
      }

      g_mutex_unlock (&sample->ops_mutex);
    }
  }
}

static void
do_filter_regions_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  sw_perform_data * pd = (sw_perform_data *)inst->do_data;

  sw_edit_buffer * old_eb;
  paste_over_data * p;

  if (sample == NULL || sample->sounddata == NULL ||
      sample->sounddata->sels == NULL) goto noop;

  old_eb = edit_buffer_from_sample (sample);

  p = paste_over_data_new (old_eb, old_eb);
  inst->redo_data = inst->undo_data = p;
  set_active_op (sample, inst);

  do_filter_regions (sample, (SweepFilterRegion)pd->func, pd->pset,
		     pd->custom_data);

  if (sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    p->new_eb = edit_buffer_from_sample (sample);

    register_operation (sample, inst);
  }

  return;

 noop:
  sample_set_tmp_message (sample, _("No selection to process"));
}

static sw_operation filter_regions_op = {
  SWEEP_EDIT_MODE_FILTER,
  (SweepCallback)do_filter_regions_thread,
  (SweepFunction)g_free,
  (SweepCallback)undo_by_paste_over,
  (SweepFunction)paste_over_data_destroy,
  (SweepCallback)redo_by_paste_over,
  (SweepFunction)paste_over_data_destroy
};

sw_op_instance *
perform_filter_region_op (sw_sample * sample, char * desc,
			  SweepFilterRegion func,
			  sw_param_set pset, gpointer custom_data)
{
  sw_perform_data * pd = (sw_perform_data *)g_malloc (sizeof(*pd));

  pd->func = (SweepFunction)func;
  pd->pset = pset;
  pd->custom_data = custom_data;

  schedule_operation (sample, desc, &filter_regions_op, pd);

  return NULL;
}

static void
do_filter_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  sw_perform_data * pd = (sw_perform_data *)inst->do_data;

  SweepFilter func = (SweepFilter)pd->func;
  sw_param_set pset = pd->pset;
  void * custom_data = pd->custom_data;

  sw_edit_buffer * old_eb;
  paste_over_data * p;
  sw_sample * out;

  old_eb = edit_buffer_from_sample (sample);

  p = paste_over_data_new (old_eb, old_eb);
  inst->redo_data = inst->undo_data = p;
  set_active_op (sample, inst);

  out = func (sample, pset, custom_data);

  /* XXX: this is all kinda assuming out == sample if out != NULL */
  if (out != NULL && sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    p->new_eb = edit_buffer_from_sample (sample);

    register_operation (sample, inst);
  }
}

static sw_operation filter_op = {
  SWEEP_EDIT_MODE_FILTER,
  (SweepCallback)do_filter_thread,
  (SweepFunction)g_free,
  (SweepCallback)undo_by_paste_over,
  (SweepFunction)paste_over_data_destroy,
  (SweepCallback)redo_by_paste_over,
  (SweepFunction)paste_over_data_destroy
};


sw_op_instance *
perform_filter_op (sw_sample * sample, char * desc, SweepFilter func,
		   sw_param_set pset, gpointer custom_data)
{
  sw_perform_data * pd = (sw_perform_data *)g_malloc (sizeof(*pd));

  pd->func = (SweepFunction)func;
  pd->pset = pset;
  pd->custom_data = custom_data;

  schedule_operation (sample, desc, &filter_op, pd);

  return NULL;
}
