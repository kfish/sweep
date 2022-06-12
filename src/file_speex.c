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
 * This file adapted from "speexdec.c" and "speexenc.c" in the Speex coded
 * source code, Copyright (C) 2002 Jean-Marc Valin
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright
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
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_SPEEX

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>

#include <ogg/ogg.h>
#ifdef HAVE_SPEEX_SUBDIR
#include <speex/speex.h>
#include <speex/speex_header.h>
#include <speex/speex_stereo.h>
#include <speex/speex_callbacks.h>
#else
#include <speex.h>
#include <speex_header.h>
#include <speex_stereo.h>
#include <speex_callbacks.h>
#endif

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

#include "../pixmaps/xifish.xpm"
#include "../pixmaps/speex_logo.xpm"

/* MAINTENANCE:
 *
 * Upon release of Speex 1.0, force a requirement on that version in
 * configure. Then, remove all references to SPEEX_HAVE_BETA4 (assume
 * this is true, as those features will be available), and also assume
 * that SPEEX_NB_MODES > 2. This should reduce the random ifdef'ing
 * present to accommodate the flux of prerelease versions of Speex.
 */

#ifdef SPEEX_SET_DTX
#define HAVE_SPEEX_BETA4
#endif

#define MODE_KEY "Speex_Mode"
#define FEATURES_KEY "Speex_Features"
#define QUALITY_KEY "Speex_Quality"
#define BR_KEY "Speex_BR"
#define BITRATE_KEY "Speex_Bitrate"
#define COMPLEXITY_KEY "Speex_Complexity"
#define SERIALNO_KEY "OggSpeex_Serialno"
#define FRAMEPACK_KEY "OggSpeex_FramePack"

/* Mode choices */
#define MODE_NARROWBAND 0
#define MODE_WIDEBAND 1
#define MODE_ULTRAWIDEBAND 2

/* Feature flags */
#define FEAT_VBR 1
#define FEAT_VAD 2
#define FEAT_DTX 4

#ifdef HAVE_SPEEX_BETA4
#define DEFAULT_FEATURES (FEAT_VBR | FEAT_VAD | FEAT_DTX)
#else
#define DEFAULT_FEATURES (FEAT_VBR)
#endif

#define DEFAULT_QUALITY 8.0
#define DEFAULT_COMPLEXITY 3.0
#define DEFAULT_FRAMEPACK 1

#define DEFAULT_ENH_ENABLED 1

extern GtkStyle * style_bw;

#define READ_SIZE 200

/*
 * file_is_ogg_speex (pathname)
 *
 * This function attempts to determine if a given file is an ogg speex file
 * by attempting to parse enough of the stream to decode an initial speex
 * header. If any steps along the way fail, it returns false;
 */
static gboolean
file_is_ogg_speex (const char * pathname)
{
  int fd;
  ssize_t nread;

  ogg_sync_state oy;
  ogg_page og;
  ogg_packet op;
  ogg_stream_state os;

  char * ogg_data;

  const SpeexMode *mode;
  SpeexHeader *header;

  fd = open (pathname, O_RDONLY);
  if (fd == -1) {
    return FALSE;
  }

  ogg_sync_init (&oy);
  ogg_data = ogg_sync_buffer (&oy, READ_SIZE);
  if (ogg_data == NULL) goto out_false_sync;
  if ((nread = read (fd, ogg_data, READ_SIZE)) <= 0) goto out_false_sync;
  ogg_sync_wrote (&oy, nread);
  if (ogg_sync_pageout (&oy, &og) != 1) goto out_false_sync;
  ogg_stream_init (&os, ogg_page_serialno (&og));
  ogg_stream_pagein (&os, &og);
  if (ogg_stream_packetout (&os, &op) != 1) goto out_false_stream;
  header = speex_packet_to_header ((char*) op.packet, op.bytes);
  if (!header) goto out_false_stream;
  if (header->mode >= SPEEX_NB_MODES) goto out_false_stream;
  mode = speex_mode_list[header->mode];
  if (mode->bitstream_version != header->mode_bitstream_version)
    goto out_false_stream;

  ogg_sync_clear (&oy);
  ogg_stream_clear (&os);

  close (fd);

  return TRUE;

 out_false_stream:
  ogg_stream_clear (&os);

 out_false_sync:
  ogg_sync_clear (&oy);

  close (fd);

  return FALSE;
}

static void *
process_header(ogg_packet *op, int enh_enabled, int * frame_size, int * rate,
	       int * nframes, int forceMode, int * channels,
	       SpeexStereoState * stereo, int * extra_headers)
{
  void *st;
  SpeexMode *mode;
  SpeexHeader *header;
  int modeID;
  SpeexCallback callback;

  header = speex_packet_to_header((char*)op->packet, op->bytes);
  if (!header) {
    info_dialog_new ("Speex error", NULL, "Speex: cannot read header");
    return NULL;
  }
  if (header->mode >= SPEEX_NB_MODES || header->mode < 0) {
    info_dialog_new ("Speex error", NULL,
		     "Mode number %d does not (any longer) exist in this version\n",
		     header->mode);
    return NULL;
  }

  modeID = header->mode;
  if (forceMode!=-1)
    modeID = forceMode;
  mode = (SpeexMode *)speex_mode_list[modeID];

#ifdef HAVE_SPEEX_BETA4
  if (header->speex_version_id > 1) {
    info_dialog_new ("Speex error", NULL,
		     "This file was encoded with Speex bit-stream version %d, "
		     "which I don't know how to decode\n",
		     header->speex_version_id);
    return NULL;
  }
#endif

  if (mode->bitstream_version < header->mode_bitstream_version) {
    info_dialog_new ("Speex error", NULL,
		     "The file was encoded with a newer version of Speex. "
		     "You need to upgrade in order to play it.\n");
    return NULL;
  }

  if (mode->bitstream_version > header->mode_bitstream_version) {
    info_dialog_new ("Speex error", NULL,
		     "The file was encoded with an older version of Speex. "
		     "You would need to downgrade the version in order to play it.\n");
    return NULL;
  }

  st = speex_decoder_init(mode);
  if (!st) {
    info_dialog_new ("Speex error", NULL,
		     "Decoder initialization failed.\n");
    return NULL;
  }

  speex_decoder_ctl(st, SPEEX_SET_ENH, &enh_enabled);
  speex_decoder_ctl(st, SPEEX_GET_FRAME_SIZE, frame_size);

  if (!(*channels==1))
    {
      callback.callback_id = SPEEX_INBAND_STEREO;
      callback.func = speex_std_stereo_request_handler;
      callback.data = stereo;
      speex_decoder_ctl(st, SPEEX_SET_HANDLER, &callback);
    }
  if (*rate==-1)
    *rate = header->rate;
  /* Adjust rate if --force-* options are used */
  if (forceMode!=-1)
    {
      if (header->mode < forceMode)
	*rate <<= (forceMode - header->mode);
      if (header->mode > forceMode)
	*rate >>= (header->mode - forceMode);
    }

  speex_decoder_ctl(st, SPEEX_SET_SAMPLING_RATE, rate);

  *nframes = header->frames_per_packet;

  if (*channels == -1)
    *channels = header->nb_channels;

#ifdef DEBUG
  fprintf (stderr, "Decoding %d Hz audio using %s mode",
	   *rate, mode->modeName);

  if (*channels==1)
      fprintf (stderr, " (mono");
   else
      fprintf (stderr, " (stereo");

  if (header->vbr)
    fprintf (stderr, " (VBR)\n");
  else
    fprintf(stderr, "\n");
#endif

#ifdef HAVE_SPEEX_BETA4
  *extra_headers = header->extra_headers;
#else
  *extra_headers = 0;
#endif

  free(header);

  return st;
}

