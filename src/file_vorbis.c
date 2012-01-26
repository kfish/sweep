/*
 * Sweep, a sound wave editor.
 *
 * Copyright (C) 2000 Conrad Parker
 * Copyright (C) 2002 CSIRO Australia
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
 * This file adapted from "decoder_example.c" and "encoder_example.c" in
 * the OggVorbis software codec source code, Copyright (C) 1994-2002 by
 * the XIPHOPHORUS Company.
 *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS
 * GOVERNED BY the following BSD-style license.
 *
 * Copyright (c) 2002, Xiph.org Foundation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of the Xiph.org Foundation nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_OGGVORBIS

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>

#define OV_EXCLUDE_STATIC_CALLBACKS

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include <vorbis/vorbisenc.h>

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_types.h>
#include <sweep/sweep_typeconvert.h>
#include <sweep/sweep_sample.h>
#include <sweep/sweep_undo.h>
#include <sweep/sweep_sounddata.h>

#include "sample.h"
#include "interface.h"
#include "file_dialogs.h"
#include "file_sndfile.h"
#include "question_dialogs.h"
#include "preferences.h"
#include "print.h"
#include "view.h"

#include "../pixmaps/white-ogg.xpm"
#include "../pixmaps/vorbisword2.xpm"
#include "../pixmaps/xifish.xpm"

#define QUALITY_KEY "OggVorbis_Quality"
#define ABR_KEY "OggVorbis_ABR"
#define NOMINAL_KEY "OggVorbis_NominalBR"
#define MINIMUM_KEY "OggVorbis_MinBR"
#define MAXIMUM_KEY "OggVorbis_MaxBR"
#define SERIALNO_KEY "OggVorbis_Serialno"

#define DEFAULT_NOMINAL 128L
#define DEFAULT_QUALITY 3.0

extern GtkStyle * style_bw;

#ifdef DEVEL_CODE
typedef struct _sw_metadata sw_metadata;

struct _sw_metadata {
  char name [128];
  char content [512];
};

static sw_metadata *
vorbis_metadata_from_str (char * str)
{
  sw_metadata * meta = NULL;
  gint i;

  for (i = 0; str[i]; i++) {
    if (i == 0) {
      str[i] = toupper(str[i]);
    } else {
      str[i] = tolower(str[i]);
    }

    if (str[i] == '=') {
      str[i] = '\0';
      meta = g_malloc (sizeof (sw_metadata));
      snprintf (meta->name, sizeof (meta->name), "%s", str);
      snprintf (meta->content, sizeof (meta->content), "%s", str + i +1);
      break;
    }
  }

  return meta;
}
#endif /* DEVEL_CODE */

static sw_sample *
sample_load_vorbis_data (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  OggVorbis_File * vf = (OggVorbis_File *)sample->file_info;
  int channels;
  float ** pcm;
  int i, j;
  float * d;
  sw_framecount_t remaining, n, run_total;
  sw_framecount_t cframes;
  gint percent;

  int bitstream;

  struct stat statbuf;

  gboolean active = TRUE;

  channels = sample->sounddata->format->channels;

  remaining = sample->sounddata->nr_frames;
  run_total = 0;

  d = sample->sounddata->data;

  cframes = remaining / 100;
  if (cframes == 0) cframes = 1;

  while (active && remaining > 0) {
    g_mutex_lock (sample->ops_mutex);

    if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
      active = FALSE;
    } else {
      n = MIN (remaining, 1024);

#ifdef OV_READ_FLOAT_THREE_ARGS
      n = ov_read_float (vf, &pcm, &bitstream);
#else
      n = ov_read_float (vf, &pcm, n, &bitstream);
#endif

      if (n == 0) {
	/* EOF */
	remaining = 0;
      } else if (n < 0) {
	/* XXX: corrupt data; ignore? */
      } else {

	for (i = 0; i < channels; i++) {
	  for (j = 0; j < n; j++) {
	    d[j*channels + i] = pcm[i][j];
	  }
	}

	d += (n * channels);

	remaining -= n;

	run_total += n;
	percent = run_total / cframes;
	sample_set_progress_percent (sample, percent);
      }
    }

    g_mutex_unlock (sample->ops_mutex);
  }

  ov_clear (vf);

  stat (sample->pathname, &statbuf);
  sample->last_mtime = statbuf.st_mtime;
  sample->edit_ignore_mtime = FALSE;
  sample->modified = FALSE;

  sample_set_edit_state (sample, SWEEP_EDIT_STATE_DONE);

  return sample;
}

static sw_operation vorbis_load_op = {
  SWEEP_EDIT_MODE_FILTER,
  (SweepCallback)sample_load_vorbis_data,
  (SweepFunction)NULL,
  (SweepCallback)NULL, /* undo */
  (SweepFunction)NULL,
  (SweepCallback)NULL, /* redo */
  (SweepFunction)NULL
};

