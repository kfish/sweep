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

#ifdef DRIVER_OSS
#include <sys/soundcard.h>
#define DEV_DSP "/dev/dsp"
#endif

#ifdef DRIVER_SOLARIS_AUDIO
#include <sys/audioio.h>
#include <stropts.h>
#include <sys/conf.h>
#define DEV_DSP "/dev/audio"
#endif

#ifdef DRIVER_ALSA
#include <sys/asoundlib.h>
static snd_pcm_t *pcm_handle;
#define ALSA_PCM_NAME "sweep"
#endif

#define PLAYBACK_SCALE (32768 / SW_AUDIO_T_MAX)
#define PBUF_SIZE 256

static int dev_dsp = -1;

static sw_sample * playing = NULL;
static pthread_t player_thread;

static int playoffset = -1;

/*
 * update_playmarker ()
 *
 * Update the position of the playback marker line for the sample
 * being played.
 *
 * gtk_idle will keep calling this function as long as this sample is
 * playing, unless otherwise stopped.
 */
static gint
update_playmarker (gpointer data)
{
  sw_sample * s = (sw_sample *)data;

  if (s == playing) {
    sample_set_playmarker (s, playoffset);
    return TRUE;
  } else {
    sample_set_playmarker (s, -1);
    return FALSE;
  }
}

static void
start_playmarker (sw_sample * s)
{
  s->playmarker_tag =
    gtk_timeout_add ((guint32)100,
		     (GtkFunction)update_playmarker,
		     (gpointer)s);
}

static void
stop_playmarker (sw_sample * s)
{
  if (s->playmarker_tag > 0)
    gtk_timeout_remove (s->playmarker_tag);
  s->playmarker_tag = 0;
  sample_set_playmarker (s, -1);
}

