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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "sweep_types.h"
#include "driver.h"
#include "sample.h"

#ifdef DRIVER_OSS
#include <sys/soundcard.h>
#define DEV_DSP "/dev/dsp"
#endif

#ifdef DRIVER_SOLARIS_AUDIO
#include <sys/audioio.h>
#define DEV_DSP "/dev/audio"
#endif


#define PLAYBACK_SCALE (32768 / SW_AUDIO_T_MAX)

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
    gtk_idle_add ((GtkFunction)update_playmarker, s);
}

static void
stop_playmarker (void)
{
  if (playing->playmarker_tag > 0)
    gtk_idle_remove (playing->playmarker_tag);
  playing->playmarker_tag = 0;
  sample_set_playmarker (playing, -1);
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
#else
  fprintf(stderr, "Warning: No audio device configured\n");
  return -1;
#endif
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

#if 0
  if (s->sdata->format->s_size == 8) {
    if(mask&AFMT_U8) {
      format=AFMT_U8;
    }
    if(mask&AFMT_S8) {
      format=AFMT_S8;
    }
  }

  if (s->sdata->format->s_size == 16) {
#endif
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

#if 0
  }
#endif

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
  info.play.channels = s->sounddata->format->f_channels;
  info.play.sample_rate = s->sounddata->format->f_rate;
  if(ioctl(dev_dsp, AUDIO_SETINFO, &info) < 0)
      perror("Unable to configure audio device");

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
#ifdef DRIVER_OSS
  if(ioctl (dev_dsp, SNDCTL_DSP_POST) == -1) {
    perror ("OSS: POST error on " DEV_DSP);
  }
#endif

#ifdef DRIVER_SOLARIS_AUDIO
  if(ioctl(dev_dsp, AUDIO_DRAIN, 0) == -1)
      perror("AUDIO_DRAIN");
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
  close (dev_dsp);
  dev_dsp = -1;
}

static void
play_view(sw_view * view, sw_framecount_t start, sw_framecount_t end, gfloat relpitch)
{
  sw_sample * s = view->sample;
  fd_set fds;
  ssize_t n;
  sw_audio_t * d;
#define PBUF_SIZE 256
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

    n = write (dev_dsp, pbuf, i*sbytes);
    
    playoffset += (int)(n * relpitch / (sbytes * channels));
  }
}

static void
pva (sw_view * view)
{
  sw_sample * s = view->sample;

  setup_dev_dsp (s);

  play_view (view, 0, s->sounddata->nr_frames, 1.0);

  flush_dev_dsp ();

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
  
  flush_dev_dsp ();

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

  flush_dev_dsp ();

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

  /* Do this explicitly */
  stop_playmarker ();

  playing = NULL;

  pthread_join (player_thread, NULL);
}

void
sample_stop_playback (sw_sample * sample)
{
  if (playing == sample) stop_playback();
}
