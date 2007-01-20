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

/*
 * ALSA 0.6 support by Paul Davis
 * ALSA 0.9 updates by Zenaan Harkness
 * ALSA 1.0 updates by Daniel Dreschers
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include <sweep/sweep_types.h>
#include <sweep/sweep_sample.h>

#include "driver.h"
#include "pcmio.h"
#include "question_dialogs.h"

#ifdef DRIVER_ALSA

#include <alsa/asoundlib.h>

// shamelessly ripped from alsaplayer alsa-final driver:
#ifndef timersub
#define timersub(a, b, result) \
do { \
	(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
  (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
  if ((result)->tv_usec < 0) { \
		--(result)->tv_sec; \
		(result)->tv_usec += 1000000; \
	} \
} while (0)
#endif

void print_pcm_state (snd_pcm_t * pcm)
{
  switch (snd_pcm_state(pcm)) {
    case SND_PCM_STATE_OPEN:
      fprintf (stderr, "sweep: print_pcm_state: state is OPEN\n");
      break;
    case SND_PCM_STATE_SETUP:
      fprintf (stderr, "sweep: print_pcm_state: state is SETUP\n");
      break;
    case SND_PCM_STATE_PREPARED:
      fprintf (stderr, "sweep: print_pcm_state: state is PREPARED\n");
      break;
    case SND_PCM_STATE_RUNNING:
      fprintf (stderr, "sweep: print_pcm_state: state is RUNNING\n");
      break;
    case SND_PCM_STATE_XRUN:
      fprintf (stderr, "sweep: print_pcm_state: state is XRUN\n");
      break;
    case SND_PCM_STATE_DRAINING:
      fprintf (stderr, "sweep: print_pcm_state: state is DRAINING\n");
      break;
    case SND_PCM_STATE_PAUSED:
      fprintf (stderr, "sweep: print_pcm_state: state is PAUSED\n");
      break;
    case SND_PCM_STATE_SUSPENDED:
      fprintf (stderr, "sweep: print_pcm_state: state is SUSPENDED\n");
      break;
    default:
      fprintf (stderr, "sweep: print_pcm_state: state is unknown! THIS SHOULD NEVER HAPPEN!\n");
  }
}

static GList *
alsa_get_names (void)
{
  GList * names = NULL;
  char * name;

  if ((name = getenv ("SWEEP_ALSA_PCM")) != 0) {
    names = g_list_append (names, name);
  }

  /* The standard command line options for this are -D or --device.
   * The default fallback should be plughw.
   */
  names = g_list_append (names, "plughw:0,0");
  names = g_list_append (names, "plughw:0,1");
  names = g_list_append (names, "plughw:1,0");
  names = g_list_append (names, "plughw:1,1");

  return names;
}

static sw_handle *
alsa_device_open (int monitoring, int flags)
{
  int err;
  char * alsa_pcm_name;
  snd_pcm_t * pcm_handle;
  sw_handle * handle;
  snd_pcm_stream_t stream;

  if (monitoring) {
    if (pcmio_get_use_monitor())
      alsa_pcm_name = pcmio_get_monitor_dev ();
    else
      return NULL;
  } else {
    alsa_pcm_name = pcmio_get_main_dev ();
  }

  if (flags == O_RDONLY) {
    stream = SND_PCM_STREAM_CAPTURE;
  } else if (flags == O_WRONLY) {
    stream = SND_PCM_STREAM_PLAYBACK;
  } else {
    return NULL;
  }

  if ((err = snd_pcm_open(&pcm_handle, alsa_pcm_name, stream, 0)) < 0) {
    sweep_perror (errno,
		  "Error opening ALSA device %s",
		  alsa_pcm_name /*, snd_strerror (err)*/);
    return NULL;
  }

  handle = g_malloc0 (sizeof (sw_handle));

  handle->driver_flags = flags;
  handle->custom_data = pcm_handle;

  return handle;
}

  // /src/alsa/alsaplayer-0.99.72/output/alsa-final/alsa.c
  // /src/alsa/alsa-lib-0.9.0rc3/test/pcm.c