static sw_sample *
sample_load_speex_data (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;

  int fd;
  struct stat statbuf;

  void * st = NULL;
  SpeexBits bits;
  int frame_size = 0;
  int rate = -1;
  int channels = -1;
  int extra_headers = 0;
  SpeexStereoState stereo = SPEEX_STEREO_STATE_INIT;
  int packet_count = 0;
  int stream_init = 0;

  ogg_sync_state oy;
  ogg_page og;
  ogg_packet op;
  ogg_stream_state os;

  char * ogg_data;

  int enh_enabled = DEFAULT_ENH_ENABLED;
  int nframes = 2;
  int eos = 0;
  int forceMode = -1;

  int i, j;
  float * d = NULL;
  sw_framecount_t frames_total = 0, frames_decoded = 0;
  size_t file_length, remaining, n;
  ssize_t nread;
  gint percent;

  gboolean active = TRUE;

  fd = open (sample->pathname, O_RDONLY);
  if (fd == -1) {
    sweep_perror (errno, "failed open in sample_load_speex_data");
    return NULL;
  }

  if (fstat (fd, &statbuf) == -1) {
    sweep_perror (errno, "failed stat in sample_load_speex_data");
    return NULL;
  }

  file_length = remaining = statbuf.st_size;

  /* Init Ogg sync */
  ogg_sync_init (&oy);

  speex_bits_init (&bits);

  while (active && remaining > 0) {
    g_mutex_lock (&sample->ops_mutex);

    if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
      active = FALSE;
    } else {
      n = MIN (remaining, READ_SIZE);

      ogg_data = ogg_sync_buffer (&oy, n);
      nread = read (fd, ogg_data, n);
      if (nread == -1) {
	sweep_perror (errno, "speex: %s", sample->pathname);
	active = FALSE;
      } else if (nread == 0) {
	/* eof */
	active = FALSE;
      } else {
	ogg_sync_wrote (&oy, nread);
	n = (size_t)nread;
      }

      /* Loop for all complete pages we got */
      while (active && ogg_sync_pageout (&oy, &og) == 1) {
	if (stream_init == 0) {
	  ogg_stream_init (&os, ogg_page_serialno (&og));
	  stream_init = 1;
	}

	/* Add page to the bitstream */
	ogg_stream_pagein (&os, &og);

	/* Extract all available packets */
	while (!eos && ogg_stream_packetout (&os, &op) == 1) {
	  if (packet_count == 0) {/* header */
	    st = process_header (&op, enh_enabled, &frame_size, &rate,
				 &nframes, forceMode, &channels, &stereo,
				 &extra_headers);
	    if (st == NULL) {
	      /*printf ("Not Speex!\n");*/
	      active = FALSE;
	    }

	    sample->sounddata->format->rate = rate;
	    sample->sounddata->format->channels = channels;

	    if (nframes == 0)
	      nframes = 1;

	  } else if (packet_count <= 1+extra_headers) {
	    /* XXX: metadata, extra_headers: ignore */
	  } else {
	    if (op.e_o_s)
	      eos = 1;

	    /* Copy Ogg packet to Speex bitstream */
	    speex_bits_read_from (&bits, (char *)op.packet, op.bytes);

	    frames_total += nframes * frame_size;

	    if (sample->sounddata->nr_frames != frames_total) {
	      sample->sounddata->data =
		g_realloc (sample->sounddata->data,
			   frames_total * channels * sizeof (float));
	    }

	    sample->sounddata->nr_frames = frames_total;

	    d = &((float *)sample->sounddata->data)
		  [frames_decoded * channels];

	    if (d != NULL) {
	      for (j = 0; j < nframes; j++) {
		/* Decode frame */
		speex_decode (st, &bits, d);
#ifdef DEBUG
		if (speex_bits_remaining (&bits) < 0) {
		  info_dialog_new ("Speex warning", NULL,
				   "Speex: decoding overflow -- corrupted stream at frame %ld", frames_decoded + (j * frame_size));
		}
#endif
		if (channels == 2)
		  speex_decode_stereo (d, frame_size, &stereo);

		for (i = 0; i < frame_size * channels; i++) {
		  d[i] /= 32767.0;
		}
		d += (frame_size * channels);
		frames_decoded += frame_size;
	      }
	    }
	  }

	  packet_count ++;
	}

	remaining -= n;

	percent = (file_length - remaining) * 100 / file_length;
	sample_set_progress_percent (sample, percent);
      }
    }

    g_mutex_unlock (&sample->ops_mutex);
  }

  if (st) speex_decoder_destroy (st);
  speex_bits_destroy (&bits);
  ogg_sync_clear (&oy);
  ogg_stream_clear (&os);

  close (fd);

  if (remaining <= 0) {
    stat (sample->pathname, &statbuf);
    sample->last_mtime = statbuf.st_mtime;
    sample->edit_ignore_mtime = FALSE;
    sample->modified = FALSE;
  }

  sample_set_edit_state (sample, SWEEP_EDIT_STATE_DONE);

  return sample;
}

static sw_operation speex_load_op = {
  SWEEP_EDIT_MODE_FILTER,
  (SweepCallback)sample_load_speex_data,
  (SweepFunction)NULL,
  (SweepCallback)NULL, /* undo */
  (SweepFunction)NULL,
  (SweepCallback)NULL, /* redo */
  (SweepFunction)NULL
};