static sw_sample *
sample_load_vorbis_info (sw_sample * sample, char * pathname)
{
  FILE * f;
  OggVorbis_File * vf;
  vorbis_info * vi;
  int ret;

  char buf[128];

  gboolean isnew = (sample == NULL);

  sw_view * v;

  f = fopen (pathname, "r");

  vf = g_malloc (sizeof (OggVorbis_File));

  if ((ret = ov_open (f, vf, NULL, 0)) < 0) {
    switch (ret) {
    case OV_EREAD:
      printf ("vorbis: read from media returned an error\n");
      break;
    case OV_ENOTVORBIS:
      /* No need to report this one -- this was not a vorbis file */
#ifdef DEBUG
      printf ("vorbis: Bitstream is not Vorbis data\n");
#endif
      break;
    case OV_EVERSION:
      printf ("vorbis: Vorbis version mismatch\n");
      break;
    case OV_EBADHEADER:
      printf ("vorbis: Invalid Vorbis bitstream header\n");
      break;
    case OV_EFAULT:
      printf ("vorbis: Internal logic fault\n");
      break;
    default:
      break;
    }

    g_free (vf);

    return NULL;
  }

  /* Get the vorbis info (channels, rate) */
  vi = ov_info (vf, -1);

  if (sample == NULL) {
    sample = sample_new_empty(pathname, vi->channels, vi->rate,
			      (sw_framecount_t) ov_pcm_total (vf, -1));
  } else {
    sounddata_destroy (sample->sounddata);
    sample->sounddata =
      sounddata_new_empty (vi->channels, vi->rate,
			   (sw_framecount_t) ov_pcm_total (vf, -1));
  }

  if(!sample) {
    g_free (vf);
    return NULL;
  }

  sample->file_method = SWEEP_FILE_METHOD_OGGVORBIS;
  sample->file_info = vf;

  sample_bank_add(sample);

  if (isnew) {
    v = view_new_all (sample, 1.0);
    sample_add_view (sample, v);
  } else {
    trim_registered_ops (sample, 0);
  }

  g_snprintf (buf, sizeof (buf), _("Loading %s"), g_basename (sample->pathname));

#ifdef DEVEL_CODE
  /* Throw the comments plus a few lines about the bitstream we're
     decoding */
  {
    GList * metadata_list = NULL;
    sw_metadata * metadata = NULL;

    char buf[1024];
    int n = 0;

    char **ptr = ov_comment(vf, -1)->user_comments;

    while(*ptr) {
      metadata = vorbis_metadata_from_str (*ptr);

      if (metadata != NULL) {
	/* Store the metadata for later use, except the encoder comment */
	if (g_strcasecmp (metadata->content, "encoder"))
	  metadata_list = g_list_append (metadata_list, metadata);

	n += snprintf (buf+n, sizeof (buf)-n, "%s: %s\n",
		       metadata->name, metadata->content);
      }

      ++ptr;
    }

    info_dialog_new (g_basename (sample->pathname), xifish_xpm,
		     _("Decoding %s\n"
		       "Encoded by: %s\n\n"
		       "%s"),
		     g_basename (sample->pathname),
		     ov_comment(vf,-1)->vendor, buf);
  }
#endif /* DEVEL_CODE */

  schedule_operation (sample, buf, &vorbis_load_op, sample);

  return sample;
}

sw_sample *
vorbis_sample_reload (sw_sample * sample)
{
  if (sample == NULL) return NULL;

  return sample_load_vorbis_info (sample, sample->pathname);
}

sw_sample *
vorbis_sample_load (char * pathname)
{
  if (pathname == NULL) return NULL;

  return sample_load_vorbis_info (NULL, pathname);
}

typedef struct {
  gchar * pathname;
  gboolean use_abr;
  gfloat quality;
  long max_bitrate;
  long nominal_bitrate;
  long min_bitrate;
  long serialno;
} vorbis_save_options;

