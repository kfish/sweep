/*
 * LADSPA meta plugin for
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

/*
 * This file assumes that both LADSPA and Sweep are built with
 * an audio datatype of 'float'.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <math.h> /* for ceil() */

#include <glib.h>

#include <dlfcn.h>

#include <gdk/gdkkeysyms.h>

#include "sweep_types.h"
#include "sweep_typeconvert.h"
#include "sweep_filter.h"

#include "ladspa.h"

#ifndef __GNUC__
#error GCCisms used here. Please report this error to \
sweep-devel@lists.sourceforge.net stating your versions of sweep \
and your operating system and compiler.
#endif

/* Compile in support for inplace processing? */
#define _PROCESS_INPLACE

#ifdef _PROCESS_INPLACE
#define LADSPA_META_IS_INPLACE_BROKEN(x) LADSPA_IS_INPLACE_BROKEN(x)
#else
#define LADSPA_META_IS_INPLACE_BROKEN(x) (1L)
#endif

#define LADSPA_IS_CONTROL_INPUT(x) (LADSPA_IS_PORT_INPUT(x) && LADSPA_IS_PORT_CONTROL(x))
#define LADSPA_IS_AUDIO_INPUT(x) (LADSPA_IS_PORT_INPUT(x) && LADSPA_IS_PORT_AUDIO(x))
#define LADSPA_IS_CONTROL_OUTPUT(x) (LADSPA_IS_PORT_OUTPUT(x) && LADSPA_IS_PORT_CONTROL(x))
#define LADSPA_IS_AUDIO_OUTPUT(x) (LADSPA_IS_PORT_OUTPUT(x) && LADSPA_IS_PORT_AUDIO(x))

#define LADSPA_frames_to_bytes(f) (f * sizeof(LADSPA_Data))

static char * default_ladspa_path = "/usr/lib/ladspa:/usr/local/lib/ladspa:/opt/ladspa/lib";

static GList * modules_list = NULL;
static gboolean ladspa_meta_initialised = FALSE;

/*
 * is_usable (d)
 *
 * Determine if a LADSPA_Descriptor * d is usable by this ladspameta
 * sweep plugin. Currently this means that:
 *   1. there is at least one audio output
 *   2. the number of audio inputs must equal the number of audio outputs.
 */
static gboolean
is_usable(const LADSPA_Descriptor * d)
{
  LADSPA_PortDescriptor pd;
  gint i;
  gint
    nr_ai=0, /* audio inputs */
    nr_ao=0; /* audio outputs */

  for (i=0; i < d->PortCount; i++) {
    pd = d->PortDescriptors[i];
    if (LADSPA_IS_AUDIO_INPUT(pd))
      nr_ai++;
    if (LADSPA_IS_AUDIO_OUTPUT(pd))
      nr_ao++;
  }

  if (nr_ao == 0) return FALSE;

  /* Sanity checks */
  if (! d->run) return FALSE; /* plugin does nothing! */
  if (! d->instantiate) return FALSE; /* plugin cannot be instantiated */
  if (! d->connect_port) return FALSE; /* plugin cannot be wired up */

  return (nr_ai == nr_ao);
}

static sw_param_type
convert_type (const LADSPA_PortRangeHintDescriptor prhd)
{
  if (LADSPA_IS_HINT_TOGGLED(prhd))
    return SWEEP_TYPE_BOOL;
  else if (LADSPA_IS_HINT_INTEGER(prhd))
    return SWEEP_TYPE_INT;
  else
    return SWEEP_TYPE_FLOAT;
}

static int
get_valid_mask (const LADSPA_PortRangeHintDescriptor prhd)
{
  int ret=0;

  if (LADSPA_IS_HINT_BOUNDED_BELOW(prhd))
    ret &= SW_RANGE_LOWER_BOUND_VALID;
  if (LADSPA_IS_HINT_BOUNDED_ABOVE(prhd))
    ret &= SW_RANGE_UPPER_BOUND_VALID;

  return ret;
}

