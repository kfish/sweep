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

#include "sweep_types.h"
#include "sweep_typeconvert.h"
#include "sweep_filter.h"
#include "i18n.h"


#define NR_PARAMS 5

#ifndef __GNUC__
#error GCCisms used here. Please report this error to \
sweep-devel@lists.sourceforge.net stating your versions of sweep \
and your operating system and compiler.
#endif


static sw_param_range delay_range = {
  SW_RANGE_LOWER_BOUND_VALID,
  lower: {i: 0},
};

static sw_param_range gain_range = {
  SW_RANGE_LOWER_BOUND_VALID|SW_RANGE_UPPER_BOUND_VALID,
  lower: {f: 0.0},
  upper: {f: 1.0},
};

static sw_param stix_list[] = {
  {i: 4},
  {s: "With a fork"},
  {s: "With a spoon"},
  {s: "With false teeth"},
  {s: "With Nigel's bum"}
};

static sw_param pants_list[] = {
  {i: 7},
  {i: 0},
  {i: 1},
  {i: 2},
  {i: 7},
  {i: 42},
  {i: 44100},
  {i: 1000000}
};

static sw_param_spec param_specs[] = {
  {
    "Delay",
    "Number of frames to delay by",
    SWEEP_TYPE_INT,
    SW_PARAM_CONSTRAINED_RANGE,
    {range: &delay_range}
  },
  {
    "Gain",
    "Gain with which to mix in delayed signal",
    SWEEP_TYPE_FLOAT,
    SW_PARAM_CONSTRAINED_RANGE,
    {range: &gain_range}
  },
  {
    "Flim",
    "Should you manage your flim?",
    SWEEP_TYPE_BOOL,
    SW_PARAM_CONSTRAINED_NOT,
    {NULL}
  },
  {
    "Stix",
    "Method of eating beans",
    SWEEP_TYPE_STRING,
    SW_PARAM_CONSTRAINED_LIST,
    {list: &stix_list}
  },
  {
    "Pants methodology",
    "How many pants should you wear per day?",
    SWEEP_TYPE_INT,
    SW_PARAM_CONSTRAINED_LIST,
    {list: &pants_list}
  }
};

static void
echo_suggest (sw_sample * sample, sw_param_set pset, gpointer custom_data)
{
  pset[0].i = 2000;
  pset[1].f = 0.4;
  pset[2].b = TRUE;
  pset[3].s = "With a fork";
}

static void
region_echo (gpointer data, sw_format * format, gint nr_frames,
	     sw_param_set pset, gpointer custom_data)
{
  glong i, sw;
  sw_audio_t * d, * e;
  gpointer ep;
  gint dlen;
  gint delay = pset[0].i;
  gfloat gain = pset[1].f;

  sw = frames_to_bytes (format, 1);

  d = (sw_audio_t *)data;
  ep = data + frames_to_bytes (format, delay);
  e = (sw_audio_t *)ep;

  if (delay > nr_frames) return;

  dlen = frames_to_samples (format, nr_frames - delay);

  for (i = 0; i < dlen; i++) {
    e[i] += (sw_audio_t)((gfloat)(d[i]) * gain);
  }
}

static sw_op_instance *
echo_apply (sw_sample * sample, sw_param_set pset, gpointer custom_data)
{
  return
    register_filter_region_op (sample, _("Echo"),
			       (SweepFilterRegion)region_echo, pset, NULL);
}


static sw_proc proc_echo = {
  _("Echo"),
  _("Apply an echo to selected regions of a sample"),
  "Conrad Parker",
  "Copyright (C) 2000",
  "http://sweep.sourceforge.net/plugins/echo",
  "Filters/Echo", /* category */
  GDK_e, /* accel_key */
  GDK_SHIFT_MASK, /* accel_mods */
  NR_PARAMS, /* nr_params */
  param_specs, /* param_specs */
  echo_suggest, /* suggests() */
  echo_apply,
  NULL /* custom_data */
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