static int
vorbis_sample_save_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  char * pathname = (char *)inst->do_data;

  FILE * outfile;
  sw_format * format;
  float * d;
  sw_framecount_t remaining, len, run_total;
  sw_framecount_t nr_frames, cframes;
  gint percent = 0;

  vorbis_save_options * so;

  ogg_stream_state os; /* take physical pages, weld into a logical
                          stream of packets */
  ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
  ogg_packet       op; /* one raw packet of data for decode */

  vorbis_info      vi; /* struct that stores all the static vorbis bitstream
                          settings */
  vorbis_comment   vc; /* struct that stores all the user comments */

  vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
  vorbis_block     vb; /* local working space for packet->PCM decode */

  int eos=0,ret;
  float **pcm;
  long i, j;

  gboolean active = TRUE;

  size_t n, bytes_written = 0;
  double average_bitrate = 0.0;

  struct stat statbuf;
  int errno_save = 0;

  if (sample == NULL) return -1;

  so = (vorbis_save_options *)sample->file_info;

  format = sample->sounddata->format;

  nr_frames = sample->sounddata->nr_frames;
  cframes = nr_frames / 100;
  if (cframes == 0) cframes = 1;

  remaining = nr_frames;
  run_total = 0;

  d = sample->sounddata->data;

  if (!(outfile = fopen (pathname, "w"))) {
    sweep_perror (errno, pathname);
    return -1;
  }

  vorbis_info_init (&vi);

  if (so->use_abr) {
    printf ("%ld, %ld, %ld\n", so->max_bitrate, so->nominal_bitrate,
	    so->min_bitrate);
    ret = vorbis_encode_init (&vi, format->channels, format->rate,
			      so->max_bitrate, so->nominal_bitrate,
			      so->min_bitrate);
  } else {
    ret = vorbis_encode_init_vbr (&vi, format->channels, format->rate,
				  so->quality /* quality: 0 to 1 */);
  }

  if (ret) {
    switch (ret) {
    case OV_EIMPL:
      sample_set_tmp_message (sample, _("Unsupported encoding mode"));
      break;
    default:
      sample_set_tmp_message (sample, _("Invalid encoding options"));
    }
    return -1;
  }

  vorbis_comment_init (&vc);
  vorbis_comment_add_tag (&vc, "ENCODER",
			  "Sweep " VERSION " (metadecks.org)");

  /* set up the analysis state and auxiliary encoding storage */
  vorbis_analysis_init (&vd, &vi);
  vorbis_block_init (&vd, &vb);

  /* set up our packet->stream encoder */
  ogg_stream_init (&os, so->serialno);

  /* Vorbis streams begin with three headers; the initial header (with
     most of the codec setup parameters) which is mandated by the Ogg
     bitstream spec.  The second header holds any comment fields.  The
     third header holds the bitstream codebook.  We merely need to
     make the headers, then pass them to libvorbis one at a time;
     libvorbis handles the additional Ogg bitstream constraints */

  {
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;

    vorbis_analysis_headerout(&vd,&vc,&header,&header_comm,&header_code);
    ogg_stream_packetin(&os,&header); /* automatically placed in its own
                                         page */
    ogg_stream_packetin(&os,&header_comm);
    ogg_stream_packetin(&os,&header_code);

        /* This ensures the actual
         * audio data will start on a new page, as per spec
         */
        while(!eos){
                int result=ogg_stream_flush(&os,&og);
                if(result==0)break;

          n = fwrite (og.header, 1, og.header_len, outfile);
	  n += fwrite (og.body, 1, og.body_len, outfile);

	  if (fflush (outfile) == 0) {
	    bytes_written += n;
	  } else {
	    errno_save = errno;
	    eos = 1; /* pffft -- this encoding wasn't going anywhere */
	  }
        }
  }

  while (!eos) {
    g_mutex_lock (sample->ops_mutex);

    if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
      active = FALSE;
    }

    if (active == FALSE || remaining <= 0) {
      /* Tell the library we're at end of stream so that it can handle
       * the last frame and mark end of stream in the output properly
       */
      vorbis_analysis_wrote (&vd, 0);
    } else {
      /* data to encode */

      len = MIN (remaining, 1024);

      /* expose the buffer to submit data */
      pcm = vorbis_analysis_buffer (&vd, 1024);

      /* uninterleave samples */
      for (i = 0; i < format->channels; i++) {
	for (j = 0; j < len; j++) {
	  pcm[i][j] = d[j*format->channels + i];
	}
      }

      /* tell the library how much we actually submitted */
      vorbis_analysis_wrote(&vd, len);

      d += (len * format->channels);

      remaining -= len;

      run_total += len;
      percent = run_total / cframes;
      sample_set_progress_percent (sample, percent);
    }

    g_mutex_unlock (sample->ops_mutex);

    /* vorbis does some data preanalysis, then divvies up blocks for
       more involved (potentially parallel) processing.  Get a single
       block for encoding now */
    while(vorbis_analysis_blockout(&vd,&vb)==1){

      /* analysis, assume we want to use bitrate management */
      vorbis_analysis(&vb,NULL);
      vorbis_bitrate_addblock(&vb);

      while(vorbis_bitrate_flushpacket(&vd,&op)){

        /* weld the packet into the bitstream */
        ogg_stream_packetin(&os,&op);

        /* write out pages (if any) */
        while(!eos){
          int result=ogg_stream_pageout(&os,&og);
          if(result==0)break;

          n = fwrite (og.header, 1, og.header_len, outfile);
	  n += fwrite (og.body, 1, og.body_len, outfile);

	  if (fflush (outfile) == 0) {
	    bytes_written += n;
	  } else {
	    errno_save = errno;
	    active = FALSE;
	  }

          /* this could be set above, but for illustrative purposes, I do
             it here (to show that vorbis does know where the stream ends) */
          if(ogg_page_eos(&og))eos=1;
        }
      }
    }
  }

  /* clean up and exit.  vorbis_info_clear() must be called last */

  ogg_stream_clear(&os);
  vorbis_block_clear(&vb);
  vorbis_dsp_clear(&vd);
  vorbis_comment_clear(&vc);
  vorbis_info_clear(&vi);

  fclose (outfile);

  /* Report success or failure; Calculate and display statistics */

  if (remaining <= 0) {
    char time_buf[16], bytes_buf[16];

    sample_store_and_free_pathname (sample, pathname);

    /* Mark the last mtime for this sample */

    stat (sample->pathname, &statbuf);
    sample->last_mtime = statbuf.st_mtime;
    sample->edit_ignore_mtime = FALSE;
    sample->modified = FALSE;

    snprint_time (time_buf, sizeof (time_buf),
		  frames_to_time (format, nr_frames - remaining));

    snprint_bytes (bytes_buf, sizeof (bytes_buf), bytes_written);

    average_bitrate =
      8.0/1000.0*((double)bytes_written/((double)nr_frames/(double)format->rate));

    info_dialog_new (_("Ogg Vorbis encoding results"), xifish_xpm,
		     "Encoding of %s succeeded.\n\n"
		     "%s written, %s audio\n"
		     "Average bitrate: %.1f kbps",
		     g_basename (sample->pathname),
		     bytes_buf, time_buf,
		     average_bitrate);
  } else {
    char time_buf[16], bytes_buf[16];

    snprint_time (time_buf, sizeof (time_buf),
		  frames_to_time (format, nr_frames - remaining));

    snprint_bytes (bytes_buf, sizeof (bytes_buf), bytes_written);

    average_bitrate =
      8.0/1000.0*((double)bytes_written/((double)(nr_frames - remaining)/(double)format->rate));
    if (isnan(average_bitrate)) average_bitrate = 0.0;

    if (errno_save == 0) {
      info_dialog_new (_("Ogg Vorbis encoding results"), xifish_xpm,
		       "Encoding of %s FAILED\n\n"
		       "%s written, %s audio (%d%% complete)\n"
		       "Average bitrate: %.1f kbps",
		       g_basename (pathname), bytes_buf, time_buf, percent,
		       average_bitrate);
    } else {
      sweep_perror (errno_save,
		    "Encoding of %s FAILED\n\n"
		    "%s written, %s audio (%d%% complete)\n"
		    "Average bitrate: %.1f kbps",
		    g_basename (pathname), bytes_buf, time_buf, percent,
		    average_bitrate);
    }
  }


  sample_set_edit_state (sample, SWEEP_EDIT_STATE_DONE);

  return 0;
}