static sw_sample *
sample_load_speex_info (sw_sample * sample, char * pathname)
{
  char buf[128];

  gboolean isnew = (sample == NULL);

  sw_view * v;

  if (!file_is_ogg_speex (pathname)) return NULL;

  /* Create the sample/sounddata, initially with length 0, to be grown
   * as the file is decoded
   */
  if (sample == NULL) {
    /* Channels and rate will be set during decoding and are basically
     * irrelevant here. Set them to 1, 8000 assuming these are the most
     * likely values, in which case the file info displayed in the window
     * will not change suddenly
     */
    sample = sample_new_empty(pathname, 1, 8000, 0);
  } else {
    int channels, rate;

    /* Set the channels and rate of the recreated sounddata to be the same
     * as the old one, as they are most likely the same after a reload */
    channels = sample->sounddata->format->channels;
    rate = sample->sounddata->format->rate;

    sounddata_destroy (sample->sounddata);
    sample->sounddata = sounddata_new_empty (channels, rate, 0);
  }

  if(!sample) {
    return NULL;
  }

  sample->file_method = SWEEP_FILE_METHOD_SPEEX;
  sample->file_info = NULL;

  sample_bank_add(sample);

  if (isnew) {
    v = view_new_all (sample, 1.0);
    sample_add_view (sample, v);
  } else {
    trim_registered_ops (sample, 0);
  }

  g_snprintf (buf, sizeof (buf), _("Loading %s"), g_path_get_basename (sample->pathname));

  schedule_operation (sample, buf, &speex_load_op, sample);

  return sample;
}

sw_sample *
speex_sample_reload (sw_sample * sample)
{
  if (sample == NULL) return NULL;

  return sample_load_speex_info (sample, sample->pathname);
}

sw_sample *
speex_sample_load (char * pathname)
{
  if (pathname == NULL) return NULL;

  return sample_load_speex_info (NULL, pathname);
}


/*
 * comment creation: from speexenc.c
 */

/*
 Comments will be stored in the Vorbis style.
 It is describled in the "Structure" section of
    http://www.xiph.org/ogg/vorbis/doc/v-comment.html

The comment header is decoded as follows:
  1) [vendor_length] = read an unsigned integer of 32 bits
  2) [vendor_string] = read a UTF-8 vector as [vendor_length] octets
  3) [user_comment_list_length] = read an unsigned integer of 32 bits
  4) iterate [user_comment_list_length] times {
     5) [length] = read an unsigned integer of 32 bits
     6) this iteration's user comment = read a UTF-8 vector as [length] octets
     }
  7) [framing_bit] = read a single bit as boolean
  8) if ( [framing_bit]  unset or end of packet ) then ERROR
  9) done.

  If you have troubles, please write to ymnk@jcraft.com.
 */

#define readint(buf, base) (((buf[base+3]<<24)&0xff000000)| \
                           ((buf[base+2]<<16)&0xff0000)| \
                           ((buf[base+1]<<8)&0xff00)| \
                            (buf[base]&0xff))
#define writeint(buf, base, val) do{ buf[base+3]=(val>>24)&0xff; \
                                     buf[base+2]=(val>>16)&0xff; \
                                     buf[base+1]=(val>>8)&0xff; \
                                     buf[base]=(val)&0xff; \
                                 }while(0)

void comment_init(char **comments, int* length, char *vendor_string)
{
  int vendor_length=strlen(vendor_string);
  int user_comment_list_length=0;
  int len=4+vendor_length+4;
  char *p=(char*)malloc(len);
  if(p==NULL){
  }
  writeint(p, 0, vendor_length);
  memcpy(p+4, vendor_string, vendor_length);
  writeint(p, 4+vendor_length, user_comment_list_length);
  *length=len;
  *comments=p;
}
void comment_add(char **comments, int* length, char *tag, char *val)
{
  char* p=*comments;
  int vendor_length=readint(p, 0);
  int user_comment_list_length=readint(p, 4+vendor_length);
  int tag_len=(tag?strlen(tag):0);
  int val_len=strlen(val);
  int len=(*length)+4+tag_len+val_len;

  p=realloc(p, len);
  if(p==NULL){
  }

  writeint(p, *length, (tag_len+val_len));      /* length of comment */
  if(tag) memcpy(p+*length+4, tag, tag_len);  /* comment */
  memcpy(p+*length+4+tag_len, val, val_len);  /* comment */
  writeint(p, 4+vendor_length, (user_comment_list_length+1));

  *comments=p;
  *length=len;
}
#undef readint
#undef writeint


typedef struct {
  gchar * pathname;
  gint mode;
  gint features;
  gboolean use_br;
  gfloat quality; /* either way */
  gint bitrate;
  gint complexity;
  gint framepack;
  long serialno;
} speex_save_options;

#define MAX_FRAME_SIZE 2000
#define MAX_FRAME_BYTES 2000

static int
speex_sample_save_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  gchar * pathname = (gchar *)inst->do_data;

  FILE * outfile;
  sw_format * format;
  float * d;
  sw_framecount_t remaining, len, run_total;
  sw_framecount_t nr_frames, cframes;
  gint percent = 0;

  speex_save_options * so;

  ogg_stream_state os; /* take physical pages, weld into a logical
                          stream of packets */
  ogg_page         og; /* one Ogg bitstream page. Speex packets are inside */
  ogg_packet       op; /* one raw packet of data for decode */

  float input[MAX_FRAME_SIZE];
  gchar cbits[MAX_FRAME_BYTES];
  int nbBytes;
  int id = 0;

  int frame_size;
  SpeexMode * mode = NULL;
  SpeexHeader header;
  void * st;
  SpeexBits bits;

  gchar * vendor_string = "Encoded with Sweep " VERSION " (metadecks.org)";
  gchar * comments = NULL;
  int comments_length = 0;

  int eos = 0;
  int i, j;

  gboolean active = TRUE;

  size_t n, bytes_written = 0;
  double average_bitrate = 0.0;

  struct stat statbuf;
  int errno_save = 0;

  if (sample == NULL) return -1;

  so = (speex_save_options *)sample->file_info;

  format = sample->sounddata->format;

  nr_frames = sample->sounddata->nr_frames;
  cframes = nr_frames / 100;
  if (cframes == 0) cframes = 1;

  remaining = nr_frames;
  run_total = 0;

  if (!(outfile = fopen (pathname, "w"))) {
    sweep_perror (errno, pathname);
    return -1;
  }

  switch (so->mode) {
  case MODE_NARROWBAND:
    mode = (SpeexMode *) &speex_nb_mode;
    break;
  case MODE_WIDEBAND:
    mode = (SpeexMode *) &speex_wb_mode;
    break;
#if (SPEEX_NB_MODES > 2)
  case MODE_ULTRAWIDEBAND:
    mode = (SpeexMode *) &speex_uwb_mode;
    break;
#endif
  default:
    mode = (SpeexMode *) &speex_nb_mode;
    break;
  }

  speex_init_header (&header, format->rate, 1 , mode);
  header.frames_per_packet = so->framepack;
  header.vbr = (so->features & FEAT_VBR) ? 1 : 0;
  header.nb_channels = format->channels;