static sw_param_range *
convert_constraint (const LADSPA_PortRangeHint * prh)
{
  sw_param_range * pr;
  LADSPA_PortRangeHintDescriptor prhd = prh->HintDescriptor;

  if (LADSPA_IS_HINT_TOGGLED(prhd))
    return NULL;

  pr = g_malloc0 (sizeof (*pr));

  pr->valid_mask = get_valid_mask (prhd);

  if (LADSPA_IS_HINT_INTEGER(prhd)) {
    if (LADSPA_IS_HINT_BOUNDED_BELOW(prhd))
      pr->lower.i = (sw_int)prh->LowerBound;
    if (LADSPA_IS_HINT_BOUNDED_ABOVE(prhd))
      pr->upper.i = (sw_int)prh->UpperBound;
  } else {
    if (LADSPA_IS_HINT_BOUNDED_BELOW(prhd))
      pr->lower.f = (sw_float)prh->LowerBound;
    if (LADSPA_IS_HINT_BOUNDED_ABOVE(prhd))
      pr->upper.f = (sw_float)prh->UpperBound;
  }

  return pr;
}

typedef struct _lm_custom lm_custom;

struct _lm_custom {
  const LADSPA_Descriptor * d;
  sw_param_spec * param_specs;
};

static lm_custom *
lm_custom_new (const LADSPA_Descriptor * d, sw_param_spec * param_specs)
{
  lm_custom * lmc;

  lmc = g_malloc (sizeof (*lmc));
  if (lmc) {
    lmc->d = d;
    lmc->param_specs = param_specs;
  }

  return lmc;
}