static sw_operation vorbis_save_op = {
  SWEEP_EDIT_MODE_META,
  (SweepCallback)vorbis_sample_save_thread,
  (SweepFunction)NULL,
  (SweepCallback)NULL, /* undo */
  (SweepFunction)NULL,
  (SweepCallback)NULL, /* redo */
  (SweepFunction)NULL
};

int
vorbis_sample_save (sw_sample * sample, char * pathname)
{
  char buf[64];

  g_snprintf (buf, sizeof (buf), _("Saving %s"), g_basename (pathname));

  schedule_operation (sample, buf, &vorbis_save_op, pathname);

  return 0;
}

static void
vorbis_save_options_dialog_ok_cb (GtkWidget * widget, gpointer data)
{
  sw_sample * sample = (sw_sample *)data;
  GtkWidget * dialog;
  vorbis_save_options * so;
  GtkWidget * checkbutton;
  GtkWidget * entry;
  const gchar * text;

  gboolean use_abr;
  GtkObject * quality_adj;
  gfloat quality = -1.0;
  long max_bitrate = -1, nominal_bitrate = -1, min_bitrate = -1;
  gboolean rem_encode;
  long serialno;
  gboolean rem_serialno;

  char * pathname;

  so = g_malloc (sizeof(vorbis_save_options));

  dialog = gtk_widget_get_toplevel (widget);

  checkbutton =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "abr_chb"));
  use_abr =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton));

  if (use_abr) {
    entry = GTK_WIDGET(g_object_get_data (G_OBJECT(dialog),
					   "nominal_bitrate_entry"));
    text = gtk_entry_get_text (GTK_ENTRY(entry));
    nominal_bitrate = strtol (text, (char **)NULL, 0);
    if (nominal_bitrate == LONG_MIN || nominal_bitrate == LONG_MAX ||
	nominal_bitrate == 0)
      nominal_bitrate = DEFAULT_NOMINAL;

    entry = GTK_WIDGET(g_object_get_data (G_OBJECT(dialog),
					   "max_bitrate_entry"));
    text = gtk_entry_get_text (GTK_ENTRY(entry));
    max_bitrate = strtol (text, (char **)NULL, 0);
    if (max_bitrate == LONG_MIN || max_bitrate == LONG_MAX ||
	max_bitrate == 0)
      max_bitrate = -1;

    entry = GTK_WIDGET(g_object_get_data (G_OBJECT(dialog),
					   "min_bitrate_entry"));
    text = gtk_entry_get_text (GTK_ENTRY(entry));
    min_bitrate = strtol (text, (char **)NULL, 0);
    if (min_bitrate == LONG_MIN || min_bitrate == LONG_MAX ||
	min_bitrate == 0)
      min_bitrate = -1;

  } else {
    quality_adj =
      GTK_OBJECT(g_object_get_data (G_OBJECT(dialog), "quality_adj"));
    quality = GTK_ADJUSTMENT(quality_adj)->value;
  }

  checkbutton =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "rem_encode_chb"));
  rem_encode =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton));

  entry =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "serialno_entry"));
  text = gtk_entry_get_text (GTK_ENTRY(entry));
  serialno = strtol (text, (char **)NULL, 0);
  if (serialno == LONG_MIN || serialno == LONG_MAX) serialno = random ();

  checkbutton =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "rem_serialno_chb"));
  rem_serialno =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton));

  pathname = g_object_get_data (G_OBJECT(dialog), "pathname");

  gtk_widget_destroy (dialog);

  if (rem_encode) {
    prefs_set_int (ABR_KEY, use_abr);
    if (use_abr) {
      prefs_set_long (NOMINAL_KEY, nominal_bitrate);
      prefs_set_long (MAXIMUM_KEY, max_bitrate);
      prefs_set_long (MINIMUM_KEY, min_bitrate);
    } else {
      prefs_set_float (QUALITY_KEY, quality);
    }
  }

  if (rem_serialno) {
    prefs_set_long (SERIALNO_KEY, serialno);
  } else {
    prefs_delete (SERIALNO_KEY);
  }

  if (sample->file_info) {
    g_free (sample->file_info);
  }

  so->use_abr = use_abr;

  if (use_abr) {
    if (max_bitrate > 0) max_bitrate *= 1000;
    nominal_bitrate *= 1000;
    if (min_bitrate > 0) min_bitrate *= 1000;

    so->max_bitrate = max_bitrate;
    so->nominal_bitrate = nominal_bitrate;
    so->min_bitrate = min_bitrate;
  } else {
    g_assert (quality != -1.0);
    so->quality = quality / 10.0;
  }

  so->serialno = serialno;

  sample->file_info = so;

  vorbis_sample_save (sample, pathname);
}

static void
vorbis_save_options_dialog_cancel_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;

  dialog = gtk_widget_get_toplevel (widget);
  gtk_widget_destroy (dialog);

  /* if the sample bank is empty, quit the program */
  sample_bank_remove (NULL);
}

static void
bitrate_enable_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * bitrate_widget = (GtkWidget *)data;
  GtkWidget * quality_widget;
  gboolean active;

  quality_widget = g_object_get_data (G_OBJECT(widget), "quality_widget");

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));

  gtk_widget_set_sensitive (quality_widget, !active);
  gtk_widget_set_sensitive (bitrate_widget, active);
}