#ifdef DEBUG
  fprintf (stderr, "Encoding %d Hz audio using %s mode\n",
	   header.rate, mode->modeName);
#endif

  /* initialise Speex encoder */
  st = speex_encoder_init (mode);

  /* initialise comments */
  comment_init (&comments, &comments_length, vendor_string);

  /* set up our packet->stream encoder */
  ogg_stream_init (&os, so->serialno);

  /* write header */

  {
    int bytes = op.bytes;
    op.packet = (unsigned char *)
      speex_header_to_packet (&header, &bytes);
    op.bytes = bytes;
    op.b_o_s = 1;
    op.e_o_s = 0;
    op.granulepos = 0;
    op.packetno = 0;
    ogg_stream_packetin(&os, &op);
    free(op.packet);

    op.packet = (unsigned char *)comments;
    op.bytes = comments_length;
    op.b_o_s = 0;
    op.e_o_s = 0;
    op.granulepos = 0;
    op.packetno = 1;
    ogg_stream_packetin(&os, &op);

    /* This ensures the actual
     * audio data will start on a new page, as per spec
     */
    while(!eos){
      int result = ogg_stream_flush (&os, &og);
      if (result == 0) break;

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

  if (comments) g_free (comments);

  speex_encoder_ctl (st, SPEEX_SET_SAMPLING_RATE, &format->rate);

  speex_encoder_ctl (st, SPEEX_GET_FRAME_SIZE, &frame_size);
  speex_encoder_ctl (st, SPEEX_SET_COMPLEXITY, &so->complexity);
  if (so->features & FEAT_VBR) {
    int tmp = 1;
    speex_encoder_ctl (st, SPEEX_SET_VBR, &tmp);
    speex_encoder_ctl (st, SPEEX_SET_VBR_QUALITY, &so->quality);
#ifdef HAVE_SPEEX_BETA4
    if (so->use_br) {
      speex_encoder_ctl (st, SPEEX_SET_ABR, &so->bitrate);
    }
#endif
  } else {
    int tmp = (int)floor(so->quality);
    speex_encoder_ctl (st, SPEEX_SET_QUALITY, &tmp);
    if (so->use_br) {
      speex_encoder_ctl (st, SPEEX_SET_BITRATE, &so->bitrate);
    }
  }

#ifdef HAVE_SPEEX_BETA4
  if (so->features & FEAT_VAD) {
    int tmp = 1;
    speex_encoder_ctl (st, SPEEX_SET_VAD, &tmp);
    if (so->features & FEAT_DTX) {
      speex_encoder_ctl (st, SPEEX_SET_DTX, &tmp);
    }
  }
#endif

  speex_bits_init (&bits);

  while (!eos) {
    g_mutex_lock (&sample->ops_mutex);

    if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
      active = FALSE;
    }

    if (active == FALSE || remaining <= 0) {
      /* Mark the end of stream */
      /* XXX: this will be set when this packet is paged out: eos = 1; */
      op.e_o_s = 1;
    } else {
      op.e_o_s = 0;

      /* data to encode */

      for (i = 0; i < so->framepack; i++) {
	if (remaining > 0) {
	  len = MIN (remaining, frame_size);

	  d = &((float *)sample->sounddata->data)
	    [run_total * format->channels];

	  memcpy (input, d, sizeof (float) * len * format->channels);

	  /* rip channel 0 out, in required format */
	  for (j = 0; j < len * format->channels; j++) {
	    input[j] *= 32767.0;
	  }

	  if (format->channels == 2)
	    speex_encode_stereo (input, len, &bits);
	  speex_encode (st, input, &bits);

	  remaining -= len;

	  run_total += len;
	  percent = run_total / cframes;
	  sample_set_progress_percent (sample, percent);
	} else {
	  /*speex_bits_pack (&bits, 0, 7);*/
	  speex_bits_pack (&bits, 15, 5);
	}

	id++;
      }
    }

    g_mutex_unlock (&sample->ops_mutex);

    nbBytes = speex_bits_write (&bits, cbits, MAX_FRAME_BYTES);
    speex_bits_reset (&bits);

    /* Put it in an ogg packet */
    op.packet = (unsigned char *)cbits;
    op.bytes = nbBytes;
    op.b_o_s = 0;
    /* op.e_o_s was set above */
    op.granulepos = id * frame_size;
    op.packetno = 2 + (id-1)/so->framepack;

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
	 it here (to show that we know where the stream ends) */
      if (ogg_page_eos(&og)) eos=1;
    }
  }

  /* clean up and exit.  speex_info_clear() must be called last */

  speex_encoder_destroy (st);
  speex_bits_destroy (&bits);
  ogg_stream_clear(&os);

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

    info_dialog_new (_("Speex encoding results"), xifish_xpm,
		     "Encoding of %s succeeded.\n\n"
		     "%s written, %s audio\n"
		     "Average bitrate: %.1f kbps",
		     g_path_get_basename (sample->pathname),
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
      info_dialog_new (_("Speex encoding results"), xifish_xpm,
		       "Encoding of %s FAILED\n\n"
		       "%s written, %s audio (%d%% complete)\n"
		       "Average bitrate: %.1f kbps",
		       g_path_get_basename (pathname), bytes_buf, time_buf, percent,
		       average_bitrate);
    } else {
      sweep_perror (errno_save,
		    "Encoding of %s FAILED\n\n"
		    "%s written, %s audio (%d%% complete)\n"
		    "Average bitrate: %.1f kbps",
		    g_path_get_basename (pathname), bytes_buf, time_buf, percent,
		    average_bitrate);
    }
  }

  sample_set_edit_state (sample, SWEEP_EDIT_STATE_DONE);

  return 0;
}

static sw_operation speex_save_op = {
  SWEEP_EDIT_MODE_META,
  (SweepCallback)speex_sample_save_thread,
  (SweepFunction)NULL,
  (SweepCallback)NULL, /* undo */
  (SweepFunction)NULL,
  (SweepCallback)NULL, /* redo */
  (SweepFunction)NULL
};

