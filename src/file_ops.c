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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <config.h>

#ifdef USE_SGI_HEADERS
#include <dmedia/audiofile.h>
#else
#include <audiofile.h>
#endif

#include "glib.h"

#include "sweep_types.h"
#include "file_ops.h"
#include "sweep_typeconvert.h"
#include "sweep_sample.h"
#include "view.h"

sw_sample *
sample_load(char * pathname)
{
#ifdef HAVE_LIBAUDIOFILE
  AFframecount framecount;
  AFfilehandle samplefile;
  float framesize;
  int channelcount, sampleformat, samplewidth;
  sw_sample * s;
  char *fn, *dn;
  double rate;
  gint i;
  /* Length in frames of temporary loading buffer */
#define LOAD_BUFFER_LEN 512
  gpointer load_buffer;
  gint buffer_frames; /* nr of frames in the loading buffer */
  gpointer copydata;

  if (!pathname) return NULL;

  samplefile = afOpenFile(pathname, "r", NULL);
  if (samplefile <= 0)
    /*    return (int) samplefile;*/
    return NULL;

  framecount = afGetFrameCount(samplefile, AF_DEFAULT_TRACK);
  if (framecount <= 0)
    return NULL;

  channelcount = afGetChannels(samplefile, AF_DEFAULT_TRACK);
  if (channelcount <= 0)
    return NULL;

  afGetSampleFormat(samplefile, AF_DEFAULT_TRACK, &sampleformat, &samplewidth);

#if defined(i386) || defined(alpha)
  afSetVirtualByteOrder(samplefile, AF_DEFAULT_TRACK,
			AF_BYTEORDER_LITTLEENDIAN);
#else
  afSetVirtualByteOrder(samplefile, AF_DEFAULT_TRACK,
			AF_BYTEORDER_BIGENDIAN);
#endif

  if (samplewidth != 16 && samplewidth != 8) {
    fprintf(stderr, "Sample width unsupported\n");
    return 0;
  }
  framesize = afGetFrameSize(samplefile, AF_DEFAULT_TRACK, 0);

  rate = afGetRate (samplefile, AF_DEFAULT_TRACK);

#ifdef DEBUG
  printf("sweep: Opened filename: %s\n"
	 "\tchannelcount: %d\n"
	 "\tsamplewidth: %d\n"
	 "\tsampleformat: %d\n"
	 "\tframecount: %d\n"
	 "\tframesize: %f\n",
	 pathname, channelcount, samplewidth, sampleformat,
	 framecount, framesize);
#endif

  /* Split directory and file names */
  fn = strrchr (pathname, '/');
  if (fn) {
    *fn++ = '\0';
    dn = pathname;
  } else {
    fn = pathname;
    dn = NULL;
  }

  s = sample_new_empty(dn, fn, channelcount, (int)rate, framecount);

  if(!s) return NULL;

  s->sounddata->data =
    g_malloc(frames_to_bytes(s->sounddata->format, framecount));

  if (s->sounddata->data == NULL) {
    fprintf(stderr,  "s->sounddata->data NULL");
    return NULL;
  }

  copydata = s->sounddata->data;

  load_buffer = g_malloc (LOAD_BUFFER_LEN * (samplewidth / 8) * channelcount);

  while (framecount > 0) {
    buffer_frames = MIN(framecount, LOAD_BUFFER_LEN);
    afReadFrames(samplefile, AF_DEFAULT_TRACK, load_buffer, buffer_frames);
    if (samplewidth == 16) {
      /* Convert from 16 bit samples */
      for (i = 0; i < (buffer_frames * channelcount); i++) {
	((sw_audio_t *)copydata)[i] = SW_AUDIO_T_MAX * (sw_audio_t)
	  (((gint16 *)load_buffer)[i]) / 32768.0;
      }
    } else {
      /* Convert from 8 bit samples */
      /* XXX: this does not deal properly with [un?]signed 8 bit data */
      for (i = 0; i < (buffer_frames * channelcount); i++) {
	((sw_audio_t *)copydata)[i] = SW_AUDIO_T_MAX * (sw_audio_t)
	  (((guint8 *)load_buffer)[i] - 128) / 128.0;
      }
    }
    copydata += frames_to_bytes (s->sounddata->format, buffer_frames);
    framecount -= buffer_frames;
  }

  free (load_buffer);

  afCloseFile(samplefile);

  return s;

#endif /* HAVE_LIBAUDIOFILE */
  return 0;
}

int
sample_load_with_view (char * pathname)
{
  sw_sample * s;
  sw_view * v;

  s = sample_load (pathname);

  if (!s) return -1;

  v = view_new_all (s, 1.0);
  sample_add_view (s, v);
  sample_bank_add (s);
  
  return 0;
}

int
sample_save (sw_sample * s)
{
  sw_format * f = s->sounddata->format;
  char pathname [SW_DIR_LEN];

  /* Length in frames of temporary loading buffer */
#define SAVE_BUFFER_LEN 512
  gpointer save_buffer;
  gint buffer_frames; /* nr of frames in the saving buffer */
  gpointer copydata;
  gint i;

#ifdef HAVE_LIBAUDIOFILE
  AFfilehandle outputFile;
  AFfilesetup outputSetup;
  AFframecount framecount=0;
  int file_format;
  int channelcount=1, samplewidth=16;

  if (!s) return -1;

  /* Hardcoded ;) */
  file_format = AF_FILE_WAVE;

  outputSetup = afNewFileSetup();
  afInitFileFormat(outputSetup, file_format);
  afInitSampleFormat (outputSetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
  afInitRate(outputSetup, AF_DEFAULT_TRACK, f->rate);
  afInitChannels(outputSetup, AF_DEFAULT_TRACK, f->channels);

  if (*s->directory) {
    snprintf (pathname, SW_DIR_LEN,
	      "%s/%s", s->directory, s->filename);
    outputFile = afOpenFile(pathname, "w", outputSetup);
  } else {  
    outputFile = afOpenFile(s->filename, "w", outputSetup);
  }

  afFreeFileSetup(outputSetup);
  if(!outputFile) return -1;

  afSetVirtualByteOrder(outputFile, AF_DEFAULT_TRACK,
			AF_BYTEORDER_LITTLEENDIAN);

  copydata = s->sounddata->data;

  channelcount = s->sounddata->format->channels;
  save_buffer = g_malloc (SAVE_BUFFER_LEN * (samplewidth / 8) * channelcount);

  framecount = s->sounddata->nr_frames;
  while (framecount > 0) {
    buffer_frames = MIN(framecount, SAVE_BUFFER_LEN);

    /* Convert to 16 bit samples */
    for (i = 0; i < (buffer_frames * channelcount); i++) {
      ((gint16 *)save_buffer)[i] =
	(gint16)(((sw_audio_t *)copydata)[i] * 32768.0 / SW_AUDIO_T_MAX);
    }
    afWriteFrames(outputFile, AF_DEFAULT_TRACK, save_buffer, buffer_frames);

    copydata += frames_to_bytes (s->sounddata->format, buffer_frames);
    framecount -= buffer_frames;
  }

  free (save_buffer);

  afCloseFile(outputFile);

  return 0;
#endif /* HAVE_LIBAUDIOFILE */
}