static void
vorbis_encode_options_reset_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;
  GtkWidget * checkbutton;
  GtkWidget * entry;

  gboolean use_abr;
  GtkObject * quality_adj;
  float quality;
  long l, bitrate;
  char temp [64];

  dialog = gtk_widget_get_toplevel (widget);

  /* Quality */

  quality_adj =
    GTK_OBJECT(g_object_get_data (G_OBJECT(dialog), "quality_adj"));

  quality = prefs_get_float (QUALITY_KEY, DEFAULT_QUALITY);

  gtk_adjustment_set_value (GTK_ADJUSTMENT(quality_adj), quality);

  /* Nominal bitrate */

  entry = GTK_WIDGET(g_object_get_data (G_OBJECT(dialog),
					 "nominal_bitrate_entry"));
  bitrate = prefs_get_long (NOMINAL_KEY, DEFAULT_NOMINAL);
  snprintf (temp, sizeof (temp), "%ld", bitrate);
  gtk_entry_set_text (GTK_ENTRY (entry), temp);

  /* Max bitrate */

  entry = GTK_WIDGET(g_object_get_data (G_OBJECT(dialog),
					 "max_bitrate_entry"));
  l = prefs_get_long (MAXIMUM_KEY, 0);
  if (l) {
    char temp [64];
    snprintf (temp, sizeof (temp), "%ld", l);
    gtk_entry_set_text (GTK_ENTRY (entry), temp);
  }

  /* Min bitrate */

  entry = GTK_WIDGET(g_object_get_data (G_OBJECT(dialog),
					 "min_bitrate_entry"));
  l = prefs_get_long (MINIMUM_KEY, 0);
  if (l) {
    char temp [128];
    snprintf (temp, sizeof (temp), "%ld", l);
    gtk_entry_set_text (GTK_ENTRY (entry), temp);
  }

  /* Use ABR */

  use_abr = prefs_get_int (ABR_KEY, FALSE);

  checkbutton =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "abr_chb"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkbutton), use_abr);
}

static void
vorbis_encode_options_default_cb (GtkWidget * widget, gpointer data)
{
  char temp [128];
  GtkWidget * dialog;
  GtkWidget * checkbutton;
  GtkWidget * entry;

  GtkObject * quality_adj;

  dialog = gtk_widget_get_toplevel (widget);

  /* Quality */

  quality_adj =
    GTK_OBJECT(g_object_get_data (G_OBJECT(dialog), "quality_adj"));
  gtk_adjustment_set_value (GTK_ADJUSTMENT(quality_adj), DEFAULT_QUALITY);

  /* Nominal bitrate */

  entry = GTK_WIDGET(g_object_get_data (G_OBJECT(dialog),
					 "nominal_bitrate_entry"));
  snprintf (temp, sizeof (temp), "%ld", DEFAULT_NOMINAL);
  gtk_entry_set_text (GTK_ENTRY (entry), temp);

  /* Max bitrate */

  entry = GTK_WIDGET(g_object_get_data (G_OBJECT(dialog),
					 "max_bitrate_entry"));
  gtk_entry_set_text (GTK_ENTRY (entry), "");

  /* Min bitrate */

  entry = GTK_WIDGET(g_object_get_data (G_OBJECT(dialog),
					 "min_bitrate_entry"));
  gtk_entry_set_text (GTK_ENTRY (entry), "");

  /* Use ABR */

  checkbutton =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "abr_chb"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkbutton), FALSE);
}

#ifdef DEVEL_CODE
static void
metadata_table_add_row (GtkWidget * table, int row, char * title, char * tip)
{
  GtkWidget * hbox;
  GtkWidget * label;
  GtkWidget * entry;
  GtkTooltips * tooltips;

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE(table), hbox, 0, 1, row, row+1,
		    GTK_FILL, GTK_SHRINK, 0, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new (title);
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE(table), entry, 1, 2, row, row+1,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (entry);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, entry, tip, NULL);
}
#endif

static void
remember_serialno_clicked_cb (GtkWidget * widget, gpointer data)
{
  sw_sample * sample = (sw_sample *)data;
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));

  if (active) {
    sample_set_tmp_message (sample, _("Hack the planet!"));
  } else {
    sample_clear_tmp_message (sample);
  }
}

static gboolean
randomise_serialno (gpointer data)
{
  GtkWidget * entry = (GtkWidget *)data;
  gchar new_text [128];

  snprintf (new_text, sizeof (new_text), "%ld", random ());
  gtk_entry_set_text (GTK_ENTRY (entry), new_text);

  return TRUE;
}

static void
randomise_serialno_pressed_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;
  GtkWidget * checkbutton;
  gint tag;

  dialog = gtk_widget_get_toplevel (widget);

  checkbutton =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "rem_serialno_chb"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkbutton), FALSE);

  tag = g_timeout_add (30, randomise_serialno, data);
  g_object_set_data (G_OBJECT(widget), "tag", GINT_TO_POINTER(tag));
}

static void
randomise_serialno_released_cb (GtkWidget * widget, gpointer data)
{
  gint tag;

  tag = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(widget), "tag"));
  g_source_remove (tag);
}

