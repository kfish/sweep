/*
 * Sweep, a sound wave editor.
 *
 * Copyright (C) 2000 Conrad Parker
 * Copyright (C) 2002 CSIRO Australia
 *
 */

/*
 * This file adapted from "player.c" in the "mad MPEG audio decoder"
 * source distribution:
 *
 * Copyright (C) 2000-2001 Robert Leslie
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


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_MAD

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>

#include <mad.h>

#define BUFFER_LEN 4096

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


static gboolean
file_is_mpeg_audio (const char * pathname)
{
  int fd;

#define BUF_LEN 2048
  unsigned char buf[BUF_LEN];
  int n, i;

  fd = open (pathname, O_RDONLY);
  if (fd == -1) goto out_false;

  n = read (fd, buf, BUF_LEN);
  if (n < 4) goto out_false;

  /* Check for MPEG frame marker */
  for (i = 0; i < BUF_LEN-1; i++) {
    if ((buf[i] & 0xff) == 0xff && (buf[i+1] & 0xe0) == 0xe0) {
      goto out_true;
    }
  }

 out_false:
  close (fd);
  return FALSE;

 out_true:
  close (fd);
  return TRUE;

}

/*
 * This is a private message structure. A generic pointer to this structure
 * is passed to each of the callback functions. Put here any data you need
 * to access from within the callbacks.
 */

struct mad_info {
  sw_sample * sample;

  /* file size */
  size_t length;

  /* input buffer */
  unsigned char const *start;
  unsigned long offset;
  unsigned long remaining;
  unsigned char * end_buffer;
  int eof;

  /* output */
  sw_framecount_t nr_frames;

};

/*
 * This is the input callback. The purpose of this callback is to (re)fill
 * the stream buffer which is to be decoded. In this example, an entire file
 * has been mapped into memory, so we just call mad_stream_buffer() with the
 * address and length of the mapping. When this callback is called a second
 * time, we are finished decoding.
 */

static enum mad_flow
input(void *data, struct mad_stream *stream)
{ 
  struct mad_info * info = data;
  unsigned long n;

  if (info->eof)
    return MAD_FLOW_STOP;

  if (stream->next_frame) {

    info->offset = stream->next_frame - info->start;
    info->remaining = info->length - info->offset;

    n = MIN (info->remaining, BUFFER_LEN);

    if (n == info->remaining) {

      info->end_buffer = g_malloc0 (info->remaining + MAD_BUFFER_GUARD);
      if (info->end_buffer == NULL)
	return MAD_FLOW_BREAK;
      
      info->eof = 1;
      
      memcpy (info->end_buffer, info->start + info->offset, info->remaining);
      
      mad_stream_buffer (stream, info->end_buffer,
			 info->remaining + MAD_BUFFER_GUARD);

      info->offset += n;
      info->remaining -= n;
      
      return MAD_FLOW_CONTINUE;
    }
  } else {
    n = MIN (info->remaining, BUFFER_LEN);
  }

  mad_stream_buffer(stream, info->start + info->offset, n);

  info->offset += n;
  info->remaining -= n;

  return MAD_FLOW_CONTINUE;
}

/*
 * This is the output callback function. It is called after each frame of
 * MPEG audio data has been completely decoded. The purpose of this callback
 * is to output (or play) the decoded PCM audio.
 */

static
enum mad_flow output(void *data,
                     struct mad_header const *header,
                     struct mad_pcm *pcm)
{
  struct mad_info * info = data;
  sw_sample * sample = info->sample;
  sw_framecount_t data_start;
  sw_audio_t * d;
  int i, j;
  gint percent;

  gboolean active = TRUE;

  g_mutex_lock (sample->ops_mutex);

  if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
    active = FALSE;
  } else {

    sample->sounddata->format->channels = pcm->channels;
    sample->sounddata->format->rate = pcm->samplerate;
    
    data_start = info->nr_frames;
    
    info->nr_frames += pcm->length;

    if (info->nr_frames > sample->sounddata->nr_frames) {
      sample->sounddata->data =
	g_realloc (sample->sounddata->data,
		   frames_to_bytes (sample->sounddata->format,
				    info->nr_frames));
    }
    
    sample->sounddata->nr_frames = info->nr_frames;
    
    d = (sw_audio_t *)sample->sounddata->data;
    d = &d[data_start * pcm->channels];
    
    for (i = 0; i < pcm->channels; i++) {
      for (j = 0; j < pcm->length; j++) {
	d[j*pcm->channels + i] =
	  (sw_audio_t)mad_f_todouble(pcm->samples[i][j]);
      }
    }
    
    percent = (info->length - info->remaining) * 100 / info->length;
    sample_set_progress_percent (info->sample, percent);
    
#ifdef DEBUG
    printf ("decoded %u samples, %d%% percent complete (%u / %u)\n",
	    nsamples, percent, info->remaining, info->length);
#endif
    
  }
  g_mutex_unlock (sample->ops_mutex);

  return (active ? MAD_FLOW_CONTINUE : MAD_FLOW_STOP);
}

