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


#define NR_PARAMS 2

#ifndef __GNUC__
#error GCCisms used here. Please report this error to \
sweep-devel@lists.sourceforge.net stating your versions of sweep \
and your operating system and compiler.
#endif


static sw_param_range delay_range = {
  SW_RANGE_LOWER_BOUND_VALID|SW_RANGE_STEP_VALID,
  lower: {f: 0.0},
  step:  {f: 0.001}
};

static sw_param_range gain_range = {
  SW_RANGE_ALL_VALID,
  lower: {f: 0.0},
  upper: {f: 1.0},
  step: {f: 0.01}
};


static sw_param_spec param_specs[] = {
  {
    N_("Delay"),
    N_("Time to delay by"),
    SWEEP_TYPE_FLOAT,
    SW_PARAM_CONSTRAINED_RANGE,
    {range: &delay_range},
    SW_PARAM_HINT_TIME
  },
  {
    N_("Gain"),
    N_("Gain with which to mix in delayed signal"),
    SWEEP_TYPE_FLOAT,
    SW_PARAM_CONSTRAINED_RANGE,
    {range: &gain_range},
    SW_PARAM_HINT_DEFAULT
  },
};

static void
echo_suggest (sw_sample * sample, sw_param_set pset, gpointer custom_data)
{
  pset[0].f = 0.0;
  pset[1].f = 0.0;
}

static void
region_echo (gpointer data, sw_format * format, sw_framecount_t nr_frames,
	     sw_param_set pset, gpointer custom_data)
{
  gfloat delay = pset[0].f;
  gfloat gain = pset[1].f;

  sw_framecount_t i, delay_f, dlen_s;
  float * d, * e;
  gpointer ep;

  delay_f = time_to_frames (format, delay);

  d = (float *)data;
  ep = data + frames_to_bytes (format, delay_f);
  e = (float *)ep;

  if (delay > nr_frames) return;

  dlen_s = frames_to_samples (format, nr_frames - delay_f);

  for (i = 0; i < dlen_s; i++) {
    e[i] += (float)((gfloat)(d[i]) * gain);
  }
}

static sw_op_instance *
echo_apply (sw_sample * sample, sw_param_set pset, gpointer custom_data)
{
  return
    perform_filter_region_op (sample, _("Echo"),
			      (SweepFilterRegion)region_echo, pset, NULL);
}


static sw_procedure proc_echo = {
  N_("Echo"),
  N_("Apply an echo to selected regions of a sample"),
  "Conrad Parker",
  "Copyright (C) 2000",
  "http://sweep.sourceforge.net/plugins/echo",
  "Filters/Echo", /* identifier */
  GDK_e, /* accel_key */
  GDK_SHIFT_MASK, /* accel_mods */
  NR_PARAMS, /* nr_params */
  param_specs, /* param_specs */
  echo_suggest, /* suggests() */
  echo_apply,
  NULL, /* custom_data */
};

static GList *
echo_init (void)
{
  return g_list_append ((GList *)NULL, &proc_echo);
}


sw_plugin plugin = {
  echo_init, /* plugin_init */
  NULL, /* plugin_cleanup */
};