static void
ladspa_meta_apply_region (gpointer pcmdata, sw_format * format,
			  gint nr_frames,
			  sw_param_set pset, gpointer custom_data)
{
  lm_custom * lm = (lm_custom *)custom_data;
  const LADSPA_Descriptor * d = lm->d;
  sw_param_spec * param_specs = lm->param_specs;

  LADSPA_Handle * handle;
  LADSPA_Data ** input_buffers, ** output_buffers;
  LADSPA_Data * mono_input_buffer=NULL;
  LADSPA_Data * p;
  LADSPA_Data * control_inputs;
  LADSPA_Data dummy_control_output;
  LADSPA_PortDescriptor pd;
  glong length_b;
  gulong port_i; /* counter for iterating over ports */
  gint i, j, n;

  /* The number of times the plugin will be run; ie. if the number of
   * channels in the input pcmdata is greater than the number of
   * audio ports on the ladspa plugin, the plugin will be run
   * multiple times until enough output channels have been calculated.
   */
  gint iterations;

  /* Enumerate the numbers of each type of port on the ladspa plugin */
  gint
    nr_ci=0, /* control inputs */
    nr_ai=0, /* audio inputs */
    nr_co=0, /* control outputs */
    nr_ao=0; /* audio outputs */

  /* The number of audio channels to be processed */
  gint nr_channels = format->channels;

  /* The number of input and output buffers to use */
  gint nr_i=0, nr_o=0;

  /* Counters for allocating input and output buffers */
  gint ibi=0, obi=0;


  /* instantiate the ladspa plugin */
  handle = d->instantiate (d, (long)format->rate);

  /* Cache how many of each type of port this ladspa plugin has */
  for (port_i=0; port_i < d->PortCount; port_i++) {
    pd = d->PortDescriptors[(int)port_i];
    if (LADSPA_IS_CONTROL_INPUT(pd))
      nr_ci++;
    if (LADSPA_IS_AUDIO_INPUT(pd))
      nr_ai++;
    if (LADSPA_IS_CONTROL_OUTPUT(pd))
      nr_co++;
    if (LADSPA_IS_AUDIO_OUTPUT(pd))
      nr_ao++;
  }

  /* Basic assumption of this meta plugin, which was
   * checked above in is_usable(); nb. for future expansion
   * much of this routine is written to accomodate this
   * assumption being incorrect.
   */
  g_assert (nr_ai == nr_ao);

  /* Basic assumption that this plugin has audio output.
   * Also important as we are about to divide by nr_ao.
   */
  g_assert (nr_ao > 0);

  iterations = (gint) ceil(((double)nr_channels) / ((double)nr_ao));

  /* Numbers of input and output buffers: ensure
   * nr_i >= nr_channels && nr_o >= nr_channels
   */
  nr_i = iterations * nr_ai;
  nr_o = iterations * nr_ao;

  if ((nr_channels == 1) && (nr_ai == 1) && (nr_ao >= 1)) {
    /*
     * Processing a mono sample with a mono filter.
     * Attempt to do this in place.
     */

    /* Copy PCM data if this ladspa plugin cannot work inplace */
    if (LADSPA_META_IS_INPLACE_BROKEN(d->Properties)) {
      length_b = frames_to_bytes (format, nr_frames);
      mono_input_buffer = g_malloc (length_b);
      input_buffers = &mono_input_buffer;
    } else {
      input_buffers = (LADSPA_Data **)&pcmdata;
    }
    
    output_buffers = (LADSPA_Data **)&pcmdata;

  } else {
    length_b = LADSPA_frames_to_bytes (nr_frames);

    /* Allocate zeroed input buffers; these will remain zeroed
     * if there aren't enough channels in the input pcmdata
     * to use them.
     */
    input_buffers = g_malloc (sizeof(LADSPA_Data *) * nr_i);
    for (i=0; i < nr_i; i++) {
      input_buffers[i] = g_malloc0 (length_b);
    }

    output_buffers = g_malloc(sizeof(LADSPA_Data *) * nr_o);

    /* Create separate output buffers if this ladspa plugin cannot
     * work inplace */
    if (LADSPA_META_IS_INPLACE_BROKEN(d->Properties)) {
      for (i=0; i < nr_o; i++) {
	output_buffers[i] = g_malloc (length_b);
      }
    } else {
      /* Re-use the input buffers, directly mapping them to
       * corresponding output buffers
       */
      for (i=0; i < MIN(nr_i, nr_o); i++) {
	output_buffers[i] = input_buffers[i];
      }
      /* Create some extra output buffers if nr_o > nr_i */
      for (; i < nr_o; i++) {
	output_buffers[i] = g_malloc (length_b);
      }
    }
  }

  /* Copy data into input buffers */
  if (nr_channels == 1) {
    if (!LADSPA_META_IS_INPLACE_BROKEN(d->Properties)) {
      length_b = frames_to_bytes (format, nr_frames);
      memcpy (input_buffers[0], pcmdata, length_b);
    } /* else we're processing in-place, so we haven't needed to set
       * up a separate input buffer; input_buffers[0] actually
       * points to pcmdata hence we don't do any copying here.
       */
  } else {
    /* de-interleave multichannel data */

    p = (LADSPA_Data *)pcmdata;

    for (n=0; n < nr_channels; n++) {
      for (i=0; i < nr_frames; i++) {
	input_buffers[n][i] = *p++;
      }
    }
  }

  /* connect control ports */
  control_inputs = g_malloc (nr_ci * sizeof(LADSPA_Data));
  j=0;
  for (port_i=0; port_i < d->PortCount; port_i++) {
    pd = d->PortDescriptors[(int)port_i];
    if (LADSPA_IS_CONTROL_INPUT(pd)) {
      /* do something with pset! */
      switch (param_specs[j].type) {
      case SWEEP_TYPE_BOOL:
	/* from ladspa.h:
	 * Data less than or equal to zero should be considered
	 * `off' or `false,'
	 * and data above zero should be considered `on' or `true.'
	 */
	control_inputs[j] = pset[j].b ? 1.0 : 0.0;
	break;
      case SWEEP_TYPE_INT:
	control_inputs[j] = (LADSPA_Data)pset[j].i;
	break;
      case SWEEP_TYPE_FLOAT:
	control_inputs[j] = pset[j].f;
	break;
      default:
	/* This plugin should produce no other types */
	g_assert_not_reached ();
	break;
      }
      d->connect_port (handle, port_i, &control_inputs[j]);
      j++;
    }
    if (LADSPA_IS_CONTROL_OUTPUT(pd)) {
      d->connect_port (handle, port_i, &dummy_control_output);
    }
  }

  /* run the plugin as many times as necessary */
  while (iterations--) {

    /* connect input and output audio buffers to the
     * audio ports of the ladspa plugin */
    for (port_i=0; port_i < d->PortCount; port_i++) {
      pd = d->PortDescriptors[(int)port_i];
      if (LADSPA_IS_AUDIO_INPUT(pd)) {
	d->connect_port (handle, port_i, input_buffers[ibi++]);
      }
      if (LADSPA_IS_AUDIO_OUTPUT(pd)) {
	d->connect_port (handle, port_i, output_buffers[obi++]);
      }
    }

    /* activate the ladspa plugin */
    if (d->activate)
      d->activate (handle);

    /* run the ladspa plugin */
    d->run (handle, nr_frames);

    /* deactivate the ladspa plugin */
    if (d->deactivate)
      d->deactivate (handle);
  }

  /* re-interleave data */
  if (nr_channels > 1) {
    p = (LADSPA_Data *)pcmdata;

    for (n=0; n < nr_channels; n++) {
      for (i=0; i < nr_frames; i++) {
	*p++ = output_buffers[n][i];
      }
    }
  }

  /* let the ladspa plugin clean up after itself */
  if (d->cleanup)
    d->cleanup (handle);

  /* free the input and output buffers */
  if (control_inputs) g_free (control_inputs);

  if ((nr_channels == 1) && (nr_ai == 1) && (nr_ao >= 1)) {
    if (LADSPA_META_IS_INPLACE_BROKEN(d->Properties)) {
      g_free (mono_input_buffer);
    }
  } else {

    /* free the output buffers */
    for (i=0; i < nr_o; i++) {
      g_free (output_buffers[i]);
    }
    g_free (output_buffers);

    /* free the input buffers, if we created some */
    if (LADSPA_META_IS_INPLACE_BROKEN(d->Properties)) {
      for (i=0; i < nr_i; i++) {
	g_free (input_buffers[i]);
      }
    } else {
      /* inplace worked, but if (nr_i > nr_o), then
       * we still need to free the last input buffers
       **/
      for (i=nr_o; i < nr_i; i++) {
	g_free (input_buffers[i]);
      }
    }
    g_free (input_buffers);  
  }
}

