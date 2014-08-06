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
normalise (sw_sample * sample, sw_param_set pset, gpointer custom_data)
{
  sw_sounddata * sounddata;
  sw_format * f;
  GList * gl;
  sw_sel * sel;
  float * d;
  float max = 0;
  gfloat factor = 1.0;
  sw_framecount_t op_total, run_total;
  glong i;
  sw_framecount_t offset, remaining, n;

  gboolean active = TRUE;

  sounddata = sample_get_sounddata (sample);
  f = sounddata->format;

  op_total = sounddata_selection_nr_frames (sounddata) * 2 / 100;/* 2 passes */
  if (op_total == 0) op_total = 1;
  run_total = 0;

  /* Find max */
  for (gl = sounddata->sels; active && gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    offset = 0;
    remaining = sel->sel_end - sel->sel_start;

    while (active && remaining > 0) {
      g_mutex_lock (&sample->ops_mutex);

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

      g_mutex_unlock (&sample->ops_mutex);
    }
  }

  if (max != 0) factor = SW_AUDIO_MAX / (gfloat)max;

  /* Scale */
  for (gl = sounddata->sels; active && gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    offset = 0;
    remaining = sel->sel_end - sel->sel_start;

    while (active && remaining > 0) {
      g_mutex_lock (&sample->ops_mutex);

      if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
	active = FALSE;
      } else {
	d = sounddata->data + frames_to_bytes (f, sel->sel_start + offset);

	n = MIN(remaining, 1024);

	for (i=0; i < n * f->channels; i++) {
	  d[i] = (float)((gfloat)d[i] * factor);
	}

	remaining -= n;
	offset += n;

	run_total += n;
	sample_set_progress_percent (sample, run_total * 100 / op_total);
      }

      g_mutex_unlock (&sample->ops_mutex);
    }
  }

  return sample;
}

static sw_op_instance *
apply_normalise(sw_sample * sample, sw_param_set pset, gpointer custom_data)
{
  return
    perform_filter_op (sample, _("Normalise"), (SweepFilter)normalise,
		       pset, NULL);
}

static sw_procedure proc_normalise = {
  N_("Normalise"),
  N_("Alter the sample's amplitude to lie between 1.0 and -1.0"),
  "Conrad Parker",
  "Copyright (C) 2000",
  "http://sweep.sourceforge.net/plugins/normalise",
  "Filters/Normalise", /* identifier */
  GDK_n, /* accel_key */
  GDK_SHIFT_MASK, /* accel_mods */
  0, /* nr_params */
  NULL, /* param_specs */
  NULL, /* suggests() */
  apply_normalise,
  NULL, /* custom_data */
};

static GList *
normalise_init (void)
{
  return g_list_append ((GList *)NULL, &proc_normalise);
}


sw_plugin plugin = {
  normalise_init, /* plugin_init */
  NULL, /* plugin_cleanup */
};