static GtkWidget *
create_vorbis_encoding_options_dialog (sw_sample * sample, char * pathname)
{
  GtkWidget * dialog;
  GtkWidget * ok_button, * button;
  GtkWidget * main_vbox;
  GtkWidget * ebox;
  GtkWidget * vbox;
  GtkWidget * hbox, * hbox2;
  GtkWidget * label;
  GtkWidget * pixmap;

  GtkWidget * notebook;

  GtkWidget * checkbutton;
  GtkWidget * frame;
  GtkObject * quality_adj;
  GtkWidget * quality_hscale;
  GtkWidget * table;
  GtkWidget * entry;
  GtkTooltips * tooltips;

  long l;

#ifdef DEVEL_CODE /* metadata */
  int t; /* table row */
  GtkWidget * scrolled;
#endif

  dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW(dialog),
			_("Sweep: Ogg Vorbis save options"));
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

  attach_window_close_accel(GTK_WINDOW(dialog));

  g_object_set_data (G_OBJECT(dialog), "pathname", pathname);

  main_vbox = GTK_DIALOG(dialog)->vbox;

  ebox = gtk_event_box_new ();
  gtk_box_pack_start (GTK_BOX(main_vbox), ebox, TRUE, TRUE, 0);
  gtk_widget_set_style (ebox, style_bw);
  gtk_widget_show (ebox);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(ebox), vbox);
  gtk_widget_show (vbox);

  /* Ogg Vorbis pixmaps */

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER(hbox), 4);
  gtk_widget_show (hbox);

  pixmap = create_widget_from_xpm (dialog, white_ogg_xpm);
  gtk_box_pack_start (GTK_BOX(hbox), pixmap, FALSE, FALSE, 0);
  gtk_widget_show (pixmap);

  pixmap = create_widget_from_xpm (dialog, vorbisword2_xpm);
  gtk_box_pack_start (GTK_BOX(hbox), pixmap, FALSE, FALSE, 0);
  gtk_widget_show (pixmap);

  /* filename */

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER(hbox), 4);
  gtk_widget_show (hbox);
/* pangoise?

 style = gtk_style_new ();
 gdk_font_unref (style->font);
 style->font =
 gdk_font_load("-*-helvetica-medium-r-normal-*-*-180-*-*-*-*-*-*");
 gtk_widget_push_style (style);
*/
  label = gtk_label_new (g_basename (pathname));
  gtk_box_pack_start (GTK_BOX(hbox), label, TRUE, FALSE, 0);
  gtk_widget_show (label);

/* gtk_widget_pop_style ();*/

  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX(main_vbox), notebook, TRUE, TRUE, 4);
  gtk_widget_show (notebook);

  /* Encoding quality */

  label = gtk_label_new (_("Vorbis encoding"));

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), vbox, label);
  gtk_container_set_border_width (GTK_CONTAINER(vbox), 4);
  gtk_widget_show (vbox);

  hbox = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER(hbox), 12);
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Encoding quality:"));
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 4);
  gtk_widget_show (label);

  quality_adj = gtk_adjustment_new (DEFAULT_QUALITY, /* value */
				    0.1, /* lower */
				    10.0, /* upper */
				    0.001, /* step incr */
				    0.001, /* page incr */
				    0.001  /* page size */
				    );

  {
    /* How sucky ... we create a vbox in order to center the hscale within
     * its allocation, thus actually lining it up with its label ...
     */
    GtkWidget * vbox_pants;

    vbox_pants = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX(hbox), vbox_pants, TRUE, TRUE, 0);
    gtk_widget_show (vbox_pants);

    quality_hscale = gtk_hscale_new (GTK_ADJUSTMENT(quality_adj));
    gtk_box_pack_start (GTK_BOX (vbox_pants), quality_hscale, TRUE, TRUE, 0);
    gtk_scale_set_draw_value (GTK_SCALE (quality_hscale), TRUE);
    gtk_widget_set_size_request (quality_hscale, gdk_screen_width() / 8, -1);
    gtk_widget_show (quality_hscale);

    label = gtk_label_new (NULL);
    gtk_box_pack_start (GTK_BOX(vbox_pants), label, FALSE, FALSE, 0);
    gtk_widget_show (label);
  }

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, quality_hscale,
			_("Encoding quality between 0 (lowest quality, "
			  "smallest file) and 10 (highest quality, largest "
			  "file) using variable bitrate mode (VBR)."),
			NULL);

  g_object_set_data (G_OBJECT (dialog), "quality_adj", quality_adj);

  /* Bitrate management (ABR) */

  checkbutton =
    gtk_check_button_new_with_label (_("Enable bitrate management engine"));
  gtk_box_pack_start (GTK_BOX(vbox), checkbutton, FALSE, FALSE, 4);
  gtk_widget_show (checkbutton);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, checkbutton,
			_("This enables average bitrate mode (ABR). You must "
			  "suggest a nominal average bitrate and may specify "
			  "minimum and maximum bounds.\n"
			  "For best results it is generally recommended that "
			  "you use the variable bitrate 'encoding quality' "
			  "control (above) instead."),
			NULL);

  g_object_set_data (G_OBJECT(checkbutton), "quality_widget", hbox);

  frame = gtk_frame_new (_("Bitrate management engine"));
  gtk_box_pack_start (GTK_BOX(vbox), frame, TRUE, TRUE, 4);
  gtk_container_set_border_width (GTK_CONTAINER(frame), 12);
  gtk_widget_show (frame);

  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE(table), 8);
  gtk_table_set_col_spacings (GTK_TABLE(table), 8);
  gtk_container_add (GTK_CONTAINER(frame), table);
  gtk_container_set_border_width (GTK_CONTAINER(table), 8);
  gtk_widget_show (table);

  g_signal_connect (G_OBJECT(checkbutton), "toggled",
		      G_CALLBACK(bitrate_enable_cb), frame);

  g_object_set_data (G_OBJECT (dialog), "abr_chb", checkbutton);

  gtk_widget_set_sensitive (frame, FALSE);

  /* Nominal bitrate */

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE(table), hbox, 0, 1, 0, 1,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Nominal bitrate (ABR):"));
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE(table), entry, 1, 2, 0, 1,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (entry);

  g_object_set_data (G_OBJECT (dialog), "nominal_bitrate_entry", entry);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, entry,
			_("Specify a nominal bitrate. Attempt to "
			  "encode at a bitrate averaging this."),
			NULL);

  label = gtk_label_new (_("kbps"));
  gtk_table_attach (GTK_TABLE(table), label, 2, 3, 0, 1,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (label);

  /* Minimum bitrate */

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE(table), hbox, 0, 1, 1, 2,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Minimum bitrate:"));
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE(table), entry, 1, 2, 1, 2,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (entry);

  g_object_set_data (G_OBJECT (dialog), "min_bitrate_entry", entry);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, entry,
			_("Specify a minimum bitrate, useful for "
			  "encoding for a fixed-size channel. (Optional)"),
			NULL);

  label = gtk_label_new (_("kbps"));
  gtk_table_attach (GTK_TABLE(table), label, 2, 3, 1, 2,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (label);


  /* Maximum bitrate */

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE(table), hbox, 0, 1, 2, 3,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Maximum bitrate:"));
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE(table), entry, 1, 2, 2, 3,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (entry);

  g_object_set_data (G_OBJECT (dialog), "max_bitrate_entry", entry);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, entry,
			_("Specify a maximum bitrate, useful for "
			  "streaming applications. (Optional)"),
			NULL);

  label = gtk_label_new (_("kbps"));
  gtk_table_attach (GTK_TABLE(table), label, 2, 3, 2, 3,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (label);

  /* Remember / Reset */

  hbox = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER(hbox), 12);
  gtk_widget_show (hbox);

  checkbutton =
    gtk_check_button_new_with_label (_("Remember these encoding options"));
  gtk_box_pack_start (GTK_BOX (hbox), checkbutton, TRUE, TRUE, 0);
  gtk_widget_show (checkbutton);

  g_object_set_data (G_OBJECT (dialog), "rem_encode_chb", checkbutton);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkbutton), TRUE);

  hbox2 = gtk_hbox_new (TRUE, 4);
  gtk_box_pack_start (GTK_BOX (hbox), hbox2, FALSE, TRUE, 0);
  gtk_widget_show (hbox2);

  button = gtk_button_new_with_label (_("Reset"));
  gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, TRUE, 4);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK(vorbis_encode_options_reset_cb), NULL);
  gtk_widget_show (button);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, button,
			_("Reset to the last remembered encoding options."),
			NULL);

  /* Call the reset callback now to set remembered options */
  vorbis_encode_options_reset_cb (button, NULL);

  button = gtk_button_new_with_label (_("Defaults"));
  gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, TRUE, 4);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK(vorbis_encode_options_default_cb), NULL);
  gtk_widget_show (button);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, button,
			_("Set to default encoding options."),
			NULL);