static sw_op_instance *
ladspa_meta_apply (sw_sample * sample,
		   sw_param_set pset, gpointer custom_data)
{
  lm_custom * lm = (lm_custom *)custom_data;
  const LADSPA_Descriptor * d = lm->d;

  return
    register_filter_region_op
    (sample, (char *)d->Name, (SweepFilterRegion)ladspa_meta_apply_region,
     pset, custom_data);
}

/*
 * ladspa_meta_add_procs (dir, name, gl)
 *
 * form sweep procs to describe the ladspa plugin functions that
 * are in the shared library file "dir/name",
 * and add these procs to the GList * (*gl)
 */
static void
ladspa_meta_add_procs (gchar * dir, gchar * name, GList ** gl)
{
#define PATH_LEN 256
  gchar path[PATH_LEN];
  void * module;
  LADSPA_Descriptor_Function desc_func;
  const LADSPA_Descriptor * d;
  LADSPA_PortDescriptor pd;
  gint i, j, k, nr_params;
  int valid_mask;
  sw_proc * proc;

  snprintf (path, PATH_LEN, "%s/%s", dir, name);

  module = dlopen (path, RTLD_NOW);
  if (!module) return;

  modules_list = g_list_append (modules_list, module);

  if ((desc_func = dlsym (module, "ladspa_descriptor"))) {
    for (i=0; (d = desc_func (i)) != NULL; i++) {

      if (!is_usable(d))
	continue;

      proc = g_malloc0 (sizeof (*proc));
      proc->name = d->Name;
      proc->author = d->Maker;
      proc->copyright = d->Copyright;

      nr_params=0;
      for (j=0; j < d->PortCount; j++) {
	pd = d->PortDescriptors[j];
	if (LADSPA_IS_CONTROL_INPUT(pd)) {
	  nr_params++;
	}
      }

      proc->nr_params = nr_params;
      proc->param_specs =
	(sw_param_spec *)g_malloc0 (nr_params * sizeof (sw_param_spec));

      k=0;
      for (j=0; j < d->PortCount; j++) {
	pd = d->PortDescriptors[j];
	if (LADSPA_IS_CONTROL_INPUT(pd)) {
	  proc->param_specs[k].name = d->PortNames[j];
	  proc->param_specs[k].desc = d->PortNames[j];
	  proc->param_specs[k].type =
	    convert_type (d->PortRangeHints[j].HintDescriptor);
	  valid_mask = get_valid_mask (d->PortRangeHints[j].HintDescriptor);
	  if (valid_mask == 0) {
	    proc->param_specs[k].constraint_type = SW_PARAM_CONSTRAINED_NOT;
	  } else {
	    proc->param_specs[k].constraint_type = SW_PARAM_CONSTRAINED_RANGE;
	    proc->param_specs[k].constraint.range =
	      convert_constraint (&d->PortRangeHints[j]);
	  }
	  k++;
	}
      }

      proc->apply = ladspa_meta_apply;

      proc->custom_data = lm_custom_new (d, proc->param_specs);

      *gl = g_list_append (*gl, proc);
    }
  }
}

