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
#include <math.h>

#include <gdk/gdkkeysyms.h>

#include <sweep/sweep.h>

#define NR_PARAMS 5

static sw_param_range resolution_range = {
  SW_RANGE_LOWER_BOUND_VALID|SW_RANGE_STEP_VALID,
  lower: {f: 0.001},
  step:  {f: 0.001}
};

static sw_param_range threshold_range = {
  SW_RANGE_ALL_VALID,
  lower: {f: 0.0},
  upper: {f: 1.0},
  step:  {f: 0.01}
};

static sw_param_range min_duration_range = {
  SW_RANGE_LOWER_BOUND_VALID|SW_RANGE_STEP_VALID,
  lower: {f: 0.0},
  step:  {f: 0.01}
};

static sw_param_range max_interruption_range = {
  SW_RANGE_LOWER_BOUND_VALID|SW_RANGE_STEP_VALID,
  lower: {f: 0.0},
  step:  {f: 0.01}
};

static sw_param_spec param_specs[] = {
  {
    N_("Select regions above threshold"),
    N_("Whether to select those regions lying above a given threshold "
       "or below it."),
    SWEEP_TYPE_BOOL,
    SW_PARAM_CONSTRAINED_NOT,
    {NULL},
  },
  {
    N_("Resolution"),
    N_("Width of energy detection window (s)"),
    SWEEP_TYPE_FLOAT,
    SW_PARAM_CONSTRAINED_RANGE,
    {range: &resolution_range}
  },
  {
    N_("Threshold"),
    N_("Energy level to detect [0.0 - 1.0]"),
    SWEEP_TYPE_FLOAT,
    SW_PARAM_CONSTRAINED_RANGE,
    {range: &threshold_range},
  },
  {
    N_("Minimum duration"),
    N_("Shortest region of selection to detect (s)"),
    SWEEP_TYPE_FLOAT,
    SW_PARAM_CONSTRAINED_RANGE,
    {range: &min_duration_range}
  },
  {
    N_("Maximum interruption"),
    N_("Longest length of sound above threshold to allow (s)"),
    SWEEP_TYPE_FLOAT,
    SW_PARAM_CONSTRAINED_RANGE,
    {range: &max_interruption_range}
  }
};

static void
by_energy_suggest (sw_sample * sample, sw_param_set pset, gpointer custom_data)
{
  pset[0].b = FALSE;
  pset[1].f = 0.02;
  pset[2].f = 0.2;
  pset[3].f = 0.2;
  pset[4].f = 0.06;
}

static void
select_by_energy (sw_sample * s, sw_param_set pset, gpointer custom_data)
{
  gboolean select_above = pset[0].b;
  gfloat resolution = pset[1].f;
  gfloat threshold = pset[2].f;
  gfloat min_duration_f = pset[3].f;
  gfloat max_interruption_f = pset[4].f;

  sw_sounddata * sounddata;
  float * d;
  glong window, win_s;
  gint i, doff;
  glong min_duration, max_interruption;
  glong length, loc=0;
  glong start=-1, end=-1;
  double di, energy, max_energy=0, factor=1.0;

  sounddata = sample_get_sounddata (s);

  window = (glong)(resolution * (gfloat)sounddata->format->rate);
  length = sounddata->nr_frames;
  min_duration = (glong)(min_duration_f * (gfloat)sounddata->format->rate);

  /* check (end-1 - (start+1)) > 0 */
  min_duration = MAX(2*window, min_duration);
  max_interruption = (glong)(max_interruption_f * (gfloat)sounddata->format->rate);

  d = (float *)sounddata->data;

  sounddata_lock_selection (sounddata);

  sounddata_clear_selection (sounddata);

  /* Find max for normalisation */

  length = sounddata->nr_frames;
  doff = 0;
  while (length > 0) {
    energy = 0;

    win_s = frames_to_samples (sounddata->format, MIN(length, window));

    /* calculate avg. for this window */
    for (i=0; i<win_s; i++) {
      di = (double)(d[doff+i] * factor);
      energy += fabs(di);
    }
    doff += win_s;

    energy /= (double)win_s;
    energy = sqrt(energy);

    max_energy = MAX(energy, max_energy);

    length -= window;
  }

  factor = SW_AUDIO_MAX / max_energy;

#ifdef DEBUG
  g_print ("factor: %f\tmax_energy: %f\n", factor, max_energy);
#endif

  threshold *= (gfloat)max_energy;

  length = sounddata->nr_frames;
  doff = 0;
  while (length > 0) {
    energy = 0;

    win_s = frames_to_samples (sounddata->format, MIN(length, window));

    /* calculate RMS energy for this window */
    for (i=0; i<win_s; i++) {
      di = (double)(d[doff+i]);
      energy += fabs(di);
    }
    doff += win_s;

    energy /= (double)win_s;
    energy = sqrt(energy);

#ifdef DEBUG
    g_print ("%ld\tenergy: %f\tthreshold: %f\n", loc, energy, threshold);
#endif

    /* Check if threshold condition is met */
    if (select_above ? (energy >= threshold) : (energy <= threshold)) {
      if (start == -1) {
	/* Not in possible selection; initialise start,end */
	end = start = loc;
      } else {
	end = loc;
      }
    } else if (end != -1) {
      if (loc - end > max_interruption) {
	if (end - start > min_duration) {
	  sounddata_add_selection_1 (sounddata, start+1, end-1);
	}
	end = start = -1;
      }
      /* else do nothing: keep start, end where they are */
    }

    loc += window;
    length -= window;
  }

  if (start != -1) {
    if (end - start > min_duration) {
      sounddata_add_selection_1 (sounddata, start, end);
    }
  }

  sounddata_unlock_selection (sounddata);
}

static sw_op_instance *
apply_by_energy(sw_sample * sample, sw_param_set pset, gpointer custom_data)
{
  return
    perform_selection_op (sample, _("Select by energy"),
			  (SweepFilter)select_by_energy, pset, NULL);
}

static sw_procedure proc_by_energy = {
  N_("Select by energy"),
  N_("Select loud or quiet regions"),
  "C. Parker, S. Pfeiffer",
  "Copyright (C) 2000 CSIRO Australia",
  "http://sweep.sourceforge.net/plugins/byenergy",
  "Filters/Select by energy", /* identifier */
  0, /* accel_key */
  0, /* accel_mods */
  NR_PARAMS, /* nr_params */
  param_specs, /* param_specs */
  by_energy_suggest, /* suggests() */
  apply_by_energy,
  NULL, /* custom data */
};

static GList *
by_energy_init (void)
{
  return g_list_append ((GList *)NULL, &proc_by_energy);
}


sw_plugin plugin = {
  by_energy_init, /* plugin_init */
  NULL, /* plugin_cleanup */
};
