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

#include "config.h"


#ifndef __GNUC__
#error GCCisms used here. Please report this error to \
sweep-devel@lists.sourceforge.net stating your versions of sweep \
and your operating system and compiler.
#endif


static sw_param stix_list[] = {
  {i: 4},
  {s: N_("With a fork")},
  {s: N_("With a spoon")},
  {s: N_("With false teeth")},
  {s: N_("With Nigel's bum")}
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

static sw_param_spec example_filter_region_param_specs[] = {
  {
    N_("Flim"),
    N_("Should you manage your flim?"),
    SWEEP_TYPE_BOOL,
    SW_PARAM_CONSTRAINED_NOT,
    {NULL},
    SW_PARAM_HINT_DEFAULT
  },
  {
    N_("Beans"),
    N_("Method of eating beans"),
    SWEEP_TYPE_STRING,
    SW_PARAM_CONSTRAINED_LIST,
    {list: (sw_param *)&stix_list},
    SW_PARAM_HINT_DEFAULT
  },
  {
    N_("Pants methodology"),
    N_("How many pants should you wear per day?"),
    SWEEP_TYPE_INT,
    SW_PARAM_CONSTRAINED_LIST,
    {list: (sw_param *)&pants_list},
    SW_PARAM_HINT_DEFAULT
  }
};

static void
example_filter_region_suggest (sw_sample * sample, sw_param_set pset,
			       gpointer custom_data)
{
  pset[0].b = TRUE;
  pset[1].s = N_("With a fork");
  pset[2].i = 7;
}

static void
example_filter_region_func (gpointer data, sw_format * format, gint nr_frames,
			    sw_param_set pset, gpointer custom_data)
{
  gboolean flim = pset[0].b;
  gchar * beans = pset[1].s;
  gint nr_pants = pset[2].i;

  if (flim) {
    /* manage flim */
  }

  if (!strcmp(beans, "With a spoon")) {
    /* eat beans with a spoon */
  } else {
    /* spill beans everywhere */
  }

  if (nr_pants > 1000) {
    /* We're wearing too many pants! */
    return;
  }

  /* Do filtering stuff */

}

static sw_op_instance *
example_filter_region_apply (sw_sample * sample, sw_param_set pset,
			     gpointer custom_data)
{
  return
    perform_filter_region_op (sample, _("Example Filter Region"),
			      (SweepFilterRegion)example_filter_region_func,
			      pset, NULL);
}


static sw_procedure proc_example_filter_region = {
  N_("Example Filter Region"),
  N_("An example filter region plugin"),
  "Conrad Parker",
  "Copyright (C) 2000",
  "http://sweep.sourceforge.net/plugins/example",
  "Example", /* category */
  0, /* accel_key */
  0, /* accel_mods */
  3, /* nr_params */
  example_filter_region_param_specs, /* param_specs */
  example_filter_region_suggest, /* suggests() */
  example_filter_region_apply,
  NULL, /* custom_data */
};

static GList *
example_init (void)
{
  return g_list_append ((GList *)NULL, &proc_example_filter_region);
}


sw_plugin plugin = {
  example_init, /* plugin_init */
  NULL, /* plugin_cleanup */
};