/*
 * ladspa_meta_init_dir (dir, gl)
 *
 * scan a directory "dirname" for LADSPA plugins, and attempt to load
 * each of them.
 */
static void
ladspa_meta_init_dir (gchar * dirname, GList ** gl)
{
  DIR * dir;
  struct dirent * dirent;

  if (!dirname) return;

  dir = opendir (dirname);
  if (!dir) {
    return;
  }

  while ((dirent = readdir (dir)) != NULL) {
    ladspa_meta_add_procs (dirname, dirent->d_name, gl);
  }
}

static GList *
ladspa_meta_init (void)
{
  GList * gl = NULL;
  char * ladspa_path=NULL;
  char * next_sep=NULL;
  char * saved_lp=NULL;

  /* If this ladspa_meta module has already been initialised, don't
   * initialise again until cleaned up.
   */
  if (ladspa_meta_initialised)
    return NULL;

  ladspa_path = getenv ("LADSPA_PATH");
  if (!ladspa_path)
    ladspa_path = saved_lp = strdup(default_ladspa_path);

  do {
    next_sep = strchr (ladspa_path, ':');
    if (next_sep != NULL) *next_sep = '\0';
    
    ladspa_meta_init_dir (ladspa_path, &gl);

    if (next_sep != NULL) ladspa_path = ++next_sep;

  } while ((next_sep != NULL) && (*next_sep != '\0'));

  ladspa_meta_initialised = TRUE;

  /* free string if dup'd for ladspa_path */
  if (saved_lp != NULL) free(saved_lp);

  return gl;
}

static void
ladspa_meta_cleanup (void)
{
  GList * gl;

  if (!ladspa_meta_initialised) return;

  for (gl = modules_list; gl; gl = gl->next) {
    dlclose(gl->data);
  }
}

sw_plugin plugin = {
  ladspa_meta_init, /* plugin_init */
  ladspa_meta_cleanup, /* plugin_cleanup */
};