int
speex_sample_save (sw_sample * sample, char * pathname)
{
  char buf[64];

  g_snprintf (buf, sizeof (buf), _("Saving %s"), g_path_get_basename (pathname));

  schedule_operation (sample, buf, &speex_save_op, pathname);

  return 0;
}

static void
speex_save_options_dialog_ok_cb (GtkWidget * widget, gpointer data)
{
  sw_sample * sample = (sw_sample *)data;
  GtkWidget * dialog;
  speex_save_options * so;
  GtkWidget * checkbutton;
  GtkWidget * entry;
  const gchar * text;

  gboolean use_br;
  GtkObject * adj;
  int mode, features, quality, bitrate, complexity, framepack;
  gboolean rem_encode;
  long serialno;
  gboolean rem_serialno;

  char * pathname;

  so = g_malloc (sizeof(speex_save_options));

  dialog = gtk_widget_get_toplevel (widget);

  /* Mode */

  mode =
    GPOINTER_TO_INT(g_object_get_data (G_OBJECT(dialog), "mode_choice"));

  /* Features */

  features =
    GPOINTER_TO_INT(g_object_get_data (G_OBJECT(dialog),
					 "features_choice"));

  adj = GTK_OBJECT(g_object_get_data (G_OBJECT(dialog), "quality_adj"));
  quality = (int)GTK_ADJUSTMENT(adj)->value;

  adj = GTK_OBJECT(g_object_get_data (G_OBJECT(dialog), "complexity_adj"));
  complexity = (int)GTK_ADJUSTMENT(adj)->value;

  adj = GTK_OBJECT(g_object_get_data (G_OBJECT(dialog), "framepack_adj"));
  framepack = (int)GTK_ADJUSTMENT(adj)->value;

  checkbutton =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "br_chb"));
  use_br = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton));

  entry =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "br_entry"));
  text = gtk_entry_get_text (GTK_ENTRY(entry));
  bitrate = (int)strtol (text, (char **)NULL, 0);

  /* rem encode */
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
    prefs_set_int (MODE_KEY, mode);
    prefs_set_int (FEATURES_KEY, features);
    prefs_set_int (BR_KEY, use_br);
    prefs_set_int (QUALITY_KEY, quality);
    prefs_set_int (BITRATE_KEY, bitrate);
    prefs_set_int (COMPLEXITY_KEY, complexity);
    prefs_set_int (FRAMEPACK_KEY, framepack);
  }

  if (rem_serialno) {
    prefs_set_long (SERIALNO_KEY, serialno);
  } else {
    prefs_delete (SERIALNO_KEY);
  }

  if (sample->file_info) {
    g_free (sample->file_info);
  }

  so->mode = mode;
  so->features = features;
  so->use_br = use_br;
  so->quality = quality;
  so->bitrate = bitrate;
  so->complexity = complexity;
  so->framepack = framepack;

  so->serialno = serialno;

  sample->file_info = so;

  speex_sample_save (sample, pathname);
}

static void
speex_save_options_dialog_cancel_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;

  dialog = gtk_widget_get_toplevel (widget);
  gtk_widget_destroy (dialog);

  /* if the sample bank is empty, quit the program */
  sample_bank_remove (NULL);
}

typedef struct {
  int number;
  char * name;
  char * desc;
} sw_choice;

static sw_choice mode_choices[] = {
  { MODE_NARROWBAND, N_("Narrowband ~8 kHz (telephone quality)"), NULL },
  { MODE_WIDEBAND, N_("Wideband ~16 kHz"), NULL },
#if SPEEX_NB_MODES > 2
  { MODE_ULTRAWIDEBAND, N_("Ultra-wideband 32-48 kHz"), NULL },
#endif
  { 0, NULL, NULL }
};

static sw_choice feature_choices[] = {
#ifdef HAVE_SPEEX_BETA4
  { 0, N_("Constant bitrate (CBR) with no features"),
    NULL
  },
  { FEAT_VAD, N_("CBR with Voice Activity Detection (VAD)"),
    N_("VAD generates low bitrate comfort noise to replace non-speech")
  },
  { FEAT_VAD | FEAT_DTX,
    N_("CBR with VAD and Discontinuous Transmission (DTX)"),
    N_("DTX marks extended pauses with a minimum bitrate signal")
  },
  { FEAT_VBR | FEAT_VAD,
    N_("Variable bitrate (VBR) with VAD"),
    N_("VBR allows the bitrate to adapt to the complexity of the speech; "
       "this selection uses VBR without DTX, which may improve performance "
       "compared to full VBR in the presence of background noise.")
  },
  { FEAT_VBR | FEAT_VAD | FEAT_DTX,
    N_("Variable bitrate (VBR) with all features"),
    N_("VBR allows the bitrate to adapt to the complexity of the speech, "
       "and handles pauses using VAD and DTX")
  },
#else
  { 0, N_("Constant bitrate (CBR)"), NULL },
  { FEAT_VBR,
    N_("Variable bitrate (VBR)"),
    N_("VBR allows the bitrate to adapt to the complexity of the speech.")
  },
#endif
  { 0, NULL, NULL }
};

static void
speex_encode_options_update_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;
  GtkWidget * br_checkbutton;
  GtkWidget * quality_label;
  GtkWidget * quality_hscale;
  GtkWidget * br_label;
  GtkWidget * br_entry;
  GtkWidget * br_units;

  gboolean br;
  gint features;

  dialog = gtk_widget_get_toplevel (widget);

  /* Features */
  features =
    GPOINTER_TO_INT (g_object_get_data (G_OBJECT(dialog),
					  "features_choice"));

  /* Quality */

  quality_label =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "quality_label"));
  quality_hscale =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "quality_hscale"));

  /* Bitrate */

  br_checkbutton =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "br_chb"));
  br = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(br_checkbutton));

  br_label =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "br_label"));
  br_entry =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "br_entry"));
  br_units =
    GTK_WIDGET(g_object_get_data (G_OBJECT(dialog), "br_units"));

  gtk_widget_set_sensitive (br_label, br);
  gtk_widget_set_sensitive (br_entry, br);
  gtk_widget_set_sensitive (br_units, br);
  gtk_widget_set_sensitive (quality_label, !br);
  gtk_widget_set_sensitive (quality_hscale, !br);

  if (features & FEAT_VBR) {
    gtk_scale_set_digits (GTK_SCALE (quality_hscale), 1);
    gtk_label_set_text (GTK_LABEL(br_label), _("Average bitrate"));
  } else {
    gtk_scale_set_digits (GTK_SCALE (quality_hscale), 0);
    gtk_label_set_text (GTK_LABEL(br_label), _("Maximum bitrate"));
  }

}