/* 
 * This is the error callback function. It is called whenever a decoding
 * error occurs. The error is indicated by stream->error; the list of
 * possible MAD_ERROR_* errors can be found in the mad.h (or
 * libmad/stream.h) header file.
 */

static
enum mad_flow error(void *data,
                    struct mad_stream *stream,
                    struct mad_frame *frame)
{
  struct mad_info * info = data;

  switch (stream->error) {
  case MAD_ERROR_BADDATAPTR:
    return MAD_FLOW_CONTINUE;
  case MAD_ERROR_LOSTSYNC:
    if (stream->next_frame)
      return MAD_FLOW_CONTINUE;
    /* else fall through */
  default:

    fprintf(stderr, "decoding error 0x%04x (%s) at byte offset %u\n",
	    stream->error, mad_stream_errorstr(stream),
	    stream->this_frame - info->start);
    break;
  }

  if (MAD_RECOVERABLE (stream->error))
    return MAD_FLOW_CONTINUE;
  else
    return MAD_FLOW_BREAK;
}


static sw_sample *
sample_load_mad_data (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  int fd;
  void * fdm;
  struct stat statbuf;

  struct mad_decoder decoder;
  struct mad_info info;
  int result;

  fd = open (sample->pathname, O_RDONLY);

  if (fstat (fd, &statbuf) == -1 || statbuf.st_size == 0) return NULL;

  fdm = mmap (0, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (fdm == MAP_FAILED) {
    perror (NULL);
    close (fd);
    return NULL;
  }

#if defined (HAVE_MADVISE)
  madvise (fdm, statbuf.st_size, MADV_SEQUENTIAL);
#endif

  info.sample = sample;
  info.length = statbuf.st_size;
  info.start = fdm;
  info.offset = 0;
  info.remaining = (unsigned long) statbuf.st_size;
  info.eof = 0;
  info.end_buffer = NULL;
  info.nr_frames = 0;

  mad_decoder_init (&decoder, &info,
		    input, 0 /* header */, 0 /* filter */, output,
		    error, 0 /* message */);

  result = mad_decoder_run (&decoder, MAD_DECODER_MODE_SYNC);

  mad_decoder_finish (&decoder);

  if (info.end_buffer != NULL) {
    g_free (info.end_buffer);
  }

  if (munmap (fdm, statbuf.st_size) == -1) {
    perror (NULL);
  }

  close (fd);

  stat (sample->pathname, &statbuf);
  sample->last_mtime = statbuf.st_mtime;
  sample->edit_ignore_mtime = FALSE;
  sample->modified = FALSE;

  sample_set_edit_state (sample, SWEEP_EDIT_STATE_DONE);

  return sample;
}

static sw_operation mad_load_op = {
  SWEEP_EDIT_MODE_FILTER,
  (SweepCallback)sample_load_mad_data,
  (SweepFunction)NULL,
  (SweepCallback)NULL, /* undo */
  (SweepFunction)NULL,
  (SweepCallback)NULL, /* redo */
  (SweepFunction)NULL
};

static sw_sample *
sample_load_mad_info (sw_sample * sample, char * pathname)
{
#undef BUF_LEN
#define BUF_LEN 128
  char buf[BUF_LEN];

  gboolean isnew = (sample == NULL);

  sw_view * v;

  if (!file_is_mpeg_audio (pathname)) return NULL;

#if 0
  fd = open (pathname, O_RDONLY);

  if (fstat (fd, &statbuf) == -1 || statbuf.st_size == 0) return NULL;
#endif

  /* Create the sample/sounddata, initially with length 0, to be grown
   * as the file is decoded
   */
  if (sample == NULL) {
    /* Channels and rate will be set during decoding and are basically
     * irrelevent here. Set them to 2, 44100 assuming these are the most
     * likely values, in which case the file info displayed in the window
     * will not change suddenly
     */
    sample = sample_new_empty(pathname, 2, 44100, 0);
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
    /*close (fd);*/
    return NULL;
  }

  sample->file_method = SWEEP_FILE_METHOD_MP3;
  /*sample->file_info = GINT_TO_POINTER(fd);*/

  sample_bank_add(sample);

  if (isnew) {
    v = view_new_all (sample, 1.0);
    sample_add_view (sample, v);
  } else {
    trim_registered_ops (sample, 0);
  }

  g_snprintf (buf, BUF_LEN, _("Loading %s"), g_basename (sample->pathname));

  schedule_operation (sample, buf, &mad_load_op, sample);

  return sample;
}

sw_sample *
mad_sample_reload (sw_sample * sample)
{
  if (sample == NULL) return NULL;

  return sample_load_mad_info (sample, sample->pathname);
}

sw_sample *
mad_sample_load (char * pathname)
{
  if (pathname == NULL) return NULL;

  return sample_load_mad_info (NULL, pathname);
}

#endif /* HAVE_MAD */
