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

#include "sweep.h"
#include "edit.h"
#include "format.h"
#include "filter_ops.h"
#include "sample.h"
#include "undo.h"


static void
region_reverse (gpointer data, sw_format * format, int nr_frames,
		sw_param_set pset)
{
  glong i, sw;
  gpointer d, e, t;

  sw = frames_to_bytes (format, 1);
  t = g_malloc (sw);

  d = data;
  e = d + frames_to_bytes (format, nr_frames);

  for (i = 0; i <= nr_frames/2; i++) {
    memcpy (t, d, sw);
    memcpy (d, e, sw);
    memcpy (e, t, sw);
    
    d += sw;
    e -= sw;
  }

  g_free (t);
}

static sw_op_instance *
apply_reverse (sw_sample * sample, sw_param_set pset)
{
  return
    register_filter_region_op (sample, _("Reverse"),
			       (SweepFilterRegion)region_reverse, pset);
}


sw_proc proc_reverse = {
  _("Reverse"),
  _("Reverse selected regions of a sample"),
  "Conrad Parker",
  "Copyright (C) 2000",
  "http://sweep.sourceforge.net/plugins/reverse",
  "Filters/Reverse", /* category */
  GDK_f, /* accel_key */
  GDK_SHIFT_MASK, /* accel_mods */
  0, /* nr_params */
  NULL, /* param_specs */
  NULL, /* suggests() */
  apply_reverse,
};

static void
reverse_init (gint * nr_procs, sw_proc **procs)
{
  *nr_procs = 1;
  *procs = &proc_reverse;
}


sw_plugin plugin = {
  reverse_init, /* plugin_init */
  NULL, /* plugin_cleanup */
};