static void
alsa_device_setup (sw_handle * handle, sw_format * format)
{
  int err;
  snd_pcm_t * pcm_handle = (snd_pcm_t *)handle->custom_data;
  snd_pcm_hw_params_t * hwparams;
  int dir;
  unsigned int rate = format->rate;
  unsigned int channels = format->channels;
  unsigned int periods;
  snd_pcm_uframes_t period_size = PBUF_SIZE/format->channels;

#if 1
  if (handle->driver_flags == O_RDONLY) {
    dir = (int)SND_PCM_STREAM_CAPTURE;
  } else if (handle->driver_flags == O_WRONLY) {
    dir = (int)SND_PCM_STREAM_PLAYBACK;
  } else {
    return;
  }
#else
  dir = 0;
#endif

  snd_pcm_hw_params_alloca (&hwparams);

  if ((err = snd_pcm_hw_params_any (pcm_handle, hwparams)) < 0) {
    fprintf(stderr,
	    "sweep: alsa_setup: can't get PCM hw params (%s)\n",
	    snd_strerror(err));
    return;
  }

  if ((err = snd_pcm_hw_params_set_access
       (pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    fprintf(stderr,
	    "sweep: alsa_setup: can't set interleaved access (%s)\n",
	    snd_strerror(err));
    return;
  }

  if ((err = snd_pcm_hw_params_set_format
       (pcm_handle, hwparams, SND_PCM_FORMAT_FLOAT)) < 0) {
    fprintf (stderr,
	     "sweep: alsa_setup: audio interface does not support "
	     "host endian 32 bit float samples (%s)\n",
	     snd_strerror(err));
    return;
  }

  if ((err = snd_pcm_hw_params_set_rate_near
       (pcm_handle, hwparams, &rate, 0 /* dir */)) < 0) {
    fprintf (stderr,
	     "sweep: alsa_setup: audio interface does not support "
	     "sample rate of %d (%s)\n",
	     format->rate, snd_strerror (err));
    /*return;*/
  }

  if ((err = snd_pcm_hw_params_set_channels_near
       (pcm_handle, hwparams, &channels)) < 0) {
    fprintf (stderr,
	     "sweep: alsa_setup: audio interface does not support "
	     "%d channels (%s)\n",
	     format->channels, snd_strerror (err));
    /*return;*/
  }

  if ((err = snd_pcm_hw_params_set_period_size_near
       (pcm_handle, hwparams, &period_size, 0)) < 0) {
    fprintf (stderr,
	     "sweep: alsa_setup: audio interface does not support "
	     "period size of %ld (%s)\n", period_size, snd_strerror (err));
    return;
  }

  periods = LOGFRAGS_TO_FRAGS(pcmio_get_log_frags());

  if ((err = snd_pcm_hw_params_set_periods_near
       (pcm_handle, hwparams, &periods, 0)) < 0) {
    fprintf (stderr,
	     "sweep: alsa_setup: audio interface does not support "
	     "period size of %d (%s) - suprising that we get this err!\n",
	     periods, snd_strerror (err));
    return;
  }

  // see alsa-lib html docs (may have to build them) for methods doco
  // The following is old alsa 0.6 code, which may need including somehow:
  //params.ready_mode = SND_PCM_READY_FRAGMENT;
  //params.start_mode = SND_PCM_START_DATA;
  //params.xrun_mode = SND_PCM_XRUN_FRAGMENT;
  //params.frag_size = PBUF_SIZE / params.format.channels;
  //params.avail_min = params.frag_size;
  // params.buffer_size = 3 * params.frag_size;

  if ((err = snd_pcm_hw_params (pcm_handle, hwparams)) < 0) {
    fprintf (stderr,
	     "sweep: alsa_setup: audio interface could not be configured "
	     "with specified parameters\n");
    return;
  }
  //printf ("sweep: alsa_setup 9\n");

  {
    unsigned int c, r;
    int dir = 0;

    if ((err = snd_pcm_hw_params_get_rate (hwparams, &r, &dir)) < 0) {
      fprintf (stderr,
	       "sweep: alsa_setup: error getting PCM rate (%s)\n",
	       snd_strerror (err));
    }

    if ((err = snd_pcm_hw_params_get_channels (hwparams, &c)) < 0) {
      fprintf (stderr,
	       "sweep: alsa_setup: error getting PCM channels (%s)\n",
	       snd_strerror (err));
    }

#ifdef DEBUG
    fprintf (stderr, "alsa got rate %i, channels %i, dir %d\n", r, c, dir);
#endif

    handle->driver_rate = r;
    handle->driver_channels = c;
  }

  if (snd_pcm_prepare (pcm_handle) < 0) {
    fprintf (stderr, "audio interface could not be prepared for playback\n");
    return;
  }
}

static int
alsa_device_wait (sw_handle * handle)
{
  snd_pcm_t * pcm_handle = (snd_pcm_t *)handle->custom_data;

  if (snd_pcm_wait (pcm_handle, 1000) < 0) {
    fprintf (stderr, "poll failed (%s)\n", strerror (errno));
  }

  return 0;
}

#define PLAYBACK_SCALE (32768 / SW_AUDIO_T_MAX)

static ssize_t
alsa_device_read (sw_handle * handle, sw_audio_t * buf, size_t count)
{
  snd_pcm_t * pcm_handle = (snd_pcm_t *)handle->custom_data;
  snd_pcm_uframes_t uframes;
  int err;

  uframes = count / handle->driver_channels;

  err = snd_pcm_readi (pcm_handle, buf, uframes);

  return err;
}

static ssize_t
alsa_device_write (sw_handle * handle, sw_audio_t * buf, size_t count,
    sw_framecount_t offset)
{
  snd_pcm_t * pcm_handle = (snd_pcm_t *)handle->custom_data;
  snd_pcm_uframes_t uframes;
  snd_pcm_status_t * status;
  int err;

#if 0
  gint16 * bbuf;
  size_t byte_count;
  ssize_t bytes_written;
  int need_bswap;
  int i;

  if (handle == NULL) {
#ifdef DEBUG
    g_print ("handle NULL in write()\n");
#endif
    return -1;
  }

  byte_count = count * sizeof (gint16);
  bbuf = alloca (byte_count);

  for (i = 0; i < count; i++) {
    bbuf[i] = (gint16)(PLAYBACK_SCALE * buf[i]);
  }

  err = snd_pcm_writei(pcm_handle, bbuf, uframes);
#else
  /*printf ("sweep: alsa_write \n");*/

  uframes = count / handle->driver_channels;
  //printf ("sweep: alsa_write 1\n");

  // this basicaly ripped straight out of alsaplayer alsa-final driver:
  err = snd_pcm_writei(pcm_handle, buf, uframes);
#endif

  if (err == -EPIPE) {
    snd_pcm_status_alloca(&status);
    if ((err = snd_pcm_status(pcm_handle, status))<0) {
      fprintf(stderr, "sweep: alsa_write: xrun. can't determine length\n");
    } else {
      if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
        struct timeval now, diff, tstamp;
        gettimeofday(&now, 0);
        snd_pcm_status_get_trigger_tstamp(status, &tstamp);
        timersub(&now, &tstamp, &diff);
        fprintf(stderr, "sweep: alsa_write: xrun of at least %.3f msecs. "
	    "resetting stream\n", diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
      } else {
        fprintf(stderr, "sweep: alsa_write: xrun. can't determine length\n");
      }
    }  
    snd_pcm_prepare(pcm_handle);
    err = snd_pcm_writei(pcm_handle, buf, uframes);
    if (err != uframes) {
      fprintf(stderr, "sweep: alsa_write: %s\n", snd_strerror(err));
      return 0;
    } else if (err < 0) {
      fprintf(stderr, "sweep: alsa_write: %s\n", snd_strerror(err));
      return 0;
    }
  }

  return 1;
}

sw_framecount_t
alsa_device_offset (sw_handle * handle)
{
  /*printf ("sweep: alsa_offset\n");*/
  return -1;
}

static void
alsa_device_reset (sw_handle * handle)
{
  /*printf ("sweep: alsa_reset\n");*/
}

static void
alsa_device_flush (sw_handle * handle)
{
  /*printf ("sweep: alsa_flush\n");*/
}

/*
 * alsa lib provides:
 * int snd_pcm_drop (snd_pcm_t *pcm) // Stop a PCM dropping pending frames.
 * int snd_pcm_drain (snd_pcm_t *pcm) // Stop a PCM preserving pending frames. 
 */
static void
alsa_device_drain (sw_handle * handle)
{
  snd_pcm_t * pcm_handle = (snd_pcm_t *)handle->custom_data;

  if (snd_pcm_drop (pcm_handle) < 0) {
        fprintf (stderr, "audio interface could not be stopped\n");
        return;
  }
  if (snd_pcm_prepare (pcm_handle) < 0) {
        fprintf (stderr, "audio interface could not be re-prepared\n");
        return;
  }
}

static void
alsa_device_close (sw_handle * handle)
{
  snd_pcm_t * pcm_handle = (snd_pcm_t *)handle->custom_data;
 
  snd_pcm_close (pcm_handle);
}

static sw_driver _driver_alsa = {
  alsa_get_names,
  alsa_device_open,
  alsa_device_setup,
  alsa_device_wait,
  alsa_device_read,
  alsa_device_write,
  alsa_device_offset,
  alsa_device_reset,
  alsa_device_flush,
  alsa_device_drain,
  alsa_device_close,
  "alsa_primary_device",
  "alsa_monitor_device",
  "alsa_log_frags"
};

#else

static sw_driver _driver_alsa = {
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

#endif

sw_driver * driver_alsa = &_driver_alsa;