static void
speex_encode_options_mode_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog = GTK_WIDGET (data);
  int mode;

  mode = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(widget), "default"));

  g_object_set_data (G_OBJECT(dialog), "mode_choice",
		       GINT_TO_POINTER(mode));
}

static void
speex_encode_options_mode_auto_cb (GtkWidget * widget, gpointer data)
{
  sw_sample * sample = (sw_sample *)data;
  sw_format * f = sample->sounddata->format;
  GtkWidget * dialog;
  GtkOptionMenu * option_menu;
  int mode;

  dialog = gtk_widget_get_toplevel (widget);
  option_menu =
    GTK_OPTION_MENU(g_object_get_data (G_OBJECT(dialog), "mode_menu"));

#if (SPEEX_NB_MODES > 2)
  if (f->rate >= 32000) {
    mode = MODE_ULTRAWIDEBAND;
  } else
#endif
    if (f->rate >= 16000) {
      mode = MODE_WIDEBAND;
    } else {
      mode = MODE_NARROWBAND;
    }

  gtk_option_menu_set_history (option_menu, mode);
  g_object_set_data (G_OBJECT(dialog), "mode_choice",
		       GINT_TO_POINTER(mode));
}

static void
speex_encode_options_set_features (GtkWidget * dialog, int features)
{
  GtkOptionMenu * option_menu;
  int i;

  option_menu =
    GTK_OPTION_MENU(g_object_get_data (G_OBJECT(dialog), "features_menu"));

  for (i = 0; feature_choices[i].name != NULL; i++) {
    if (feature_choices[i].number == features)
      gtk_option_menu_set_history (option_menu, i);
  }

  g_object_set_data (G_OBJECT(dialog), "features_choice",
		       GINT_TO_POINTER(features));
}

static void
speex_encode_options_features_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog = GTK_WIDGET (data);
  int features;

  features = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(widget), "default"));

  g_object_set_data (G_OBJECT(dialog), "features_choice",
		       GINT_TO_POINTER(features));

  speex_encode_options_update_cb (dialog, data);
}


static void
speex_encode_options_reset_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;

  GtkObject * adj;
  int features, quality, complexity, framepack;

  dialog = gtk_widget_get_toplevel (widget);

  /* Mode menu */
  speex_encode_options_mode_auto_cb (widget, data);

  /* Features menu */
  features = prefs_get_int (FEATURES_KEY, DEFAULT_FEATURES);

  speex_encode_options_set_features (dialog, features);

  /* Quality */

  adj = GTK_OBJECT(g_object_get_data (G_OBJECT(dialog), "quality_adj"));

  quality = prefs_get_int (QUALITY_KEY, DEFAULT_QUALITY);

  gtk_adjustment_set_value (GTK_ADJUSTMENT(adj), (float)quality);

  /* Complexity */

  adj = GTK_OBJECT(g_object_get_data (G_OBJECT(dialog), "complexity_adj"));

  complexity = prefs_get_int (COMPLEXITY_KEY, DEFAULT_COMPLEXITY);

  gtk_adjustment_set_value (GTK_ADJUSTMENT(adj), (float)complexity);

  /* Framepack */

  adj = GTK_OBJECT(g_object_get_data (G_OBJECT(dialog), "framepack_adj"));

  framepack = prefs_get_int (FRAMEPACK_KEY, DEFAULT_FRAMEPACK);

  gtk_adjustment_set_value (GTK_ADJUSTMENT(adj), (float)framepack);

  speex_encode_options_update_cb (widget, data);
}

static void
speex_encode_options_default_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;
  GtkObject * quality_adj;
  GtkObject * complexity_adj;
  GtkObject * framepack_adj;

  dialog = gtk_widget_get_toplevel (widget);

  /* Mode menu */
  speex_encode_options_mode_auto_cb (widget, data);

  /* Features menu */
  speex_encode_options_set_features (dialog, DEFAULT_FEATURES);

  /* Quality */
  quality_adj =
    GTK_OBJECT(g_object_get_data (G_OBJECT(dialog), "quality_adj"));
  gtk_adjustment_set_value (GTK_ADJUSTMENT(quality_adj), DEFAULT_QUALITY);

  /* Complexity */
  complexity_adj =
    GTK_OBJECT(g_object_get_data (G_OBJECT(dialog), "complexity_adj"));
  gtk_adjustment_set_value (GTK_ADJUSTMENT(complexity_adj),
			    DEFAULT_COMPLEXITY);

  /* Framepack */
  framepack_adj =
    GTK_OBJECT(g_object_get_data (G_OBJECT(dialog), "framepack_adj"));
  gtk_adjustment_set_value (GTK_ADJUSTMENT(framepack_adj), DEFAULT_FRAMEPACK);

  speex_encode_options_update_cb (widget, data);
}


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
  gchar new_text [256];

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
create_speex_encoding_options_dialog (sw_sample * sample, char * pathname)
{
  GtkWidget * dialog;
  GtkWidget * ok_button, * button;
  GtkWidget * main_vbox;
  GtkWidget * ebox;
  GtkWidget * vbox;
  GtkWidget * hbox, * hbox2;
  GtkWidget * option_menu;
  GtkWidget * menu;
  GtkWidget * menuitem;
  GtkWidget * table;
  GtkWidget * label;
  GtkWidget * pixmap;

  GtkWidget * notebook;

  GtkWidget * checkbutton;
  GtkObject * quality_adj;
  GtkWidget * quality_hscale;
  GtkObject * complexity_adj;
  GtkWidget * complexity_hscale;
  GtkObject * framepack_adj;
  GtkWidget * framepack_spin;

  GtkWidget * entry;

  GtkWidget * separator;
  GtkTooltips * tooltips;
  GtkWidget *speex_logo;

/*  GtkStyle * style; */

  int i;
  long l;

  dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW(dialog),
			_("Sweep: Speex save options"));
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  sweep_set_window_icon(GTK_WINDOW(dialog));

  attach_window_close_accel(GTK_WINDOW(dialog));

  g_object_set_data (G_OBJECT(dialog), "pathname", pathname);

  main_vbox = GTK_DIALOG(dialog)->vbox;

  ebox = gtk_event_box_new ();
  gtk_box_pack_start (GTK_BOX(main_vbox), ebox, FALSE, TRUE, 0);
  gtk_widget_set_style (ebox, style_bw);
  gtk_widget_show (ebox);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(ebox), vbox);
  gtk_widget_show (vbox);

  /* Ogg Speex pixmaps */

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER(hbox), 4);
  gtk_widget_show (hbox);

  speex_logo = create_widget_from_xpm (dialog, speex_logo_xpm);
  gtk_box_pack_start (GTK_BOX(hbox), speex_logo, FALSE, FALSE, 0);
  gtk_widget_show (speex_logo);

   /* filename */


