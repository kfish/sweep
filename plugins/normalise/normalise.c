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

static sw_sounddata *
normalise (sw_sounddata * sounddata, sw_param_set pset, gpointer custom_data)
{
  sw_format * f = sounddata->format;
  GList * gl;
  sw_sel * sel;
  sw_audio_t * d;
  sw_audio_t max = 0;
  gfloat factor;
  glong i, len;

  /* Find max */
  for (gl = sounddata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    d = (sw_audio_t *)(sounddata->data + frames_to_bytes (f, sel->sel_start));
    len = sel->sel_end - sel->sel_start;
    for (i=0; i < len * f->channels; i++) {
      if(d[i]>=0) max = MAX(max, d[i]);
      else max = MAX(max, -d[i]);
    }
  }

  factor = SW_AUDIO_T_MAX / (gfloat)max;

  /* Scale */
  for (gl = sounddata->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    d = (sw_audio_t *)(sounddata->data + frames_to_bytes (f, sel->sel_start));
    len = sel->sel_end - sel->sel_start;
    for (i=0; i < len * f->channels; i++) {
      d[i] = (sw_audio_t)((gfloat)d[i] * factor);
    }
  }

  return sounddata;
}

static sw_op_instance *
apply_normalise(sw_sample * sample, sw_param_set pset, gpointer custom_data)
{
  return
    register_filter_op (sample, _("Normalise"), (SweepFilter)normalise,
			pset, NULL);
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