#ifdef DEVEL_CODE

  /* Metadata */

  label = gtk_label_new (_("Metadata"));

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
				  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), scrolled, label);
  gtk_widget_show (scrolled);

  t = 0;

  table = gtk_table_new (1, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE(table), 8);
  gtk_table_set_col_spacings (GTK_TABLE(table), 8);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled),
					 table);
  gtk_container_set_border_width (GTK_CONTAINER(table), 8);
  gtk_widget_show (table);

  /* These fields and descriptions derived from libvorbis API documentation:
   * /usr/share/doc/libvorbis-dev/v-comment.html
   */

  metadata_table_add_row (table, t++, _("Title:"), _("Track/Work name"));

  metadata_table_add_row (table, t++, _("Version:"),
			  _("The version field may be used to differentiate "
			    "multiple versions of the same track title in a "
			    "single collection. (e.g. remix info)"));

  metadata_table_add_row (table, t++, _("Album:"),
			  _("The collection name to which this track belongs"));

  metadata_table_add_row (table, t++, _("Artist:"),
			  _("The artist generally considered responsible for "
			    "the work. In popular music this is usually the "
			    "performing band or singer. For classical music "
			    "it would be the composer. For an audio book it "
			    "would be the author of the original text."));

  metadata_table_add_row (table, t++, _("Performer:"),
			  _("The artist(s) who performed the work. In "
			    "classical music this would be the conductor, "
			    "orchestra, soloists. In an audio book it would "
			    "be the actor who did the reading. In popular "
			    "music this is typically the same as the ARTIST "
			    "and is omitted."));

  metadata_table_add_row (table, t++, _("Copyright:"),
			  _("Copyright attribution, e.g., '2001 Nobody's "
			    "Band' or '1999 Jack Moffitt'"));

  metadata_table_add_row (table, t++, _("License:"),
			  _("License information, eg, 'All Rights Reserved', "
			    "'Any Use Permitted', a URL to a license such as "
			    "a Creative Commons license "
			    "(\"www.creativecommons.org/blahblah/license.html\") "
			    "or the EFF Open Audio License ('distributed "
			    "under the terms of the Open Audio License. see "
			    "http://www.eff.org/IP/Open_licenses/eff_oal.html "
			    "for details'), etc."));

  metadata_table_add_row (table, t++, _("Organization:"),
			  _("Name of the organization producing the track "
			    "(i.e. the 'record label')"));

  metadata_table_add_row (table, t++, _("Description:"),
			  _("A short text description of the contents"));

  metadata_table_add_row (table, t++, _("Genre:"),
			  _("A short text indication of music genre"));

  metadata_table_add_row (table, t++, _("Date:"),
			  _("Date the track was recorded"));

  metadata_table_add_row (table, t++, _("Location:"),
			  _("Location where track was recorded"));

  metadata_table_add_row (table, t++, _("Contact:"),
			  _("Contact information for the creators or "
			    "distributors of the track. This could be a URL, "
			    "an email address, the physical address of the "
			    "producing label."));

  metadata_table_add_row (table, t++, _("ISRC:"),
			  _("ISRC number for the track; see the ISRC intro "
			    "page (http://www.ifpi.org/site-content/online/isrc_intro.html) "
			    "for more information on ISRC numbers."));

