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


static sw_sdata *
normalise (sw_sdata * sdata, sw_param_set pset)
{
  sw_format * f = sdata->format;
  GList * gl;
  sw_sel * sel;
  sw_audio_t * d;
  sw_audio_t max = 0;
  gfloat factor;
  glong i, len;

  /* Find max */
  for (gl = sdata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    d = (sw_audio_t *)(sdata->data + frames_to_bytes (f, sel->sel_start));
    len = sel->sel_end - sel->sel_start;
    for (i=0; i < len * f->f_channels; i++) {
      if(d[i]>=0) max = MAX(max, d[i]);
      else max = MAX(max, -d[i]);
    }
  }

  factor = SW_AUDIO_T_MAX / (gfloat)max;

  /* Scale */
  for (gl = sdata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    d = (sw_audio_t *)(sdata->data + frames_to_bytes (f, sel->sel_start));
    len = sel->sel_end - sel->sel_start;
    for (i=0; i < len * f->f_channels; i++) {
      d[i] = (sw_audio_t)((gfloat)d[i] * factor);
    }
  }

  return sdata;
}

static sw_op_instance *
apply_normalise(sw_sample * sample, sw_param_set pset)
{
  return
    register_filter_op (sample, _("Normalise"), (SweepFilter)normalise, pset);
}

sw_proc proc_normalise = {
  _("Normalise"),
  _("Alter the sample's amplitude to lie between 1.0 and -1.0"),
  "Conrad Parker",
  "Copyright (C) 2000",
  "http://sweep.sourceforge.net/plugins/normalise",
  "Filters/Normalise", /* category */
  GDK_n, /* accel_key */
  GDK_SHIFT_MASK, /* accel_mods */
  0, /* nr_params */
  NULL, /* param_specs */
  NULL, /* suggests() */
  apply_normalise,
};
  
static void
normalise_init (gint * nr_procs, sw_proc **procs)
{
  *nr_procs = 1;
  *procs = &proc_normalise;
}


sw_plugin plugin = {
  normalise_init, /* plugin_init */
  NULL, /* plugin_cleanup */
};
