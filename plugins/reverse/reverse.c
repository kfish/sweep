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
#include <stdlib.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>

#include <sweep/sweep.h>

#include "../src/sweep_app.h" /* XXX */

static sw_sample *
sounddata_reverse (sw_sample * sample, sw_param_set pset,
		   gpointer custom_data)
{
  GList * gl;
  sw_sel * sel;
  glong i, sw;
  sw_sounddata * sounddata;
  sw_format * format;
  sw_framecount_t nr_frames;
  gpointer d, e, t;

  sw_framecount_t op_total, run_total;
  sw_framecount_t remaining, n;

  gboolean active = TRUE;

  sounddata = sample_get_sounddata (sample);
  format = sounddata->format;

  op_total = sounddata_selection_nr_frames (sounddata) / 200;
  if (op_total == 0) op_total = 1;
  run_total = 0;

  sw = frames_to_bytes (format, 1);
  t = alloca (sw);

  for (gl = sounddata->sels; active && gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    d = sounddata->data + frames_to_bytes (format, sel->sel_start);
    nr_frames = sel->sel_end - sel->sel_start;

    e = d + frames_to_bytes (format, nr_frames);

    remaining = nr_frames/2;

    while (active && remaining > 0) {
      g_mutex_lock (&sample->ops_mutex);

      if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
	active = FALSE;
      } else {
	n = MIN (remaining, 1024);

	for (i = 0; i <= n; i++) {
	  memcpy (t, d, sw);
	  memcpy (d, e, sw);
	  memcpy (e, t, sw);

	  d += sw;
	  e -= sw;
	}

	remaining -= n;

	run_total += n;
	sample_set_progress_percent (sample, run_total / op_total);
      }

      g_mutex_unlock (&sample->ops_mutex);
    }
  }

  return sample;
}

static sw_op_instance *
apply_reverse (sw_sample * sample, sw_param_set pset, gpointer custom_data)
{
  return
    perform_filter_op (sample, _("Reverse"),
		       (SweepFilter)sounddata_reverse, pset, NULL);
}


static sw_procedure proc_reverse = {
  N_("Reverse"),
  N_("Reverse selected regions of a sample"),
  "Conrad Parker",
  "Copyright (C) 2000",
  "http://sweep.sourceforge.net/plugins/reverse",
  "Filters/Reverse", /* identifier */
  GDK_f, /* accel_key */
  GDK_SHIFT_MASK, /* accel_mods */
  0, /* nr_params */
  NULL, /* param_specs */
  NULL, /* suggests() */
  apply_reverse,
  NULL, /* custom_data */
};

static GList *
reverse_init (void)
{
  return g_list_append ((GList *)NULL, &proc_reverse);
}


sw_plugin plugin = {
  reverse_init, /* plugin_init */
  NULL, /* plugin_cleanup */
};