#endif /* DEVEL_CODE */

  /* Ogg stream */

  label = gtk_label_new (_("Ogg stream"));

  vbox = gtk_vbox_new (FALSE, 4);
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), vbox, label);
  gtk_widget_show (vbox);

  /* Stream serial no. */

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER(hbox), 12);
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Ogg stream serial number:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 4);
  gtk_widget_show (entry);

  g_object_set_data (G_OBJECT (dialog), "serialno_entry", entry);

  /* Remember serialno ? */

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  button = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 8);
  gtk_widget_show (button);

  checkbutton =
    gtk_check_button_new_with_label (_("Remember this serial number"));
  gtk_box_pack_start (GTK_BOX (hbox), checkbutton, FALSE, TRUE, 0);
  gtk_widget_show (checkbutton);

  g_signal_connect (G_OBJECT(checkbutton), "toggled",
		      G_CALLBACK(remember_serialno_clicked_cb),
		      sample);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, checkbutton,
			_("Remember this serial number for future re-use.\n"
			  "USE OF THIS OPTION IS NOT RECOMMENDED.\n"
			  "Each encoded file should have a different "
			  "serial number; "
			  "re-use of Ogg serial numbers in different files "
			  "may create incompatabilities with streaming "
			  "applications. "
			  "This option is provided for bitstream engineering "
			  "purposes only.\n"
			  "If this option is not checked, new serial numbers "
			  "will be randomly generated for each file encoded."),
			NULL);

  g_object_set_data (G_OBJECT (dialog), "rem_serialno_chb", checkbutton);

  l = prefs_get_long (SERIALNO_KEY, 0);

  if (l == 0) {
    randomise_serialno (entry);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkbutton), FALSE);
  } else {
    char temp [128];
    snprintf (temp, sizeof (temp), "%ld", l);
    gtk_entry_set_text (GTK_ENTRY(entry), temp);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(checkbutton), TRUE);
  }

  /* Randomise serialno! */

  button = gtk_button_new_with_label (_("Randomize!"));
  gtk_container_set_border_width (GTK_CONTAINER(button), 64);
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, button,
			_("Generate a random serial number for the "
			  "Ogg bitstream. The number will change while "
			  "this button is held down."),
			NULL);

  g_signal_connect (G_OBJECT(button), "pressed",
		      G_CALLBACK(randomise_serialno_pressed_cb),
		      entry);

  g_signal_connect (G_OBJECT(button), "released",
		      G_CALLBACK(randomise_serialno_released_cb),
		      entry);

  /* About */

  label = gtk_label_new (_("About"));

  ebox = gtk_event_box_new ();
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), ebox, label);
  gtk_widget_set_style (ebox, style_bw);
  gtk_widget_show (ebox);

  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK(notebook), ebox,
				      TRUE, TRUE, GTK_PACK_END);

  vbox = gtk_vbox_new (FALSE, 16);
  gtk_container_add (GTK_CONTAINER(ebox), vbox);
  gtk_container_set_border_width (GTK_CONTAINER(vbox), 8);
  gtk_widget_show (vbox);

  label =
    gtk_label_new (_("Ogg Vorbis is a high quality general purpose\n"
		     "perceptual audio codec. It is free, open and\n"
		     "unpatented."));
  gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  hbox = gtk_hbox_new (FALSE, 16);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  label =
    gtk_label_new (_("Ogg, Vorbis, Xiph.org Foundation and their logos\n"
		     "are trademarks (tm) of the Xiph.org Foundation.\n"
		     "Used with permission."));
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 8);
  gtk_widget_show (label);

  pixmap = create_widget_from_xpm (dialog, xifish_xpm);
  gtk_box_pack_start (GTK_BOX(hbox), pixmap, FALSE, FALSE, 8);
  gtk_widget_show (pixmap);


  button = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, FALSE, 8);
  gtk_widget_show (button);

  label = gtk_label_new (_("This user interface by Conrad Parker,\n"
			   "Copyright (C) 2002 CSIRO Australia.\n\n"));
  gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /* OK */

  ok_button = gtk_button_new_with_label (_("Save"));
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (ok_button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), ok_button,
		      TRUE, TRUE, 0);
  gtk_widget_show (ok_button);
  g_signal_connect (G_OBJECT(ok_button), "clicked",
		      G_CALLBACK (vorbis_save_options_dialog_ok_cb),
		      sample);

  /* Cancel */

  button = gtk_button_new_with_label (_("Don't save"));
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), button,
		      TRUE, TRUE, 0);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK (vorbis_save_options_dialog_cancel_cb),
		      sample);

  gtk_widget_grab_default (ok_button);

  return (dialog);
}

int
vorbis_save_options_dialog (sw_sample * sample, char * pathname)
{
  GtkWidget * dialog;

  dialog = create_vorbis_encoding_options_dialog (sample, pathname);
  gtk_widget_show (dialog);

  return 0;
}

#endif /* HAVE_OGGVORBIS */