static int
open_dev_dsp (void)
{
#if defined(DRIVER_OSS) || defined(DRIVER_SOLARIS_AUDIO)
  if((dev_dsp = open(DEV_DSP, O_WRONLY|O_NDELAY, 0)) == -1) {
    perror ("sweep: unable to open device " DEV_DSP);
    return -1; /* XXX: Flag error */
  }

  return dev_dsp;
#elif defined(DRIVER_ALSA)
  int err;
  char *alsa_pcm_name;
  if ((alsa_pcm_name = getenv ("SWEEP_ALSA_PCM")) == 0) {
        alsa_pcm_name = ALSA_PCM_NAME;
  }
  if ((err = snd_pcm_open(&pcm_handle, alsa_pcm_name,
                        SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) {
    fprintf (stderr, "sweep: unable to open ALSA device %s (%s)\n",
           alsa_pcm_name, snd_strerror (err));
    return -1; /* XXX: Flag error */
  }
  dev_dsp = snd_pcm_poll_descriptor (pcm_handle);
#else
  fprintf(stderr, "Warning: No audio device configured\n");
  return -1;
#endif
  return 0;
}

static void
setup_dev_dsp (sw_sample * s)
{
#ifdef DRIVER_OSS
  int mask, format, stereo, frequency;

  if(ioctl(dev_dsp, SNDCTL_DSP_GETFMTS, &mask) == -1)  {
    perror("OSS: error getting format masks");
    close(dev_dsp);
    return; /* XXX: Flag error */
  }

  if(mask&AFMT_U16_LE) {
    format=AFMT_U16_LE;
  }
  if(mask&AFMT_U16_BE) {
    format=AFMT_U16_BE;
  }
  if(mask&AFMT_S16_BE) {
    format=AFMT_S16_BE;
  }
  if(mask&AFMT_U16_LE) {
    format=AFMT_U16_LE;
  }
  if(mask&AFMT_S16_LE) {
    format=AFMT_S16_LE;
  }

  if (ioctl(dev_dsp, SNDCTL_DSP_SETFMT, &format) == -1) {
    perror("OSS: Unable to set format");
    exit(-1);
    }

  stereo = s->sounddata->format->channels - 1;
  if(ioctl(dev_dsp, SNDCTL_DSP_STEREO, &stereo) == -1 ) {
    perror("OSS: Unable to set channels");
  }

  frequency = s->sounddata->format->rate;
  if(ioctl(dev_dsp, SNDCTL_DSP_SPEED, &frequency) == -1 ) {
    perror("OSS: Unable to set playback frequency");
  }
#elif defined(DRIVER_SOLARIS_AUDIO)

  audio_info_t info;
  AUDIO_INITINFO(&info);
  info.play.precision = 16;	/* cs4231 doesn't handle 16-bit linear PCM */
  info.play.encoding = AUDIO_ENCODING_LINEAR;
  info.play.channels = s->sounddata->format->channels;
  info.play.sample_rate = s->sounddata->format->rate;
  if(ioctl(dev_dsp, AUDIO_SETINFO, &info) < 0)
      perror("Unable to configure audio device");
#elif defined(DRIVER_ALSA)
  snd_pcm_params_t params;
  snd_pcm_params_info_t params_info;
  int err = 0;

  memset (&params, 0, sizeof(params));
  memset (&params_info, 0, sizeof(params_info));

  if (snd_pcm_params_info (pcm_handle, &params_info) < 0) {
        fprintf(stderr, "cannot get audio interface parameters (%s)\n",
                snd_strerror(err));
          return;
  }

  if (params_info.formats & SND_PCM_FMT_S16_LE) {
    params.format.sfmt = SND_PCM_SFMT_S16_LE;
  } else {
    fprintf (stderr, "audio interface does not support "
	     "linear 16 bit little endian samples\n");
    return;
  }

  switch (s->sounddata->format->rate) {
  case 44100:
        if (params_info.rates & SND_PCM_RATE_44100) {
                params.format.rate = 44100;
        } else {
                fprintf (stderr, "audio interface does not support "
                         "44.1kHz sample rate (0x%x)\n",
                         params_info.rates);
                return;
        }
        break;

  case 48000:
        if (params_info.rates & SND_PCM_RATE_48000) {
                params.format.rate = 48000;
        } else {
                fprintf (stderr, "audio interface does not support "
                         "48kHz sample rate\n");
                return;
        }
        break;

  default:
        fprintf (stderr, "audio interface does not support "
                 "a sample rate of %d\n",
                 s->sounddata->format->rate);
        return;
  }

  if (s->sounddata->format->channels < params_info.min_channels ||
      s->sounddata->format->channels > params_info.max_channels) {
      fprintf (stderr, "audio interface does not support %d channels\n",
             s->sounddata->format->channels);
      return;
  }
  params.format.channels = s->sounddata->format->channels;
  params.ready_mode = SND_PCM_READY_FRAGMENT;
  params.start_mode = SND_PCM_START_DATA;
  params.xrun_mode = SND_PCM_XRUN_FRAGMENT;
  params.frag_size = PBUF_SIZE / params.format.channels;
  params.avail_min = params.frag_size;
  // params.buffer_size = 3 * params.frag_size;

  if ((err = snd_pcm_params (pcm_handle, &params)) < 0) {
        fprintf (stderr, "audio interface could not be configured "
                 "with the specified parameters\n");
        return;
  }

  if (snd_pcm_prepare (pcm_handle) < 0) {
        fprintf (stderr, "audio interface could not be prepared "
                 "for playback\n");
        return;
  }
#endif
}

static void
reset_dev_dsp (void)
{
#ifdef DRIVER_OSS
  if(ioctl (dev_dsp, SNDCTL_DSP_RESET) == -1) {
    perror ("OSS: error resetting " DEV_DSP);
  }
#endif
}

static void
flush_dev_dsp (void)
{
#ifdef DRIVER_SOLARIS_AUDIO
  if (ioctl(dev_dsp, I_FLUSH, FLUSHW) == -1)
    perror("I_FLUSH");
#endif
}

static void
drain_dev_dsp (void)
{
#ifdef DRIVER_OSS
  if(ioctl (dev_dsp, SNDCTL_DSP_POST) == -1) {
    perror ("OSS: POST error on " DEV_DSP);
  }
#endif

#ifdef DRIVER_SOLARIS_AUDIO
  if(ioctl(dev_dsp, AUDIO_DRAIN, 0) == -1)
      perror("AUDIO_DRAIN");
#endif

#ifdef DRIVER_ALSA
  if (snd_pcm_drop (pcm_handle) < 0) {
        fprintf (stderr, "audio interface could not be stopped\n");
        return;
  }
  if (snd_pcm_prepare (pcm_handle) < 0) {
        fprintf (stderr, "audio interface could not be re-prepared\n");
        return;
  }
#endif

}

/*
 * WAIT_FOR_PLAYING
 *
 * This busy waits until 'playing' is set to NULL..
 * This is done to retain control of the dev_dsp device
 * so that it can be reset to "immediately"
 * stop playback when interrupted.
 */
#define WAIT_FOR_PLAYING \
  while (playing) usleep (100000)

static void
close_dev_dsp (void)
{
#if defined(DRIVER_OSS) || defined(DRIVER_SOLARIS_AUDIO)
  close (dev_dsp);
#elif defined(DRIVER_ALSA)
  snd_pcm_close (pcm_handle);
#endif
  dev_dsp = -1;
}

static void
play_view(sw_view * view, sw_framecount_t start, sw_framecount_t end, gfloat relpitch)
{
  sw_sample * s = view->sample;
  fd_set fds;
  ssize_t n = 0;
  sw_audio_t * d;
  gint16 pbuf[PBUF_SIZE];
  gint sbytes, channels;
  gdouble po = 0.0, p, endf;
  gint i=0, si=0;

  d = (sw_audio_t *)s->sounddata->data;

  sbytes = 2;
 
  channels = s->sounddata->format->channels;

  playoffset = start;
  po = (gdouble)(start);
  endf = (gdouble)(end);

  while ((po <= endf) && playing) {
    FD_ZERO (&fds);
    FD_SET (dev_dsp, &fds);
    
    if (select (dev_dsp+1, NULL, &fds, NULL, NULL) == 0);

    memset (pbuf, 0, sizeof (pbuf));

    switch (channels) {
    case 1:
      for (i = 0; i < PBUF_SIZE; i++) {
	si = (int)floor(po);
	p = po - (gdouble)si;
	((gint16 *)pbuf)[i] =
	  (gint16)(PLAYBACK_SCALE * view->vol *
		   (d[si] * p + d[si+channels] * (1 - p)));
	po += relpitch;
	if (po > endf) break;
      }
      
      break;
    case 2:
      for (i = 0; i < PBUF_SIZE; i++) {
	si = (int)floor(po);
	p = po - (gdouble)si;
	si *= 2;
	((gint16 *)pbuf)[i] =
	  (gint16)(PLAYBACK_SCALE * view->vol *
		   (d[si] * p + d[si+channels] * (1 - p)));
	i++; si++;
	((gint16 *)pbuf)[i] =
	  (gint16)(PLAYBACK_SCALE * view->vol *
		   (d[si] * p + d[si+channels] * (1 - p)));
	po += relpitch;
	if (po > endf) break;
      }
      
      break;
    }

    /* Only write if still playing */
    if (playing) {
#if defined(DRIVER_OSS) || defined(DRIVER_SOLARIS_AUDIO)
      n = write (dev_dsp, pbuf, i*sbytes);
#elif defined(DRIVER_ALSA)
      n = snd_pcm_write (pcm_handle, pbuf, PBUF_SIZE/channels);
#endif
    }

    playoffset += (int)(n * relpitch / (sbytes * channels));
  }
}

static void
pva (sw_view * view)
{
  sw_sample * s = view->sample;

  setup_dev_dsp (s);

  play_view (view, 0, s->sounddata->nr_frames, 1.0);

  drain_dev_dsp ();

  stop_playmarker (s);

  WAIT_FOR_PLAYING;

  reset_dev_dsp ();
  close_dev_dsp ();
}

void
play_view_all (sw_view * view)
{
  sw_sample * s = view->sample;

  stop_playback ();

  playing = s;
  if (open_dev_dsp () >= 0) {
    pthread_create (&player_thread, NULL, (void *) (*pva), view);
    start_playmarker (s);
  }
}

void
pval (sw_view * view)
{
  sw_sample * s = view->sample;

  setup_dev_dsp (s);

  while (playing)
    play_view (view, 0, s->sounddata->nr_frames, 1.0);

  reset_dev_dsp ();
  close_dev_dsp ();
}

void
play_view_all_loop (sw_view * view)
{
  sw_sample * s = view->sample;

  stop_playback ();

  playing = s;
  if (open_dev_dsp () >= 0) {
    pthread_create (&player_thread, NULL, (void *) (*pval), view);
    start_playmarker (s);
  }
}

static void
pvs (sw_view * view)
{
  sw_sample * s = view->sample;
  GList * gl, * gl_next;
  sw_sel * sel;
  sw_framecount_t start, end;

  setup_dev_dsp (s);

  for (gl = s->sounddata->sels; gl; ) {

    /* Hold sels_mutex for as little time as possible */
    g_mutex_lock (s->sounddata->sels_mutex);

    /* if gl is no longer in sels, break out */
    if (g_list_position (s->sounddata->sels, gl) == -1) {
      g_mutex_unlock (s->sounddata->sels_mutex);
      break;
    }

    sel = (sw_sel *)gl->data;
    start = sel->sel_start;
    end = sel->sel_end;
    gl_next = gl->next;

    g_mutex_unlock (s->sounddata->sels_mutex);

    play_view (view, start, end, 1.0);

    gl = gl_next;
  }
  
  drain_dev_dsp ();

  stop_playmarker (s);

  WAIT_FOR_PLAYING; 

  reset_dev_dsp ();
  close_dev_dsp ();
}

void
play_view_sel (sw_view * view)
{
  sw_sample * s = view->sample;

  stop_playback ();

  playing = s;
  if (open_dev_dsp () >= 0) {
    pthread_create (&player_thread, NULL, (void *) (*pvs), view);
    start_playmarker (s);
  }
}

static void
pvsl (sw_view * view)
{
  sw_sample * s = view->sample;
  GList * gl, * gl_next;
  sw_sel * sel;
  sw_framecount_t start, end;

  setup_dev_dsp (s);

  while (playing) {

    for (gl = s->sounddata->sels; gl; ) {
      
      /* Hold sels_mutex for as little time as possible */
      g_mutex_lock (s->sounddata->sels_mutex);

      /* if gl is no longer in sels, break out */
      if (g_list_position (s->sounddata->sels, gl) == -1) {
	g_mutex_unlock (s->sounddata->sels_mutex);
	break;
      }

      sel = (sw_sel *)gl->data;
      start = sel->sel_start;
      end = sel->sel_end;
      gl_next = gl->next;
      
      g_mutex_unlock (s->sounddata->sels_mutex);
      
      play_view (view, start, end, 1.0);
      
      gl = gl_next;
    }
  }

  reset_dev_dsp ();

  close_dev_dsp ();
}

void
play_view_sel_loop (sw_view * view)
{
  sw_sample * s = view->sample;
  stop_playback ();

  playing = s;
  if (open_dev_dsp () >= 0) {
    pthread_create (&player_thread, NULL, (void *) (*pvsl), view);
    start_playmarker (s);
  }
}

typedef struct _pvap_data pvap_data;
struct _pvap_data {
  sw_view * view;
  gfloat pitch;
};

static void
pvap (pvap_data * p)
{
  sw_sample * s = p->view->sample;

  setup_dev_dsp (s);

  play_view (p->view, 0, s->sounddata->nr_frames, p->pitch);

  drain_dev_dsp ();

  stop_playmarker (s);

  WAIT_FOR_PLAYING;

  reset_dev_dsp ();
  close_dev_dsp ();

  g_free (p);
}

void
play_view_all_pitch (sw_view * view, gfloat pitch)
{
  sw_sample * s = view->sample;
  pvap_data * p;

  p = g_malloc (sizeof (*p));
  p->view = view;
  p->pitch = pitch;

  stop_playback ();

  playing = s;
  if (open_dev_dsp () >= 0) {
    pthread_create (&player_thread, NULL, (void *) (*pvap), p);
    start_playmarker (s);
  }
}

void
stop_playback (void)
{
  if (!playing) return;

  playing = NULL;

  flush_dev_dsp ();

  pthread_join (player_thread, NULL);
}

void
sample_stop_playback (sw_sample * sample)
{
  if (playing == sample) stop_playback();
}
