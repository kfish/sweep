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

#include <gdk/gdkkeysyms.h>

#include <sweep/sweep.h>

#include <../src/sweep_app.h> /* XXX */

static sw_sample *
fade (sw_sample * sample, gfloat start, gfloat end)
{
  sw_sounddata * sounddata;
  sw_format * f;
  GList * gl;
  sw_sel * sel;
  sw_audio_t * d;
  gfloat factor = start;
  sw_framecount_t op_total, run_total, frames_total;
  glong i, j;
  sw_framecount_t offset, remaining, n;

  gboolean active = TRUE;

  sounddata = sample_get_sounddata (sample);
  f = sounddata->format;

  op_total = sounddata_selection_nr_frames (sounddata) / 100;
  frames_total = sounddata_selection_nr_frames (sounddata);

  if (op_total == 0) op_total = 1;
  run_total = 0;

#if 0
  /* Find max */
  for (gl = sounddata->sels; active && gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    offset = 0;
    remaining = sel->sel_end - sel->sel_start;

    while (active && remaining > 0) {
      g_mutex_lock (sample->ops_mutex);

      if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
	active = FALSE;
      } else {

	d = sounddata->data + frames_to_bytes (f, sel->sel_start + offset);

	n = MIN(remaining, 1024);

	for (i=0; i < n * f->channels; i++) {
	  if(d[i]>=0) max = MAX(max, d[i]);
	  else max = MAX(max, -d[i]);
	}

	remaining -= n;
	offset += n;

	run_total += n;
	sample_set_progress_percent (sample, run_total / op_total);
      }

      g_mutex_unlock (sample->ops_mutex);
    }
  }

  if (max != 0) factor = SW_AUDIO_T_MAX / (gfloat)max;
#endif

  /* Fade */
  for (gl = sounddata->sels; active && gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    offset = 0;
    remaining = sel->sel_end - sel->sel_start;

    while (active && remaining > 0) {
      g_mutex_lock (sample->ops_mutex);

      if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
	active = FALSE;
      } else {
	d = sounddata->data + frames_to_bytes (f, sel->sel_start + offset);

	n = MIN(remaining, 1024);

	for (i = 0; i < n; i++)
	{
		factor = start + (end - start) * 
                  (gfloat)run_total++ / (gfloat)frames_total;

		for (j = 0; j < f->channels; j++)
		{
			d[i*f->channels+j] = (sw_audio_t)((gfloat)d[i*f->channels+j] * factor);
		}
	}

	remaining -= n;
	offset += n;

	sample_set_progress_percent (sample, run_total * frames_total);
      }

      g_mutex_unlock (sample->ops_mutex);
    }
  }

  return sample;
}

static sw_sample *
fade_in (sw_sample * sample, sw_param_set pset, gpointer custom_data)
{
  return fade (sample, 0.0, 1.0);
}

static sw_sample *
fade_out (sw_sample * sample, sw_param_set pset, gpointer custom_data)
{
  return fade (sample, 1.0, 0.0);
}

static sw_op_instance *
apply_fade_in (sw_sample * sample, sw_param_set pset, gpointer custom_data)
{
  return
    perform_filter_op (sample, _("Fade in"), (SweepFilter)fade_in,
		       pset, NULL);
}

static sw_op_instance *
apply_fade_out (sw_sample * sample, sw_param_set pset, gpointer custom_data)
{
  return
    perform_filter_op (sample, _("Fade out"), (SweepFilter)fade_out,
		       pset, NULL);
}

static sw_procedure proc_fade_in = {
  N_("Fade in"),
  N_("Apply a linear fade to the selection, fading in from silence"),
  "Conrad Parker",
  "Copyright (C) 2002",
  "http://sweep.sourceforge.net/plugins/fade",
  "Filters/Fade_In", /* identifier */
  0, /* accel_key */
  0, /* accel_mods */
  0, /* nr_params */
  NULL, /* param_specs */
  NULL, /* suggests() */
  apply_fade_in,
  NULL, /* custom_data */
};

static sw_procedure proc_fade_out = {
  N_("Fade out"),
  N_("Apply a linear fade to the selection, fading out to silence"),
  "Conrad Parker",
  "Copyright (C) 2002",
  "http://sweep.sourceforge.net/plugins/fade",
  "Filters/Fade_In", /* identifier */
  0, /* accel_key */
  0, /* accel_mods */
  0, /* nr_params */
  NULL, /* param_specs */
  NULL, /* suggests() */
  apply_fade_out,
  NULL, /* custom_data */
};
  
static GList *
fade_init (void)
{
  GList * gl = NULL;

  gl = g_list_append (gl, &proc_fade_in);
  gl = g_list_append (gl, &proc_fade_out);

  return gl;
}


sw_plugin plugin = {
  fade_init, /* plugin_init */
  NULL, /* plugin_cleanup */
};