/* worth changing this over to pango?

  style = gtk_style_new ();
  gdk_font_unref (style->font);
  style->font =
  gdk_font_load("-*-helvetica-medium-r-normal-*-*-180-*-*-*-*-*-*");
  gtk_widget_push_style (style);
*/
  label = gtk_label_new (g_path_get_basename (pathname));
  gtk_box_pack_start (GTK_BOX(vbox), label, TRUE, FALSE, 0);
  gtk_widget_show (label);

/* gtk_widget_pop_style (); */

  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX(main_vbox), notebook, TRUE, TRUE, 4);
  gtk_widget_show (notebook);

  label = gtk_label_new (_("Speex encoding"));

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), vbox, label);
  gtk_container_set_border_width (GTK_CONTAINER(vbox), 4);
  gtk_widget_show (vbox);

  /* Mode */

  hbox = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, TRUE, 4);
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Mode:"));
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  option_menu = gtk_option_menu_new ();
  gtk_box_pack_start (GTK_BOX (hbox), option_menu, TRUE, TRUE, 4);
  gtk_widget_show (option_menu);

  menu = gtk_menu_new ();

  for (i = 0; mode_choices[i].name != NULL; i++) {
    menuitem =
      gtk_menu_item_new_with_label (_(mode_choices[i].name));
    gtk_menu_append (GTK_MENU(menu), menuitem);
    g_object_set_data (G_OBJECT(menuitem), "default",
			      GINT_TO_POINTER(mode_choices[i].number));
    gtk_widget_show (menuitem);

    g_signal_connect (G_OBJECT(menuitem), "activate",
			G_CALLBACK(speex_encode_options_mode_cb),
			dialog);
  }
  gtk_option_menu_set_menu (GTK_OPTION_MENU(option_menu), menu);

  g_object_set_data (G_OBJECT(dialog), "mode_menu", option_menu);

  button = gtk_button_new_with_label (_("Auto"));
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 4);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK(speex_encode_options_mode_auto_cb),
		      sample);
  gtk_widget_show (button);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, button,
			_("Automatically select the encoding mode based on "
			  "the sampling rate of the file."),
			NULL);

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX(vbox), separator, FALSE, TRUE, 4);
  gtk_widget_show (separator);

  /* Features */

  option_menu = gtk_option_menu_new ();
  gtk_box_pack_start (GTK_BOX (vbox), option_menu, FALSE, FALSE, 4);
  gtk_widget_show (option_menu);

  menu = gtk_menu_new ();

  for (i = 0; feature_choices[i].name != NULL; i++) {
    menuitem =
      gtk_menu_item_new_with_label (_(feature_choices[i].name));
    gtk_menu_append (GTK_MENU(menu), menuitem);
    g_object_set_data (G_OBJECT(menuitem), "default",
			      GINT_TO_POINTER(feature_choices[i].number));
    gtk_widget_show (menuitem);

    g_signal_connect (G_OBJECT(menuitem), "activate",
			G_CALLBACK(speex_encode_options_features_cb),
			dialog);

    if (feature_choices[i].desc != NULL) {
      tooltips = gtk_tooltips_new ();
      gtk_tooltips_set_tip (tooltips, menuitem, _(feature_choices[i].desc),
			    NULL);
    }
  }
  gtk_option_menu_set_menu (GTK_OPTION_MENU(option_menu), menu);

  g_object_set_data (G_OBJECT(dialog), "features_menu", option_menu);


  table = gtk_table_new (3, 2, TRUE);
  gtk_box_pack_start (GTK_BOX(vbox), table, FALSE, FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER(table), 8);
  gtk_widget_show (table);

  /* quality */

  hbox = gtk_hbox_new (FALSE, 4);
  /*gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 4);*/
  gtk_table_attach (GTK_TABLE(table), hbox, 0, 1, 0, 1,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  /*gtk_container_set_border_width (GTK_CONTAINER(hbox), 12);*/
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Encoding quality:"));
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 4);
  gtk_widget_show (label);

  g_object_set_data (G_OBJECT (dialog), "quality_label", label);

  quality_adj = gtk_adjustment_new (DEFAULT_QUALITY, /* value */
				    1.0,  /* lower */
				    11.0, /* upper */
				    1.0,  /* step incr */
				    1.0,  /* page incr */
				    1.0   /* page size */
				    );

  {
    /* How sucky ... we create a vbox in order to center the hscale within
     * its allocation, thus actually lining it up with its label ...
     */
    GtkWidget * vbox_pants;

    vbox_pants = gtk_vbox_new (FALSE, 0);
    /*gtk_box_pack_start (GTK_BOX(hbox), vbox_pants, TRUE, TRUE, 0);*/
    gtk_table_attach (GTK_TABLE(table), vbox_pants, 1, 2, 0, 1,
		      GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show (vbox_pants);

    quality_hscale = gtk_hscale_new (GTK_ADJUSTMENT(quality_adj));
    gtk_box_pack_start (GTK_BOX (vbox_pants), quality_hscale, TRUE, TRUE, 0);
    gtk_scale_set_draw_value (GTK_SCALE (quality_hscale), TRUE);
    gtk_scale_set_digits (GTK_SCALE (quality_hscale), 0);
    gtk_widget_set_usize (quality_hscale, gdk_screen_width() / 8, -1);
    gtk_widget_show (quality_hscale);

    g_object_set_data (G_OBJECT (dialog), "quality_hscale",
			 quality_hscale);

    label = gtk_label_new (NULL);
    gtk_box_pack_start (GTK_BOX(vbox_pants), label, FALSE, FALSE, 0);
    gtk_widget_show (label);
  }

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, quality_hscale,
			_("Encoding quality between 0 (lowest quality, "
			  "smallest file) and 10 (highest quality, largest "
			  "file)."),
			NULL);

  g_object_set_data (G_OBJECT (dialog), "quality_adj", quality_adj);

  /* Bit rate */

  checkbutton =
    gtk_check_button_new_with_label (_("Enable bitrate management"));
  gtk_table_attach (GTK_TABLE(table), checkbutton, 0, 2, 1, 2,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (checkbutton);

  g_signal_connect (G_OBJECT(checkbutton), "toggled",
		      G_CALLBACK(speex_encode_options_update_cb),
		      dialog);

  g_object_set_data (G_OBJECT (dialog), "br_chb", checkbutton);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, checkbutton,
			_("For non-VBR (constant bitrate) encoding, "
			  "this sets the maximum bitrate."
			  "For VBR encoding, this sets the average bitrate."),
			NULL);

  label = gtk_label_new (_("Average bitrate"));
  gtk_table_attach (GTK_TABLE(table), label, 0, 1, 2, 3,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (label);

  g_object_set_data (G_OBJECT (dialog), "br_label", label);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE(table), hbox, 1, 2, 2, 3,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (hbox);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX(hbox), entry, TRUE, TRUE, 0);
  gtk_widget_show (entry);

  g_object_set_data (G_OBJECT (dialog), "br_entry", entry);

  label = gtk_label_new (_("bps"));
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 4);
  gtk_widget_show (label);

  g_object_set_data (G_OBJECT (dialog), "br_units", label);

  label = gtk_label_new (_("Extra"));

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), vbox, label);
  gtk_container_set_border_width (GTK_CONTAINER(vbox), 4);
  gtk_widget_show (vbox);

  table = gtk_table_new (2, 2, TRUE);
  gtk_box_pack_start (GTK_BOX(vbox), table, FALSE, FALSE, 4);
  /*gtk_container_set_border_width (GTK_CONTAINER(table), 8);*/
  gtk_widget_show (table);


  /* Encoder complexity */

  hbox = gtk_hbox_new (FALSE, 4);
  /*gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 4);*/
  gtk_table_attach (GTK_TABLE(table), hbox, 0, 1, 0, 1,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  /*gtk_container_set_border_width (GTK_CONTAINER(hbox), 12);*/
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Encoding complexity:"));
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 4);
  gtk_widget_show (label);

  complexity_adj = gtk_adjustment_new (DEFAULT_COMPLEXITY, /* value */
				       1.0,  /* lower */
				       11.0, /* upper */
				       1.0,  /* step incr */
				       1.0,  /* page incr */
				       1.0   /* page size */
				       );

  {
    /* How sucky ... we create a vbox in order to center the hscale within
     * its allocation, thus actually lining it up with its label ...
     */
    GtkWidget * vbox_pants;

    vbox_pants = gtk_vbox_new (FALSE, 0);
    /*gtk_box_pack_start (GTK_BOX(hbox), vbox_pants, TRUE, TRUE, 0);*/
    gtk_table_attach (GTK_TABLE(table), vbox_pants, 1, 2, 0, 1,
		      GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show (vbox_pants);

    complexity_hscale = gtk_hscale_new (GTK_ADJUSTMENT(complexity_adj));
    gtk_box_pack_start (GTK_BOX (vbox_pants), complexity_hscale,
			TRUE, TRUE, 0);
    gtk_scale_set_draw_value (GTK_SCALE (complexity_hscale), TRUE);
    gtk_scale_set_digits (GTK_SCALE (complexity_hscale), 0);
    gtk_widget_set_usize (complexity_hscale, gdk_screen_width() / 8, -1);
    gtk_widget_show (complexity_hscale);

    label = gtk_label_new (NULL);
    gtk_box_pack_start (GTK_BOX(vbox_pants), label, FALSE, FALSE, 0);
    gtk_widget_show (label);
  }

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, complexity_hscale,
			_("This sets the encoding speed/quality tradeoff "
			  "between 0 (faster encoding) "
			  "and 10 (slower encoding)"),
			NULL);

  g_object_set_data (G_OBJECT (dialog), "complexity_adj", complexity_adj);

  /* Frames per packet */

  hbox = gtk_hbox_new (FALSE, 4);
  /*gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 4);*/
  gtk_table_attach (GTK_TABLE(table), hbox, 0, 1, 1, 2,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  /*gtk_container_set_border_width (GTK_CONTAINER(hbox), 12);*/
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Speex frames per Ogg packet:"));
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 4);
  gtk_widget_show (label);

  hbox = gtk_hbox_new (FALSE, 4);
  /*gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 4);*/
  gtk_table_attach (GTK_TABLE(table), hbox, 1, 2, 1, 2,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  /*gtk_container_set_border_width (GTK_CONTAINER(hbox), 12);*/
  gtk_widget_show (hbox);

  framepack_adj = gtk_adjustment_new (DEFAULT_FRAMEPACK, /* value */
				      1.0,  /* lower */
				      10.0, /* upper */
				      1.0,  /* step incr */
				      1.0,  /* page incr */
				      1.0   /* page size */
				      );

  framepack_spin = gtk_spin_button_new (GTK_ADJUSTMENT(framepack_adj),
					  1.0, 0);
  gtk_box_pack_start (GTK_BOX (hbox), framepack_spin, FALSE, FALSE, 0);
  gtk_widget_show (framepack_spin);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, framepack_spin,
			_("Number of Speex frames to pack into each Ogg "
			  "packet. Higher values save space at low "
			  "bitrates."),
			NULL);

  g_object_set_data (G_OBJECT (dialog), "framepack_adj", framepack_adj);

  /* Remember / Reset */

  hbox = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX(main_vbox), hbox, FALSE, FALSE, 4);
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
		      G_CALLBACK(speex_encode_options_reset_cb), sample);
  gtk_widget_show (button);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, button,
			_("Reset to the last remembered encoding options."),
			NULL);

  /* Call the reset callback now to set remembered options */
  speex_encode_options_reset_cb (button, sample);

  button = gtk_button_new_with_label (_("Defaults"));
  gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, TRUE, 4);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK(speex_encode_options_default_cb),
		      sample);
  gtk_widget_show (button);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, button,
			_("Automatically select best encoding options for this file."),
			NULL);

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
			  "may create incompatibilities with streaming "
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
    gtk_label_new (_("Speex is a high quality speech codec designed for\n"
		     "voice over IP (VoIP) and file-based compression.\n"
		     "It is free, open and unpatented."));
  gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  hbox = gtk_hbox_new (FALSE, 16);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  label =
    gtk_label_new (_("Ogg, Speex, Xiph.org Foundation and their logos\n"
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
		      G_CALLBACK (speex_save_options_dialog_ok_cb),
		      sample);

  /* Cancel */

  button = gtk_button_new_with_label (_("Don't save"));
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), button,
		      TRUE, TRUE, 0);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK (speex_save_options_dialog_cancel_cb),
		      sample);

  gtk_widget_grab_default (ok_button);

  return (dialog);
}

int
speex_save_options_dialog (sw_sample * sample, char * pathname)
{
  GtkWidget * dialog;

  dialog = create_speex_encoding_options_dialog (sample, pathname);
  gtk_widget_show (dialog);

  return 0;
}

#endif /* HAVE_SPEEX */
