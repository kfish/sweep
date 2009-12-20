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

/* Define this to record all output to files in /tmp */
/* #define RECORD_DEMO_FILES */

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

#ifdef RECORD_DEMO_FILES
#include <sndfile.h>

#if !defined (SNDFILE_1)
#error Recording demo files requires libsndfile version 1
#endif

#endif

#include <sweep/sweep_types.h>
#include <sweep/sweep_sample.h>
#include <sweep/sweep_typeconvert.h>

#include "play.h"
#include "head.h"
#include "driver.h"
#include "preferences.h"
#include "sample-display.h"

/*#define DEBUG*/

#define SCRUB_SLACKNESS 2.0

#define USE_MONITOR_KEY "UseMonitor"

static GMutex * play_mutex = NULL;

static sw_handle * main_handle = NULL;
static sw_handle * monitor_handle = NULL;

static GList * active_main_heads = NULL;
static GList * active_monitor_heads = NULL;

/*static int realoffset = 0;*/
static sw_sample * prev_sample = NULL;
static pthread_t player_thread = (pthread_t)-1;

static gboolean stop_all = FALSE;

static sw_audio_t * pbuf = NULL, * devbuf = NULL;
static int pbuf_chans = 0, devbuf_chans = 0;


/*
 * update_playmarker ()
 *
 * Update the position of the playback marker line for the sample
 * being played.
 *
 * Called within the main sweep interface thread.
 * gtk_idle will keep calling this function as long as this sample is
 * playing, unless otherwise stopped.
 */
static gint
update_playmarker (gpointer data)
{
  sw_sample * s = (sw_sample *)data;
  sw_head * head = s->play_head;

  if (!sample_bank_contains (s)) {
    return FALSE;
  } else if (!head->going) {

#ifdef DEBUG
    g_print ("update_playmarker: !head->going\n");
#endif
    s->playmarker_tag = 0;

    /* Set user offset to correct offset */
    if (head->previewing) {
      sample_set_playmarker (s, head->stop_offset, TRUE);
      head_set_previewing (head, FALSE);
    } else {
      sample_set_playmarker (s, (sw_framecount_t)head->offset, TRUE);
    }
    
    /* As this may have been stopped by the player thread, refresh the
     * interface */
    head_set_going (head, FALSE);
    sample_refresh_playmode (s);
    
    return FALSE;
  } else {
    sample_set_playmarker (s, head->realoffset, FALSE);

    return TRUE;
  }
}

static void
start_playmarker (sw_sample * s)
{
  if (s->playmarker_tag > 0) {
    g_source_remove (s->playmarker_tag);
  }

  s->playmarker_tag =
    g_timeout_add ((guint32)30,
		     (GSourceFunc) update_playmarker,
		     (gpointer)s);
}

#if 0
static struct timeval tv_instant = {0, 0};
#endif

static sw_framecount_t
head_read_unrestricted (sw_head * head, sw_audio_t * buf,
			sw_framecount_t count, int driver_rate)
{
  sw_sample * sample = head->sample;
  sw_sounddata * sounddata = sample->sounddata;
  sw_format * f = sounddata->format;
  sw_audio_t * d;
  gdouble po = 0.0, p;
  gfloat relpitch;
  sw_framecount_t i, j, b;
  gint si=0, si_next = 0;
  gboolean interpolate = FALSE;
  gboolean do_smoothing = FALSE;
  sw_framecount_t last_user_offset = -1;
  int pbuf_size = count * f->channels;
  gdouble scrub_rate = f->rate / 30.0;

  d = (sw_audio_t *)sounddata->data;

  b = 0;

  po = head->offset;

  /* compensate for sampling rate of driver */
  relpitch = (gfloat)((gdouble)f->rate / (gdouble)driver_rate);
  
  for (i = 0; i < count; i++) {
    if (head->mute || sample->user_offset == last_user_offset) {
      for (j = 0; j < f->channels; j++) {
	buf[b] = 0.0;
	b++;
      }
    } else {
      si = (int)floor(po);
      
      interpolate = (si < sounddata->nr_frames);
      
      p = po - (gdouble)si;
      si *= f->channels;
      g_mutex_lock(head->sample->sounddata->data_mutex);

      if (interpolate) {
	si_next = si+f->channels;
	for (j = 0; j < f->channels; j++) {
         buf[b] = head->gain * (((sw_audio_t *)head->sample->sounddata->data)[si] * p + ((sw_audio_t *)head->sample->sounddata->data)[si_next] * (1 - p));
	  if (do_smoothing) {
	    sw_framecount_t b1, b2;
	    b1 = (b - f->channels + pbuf_size) % pbuf_size;
	    b2 = (b1 - f->channels + pbuf_size) % pbuf_size;
	    buf[b] += buf[b] * 2.0;
	    buf[b] += buf[b1] * 3.0 + buf[b2] * 4.0;
	    buf[b] /= 10.0;
	  }
	  b++; si++; si_next++;
	}
      } else {
	for (j = 0; j < f->channels; j++) {
	 buf[b] = head->gain * ((sw_audio_t *)head->sample->sounddata->data)[si];
	  if (do_smoothing) {
	    sw_framecount_t b1, b2;
	    b1 = (b - f->channels + pbuf_size) % pbuf_size;
	    b2 = (b1 - f->channels + pbuf_size) % pbuf_size;
	    buf[b] += buf[b] * 2.0;
	    buf[b] += buf[b1] * 3.0 + buf[b2] * 4.0;
	    buf[b] /= 10.0;
	  }
	  b++; si++;
	}
      }
     g_mutex_unlock(head->sample->sounddata->data_mutex);
    }

    if (head->scrubbing) {
      gfloat new_delta;

      if (sample->by_user) {
	new_delta = (sample->user_offset - po) / scrub_rate;
	
	head->delta = head->delta * 0.9 + new_delta * 0.1;
	
	sample->by_user = FALSE;
	
	last_user_offset = sample->user_offset;
      } else  {
	gfloat new_po, u_po = (gdouble)sample->user_offset;

	new_delta = (sample->user_offset - po) / scrub_rate;
	
	head->delta = head->delta * 0.99 + new_delta * 0.01;
	
	new_po = po + (head->delta * relpitch);
	
	if ((head->delta < 0 && new_po < u_po) ||
	    (head->delta > 0 && new_po > u_po)) {
	  po = u_po;
	  head->delta = 0.0;
	}
      }
      
      do_smoothing = TRUE;

    } else {
      gfloat tdelta = head->rate * sample->rate;
      gfloat hdelta = head->delta * (head->reverse ? -1.0 : 1.0);

      if (sample->by_user) {
	head->offset = (gdouble)sample->user_offset;
	po = head->offset;
	head->delta = tdelta;

	sample->by_user = FALSE;
	do_smoothing = TRUE;

	last_user_offset = sample->user_offset;
      }

      if (hdelta < -0.3 * tdelta || hdelta > 1.001 * tdelta) {
	head->delta *= 0.9999;
      } else if (hdelta < 0.7 * tdelta) {
	head->delta = 0.8 * tdelta * (head->reverse ? -1.0 : 1.0);
      } else if (hdelta < .999 * tdelta) {
	head->delta *= 1.0001;
      } else {
	head->delta = tdelta * (head->reverse ? -1.0 : 1.0);
      }

      do_smoothing = FALSE;
    }

    po += head->delta * relpitch;

    {
      gdouble nr_frames = (gdouble)sample->sounddata->nr_frames;
      if (head->looping) {
	while (po < 0.0) po += nr_frames;
	while (po > nr_frames) po -= nr_frames;
      } else {
	if (po < 0.0) po = 0.0;
	else if (po > nr_frames) po = nr_frames;
      }
    }

    head->offset = po;

  }

  return count;
}

sw_framecount_t
head_read (sw_head * head, sw_audio_t * buf, sw_framecount_t count,
	   int driver_rate)
{
  sw_sample * sample = head->sample;
  sw_sounddata * sounddata = sample->sounddata;
  sw_format * f = sounddata->format;
  sw_framecount_t head_offset;
  sw_framecount_t remaining = count, written = 0, n = 0;
  sw_framecount_t delta, bound;
  GList * gl;
  sw_sel * sel, * osel;

  while (head->going && remaining > 0) {
    n = 0;

    if (head->restricted /* && !head->scrubbing */) {
      g_mutex_lock (sounddata->sels_mutex);

      if (g_list_length (sounddata->sels) == 0) {
	g_mutex_unlock (sounddata->sels_mutex);

	if (head->previewing && !head->scrubbing) {
	  if (head->reverse) {
	    n = MIN (remaining, head->offset - head->stop_offset);
	  } else {
	    n = MIN (remaining, head->stop_offset - head->offset);
	  }
	  goto got_n;

	} else {
	  goto zero_pad;
	}
      }

      if (head->previewing && !head->scrubbing) {
	/* Find selection region that offset is or should be playing to */
	if (head->reverse) {
	  osel = NULL;
	  for (gl = g_list_last (sounddata->sels); gl; gl = gl->prev) {
	    sel = (sw_sel *)gl->data;
	    
	    if (osel && ((sw_framecount_t)head->offset > osel->sel_start))
	      head->offset = osel->sel_start;

	    osel = sel;

	    head_offset = (sw_framecount_t)head->offset;
	    
	    if (head_offset > sel->sel_end) {
	      n = MIN (remaining, head_offset - sel->sel_end);
	      break;
	    }
	  }

	  /* If now at start of first selection region ... */
	  if (gl == NULL && osel != NULL) {
	    if ((sw_framecount_t)head->offset > osel->sel_start)
	      head->offset = osel->sel_start;

	    head_offset = (sw_framecount_t)head->offset;
	    
	    /* continue 1 second */
	    delta = time_to_frames (sounddata->format, 1.0);
	    bound = MAX((osel->sel_start - delta), 0);
	    if (head_offset > bound) {
	      n = MIN (remaining, head_offset - bound);
	    }
	  }

	} else {
	  osel = NULL;
	  for (gl = sounddata->sels; gl; gl = gl->next) {
	    sel = (sw_sel *)gl->data;
	    
	    if (osel && ((sw_framecount_t)head->offset < osel->sel_end))
	      head->offset = osel->sel_end;

	    osel = sel;
	    
	    head_offset = (sw_framecount_t)head->offset;

	    if (head_offset < sel->sel_start) {
	      n = MIN (remaining, sel->sel_start - head_offset);
	      break;
	    }
	  }
	  
	  /* If now at end of last selection region ... */
	  if (gl == NULL && osel != NULL) {
	    if ((sw_framecount_t)head->offset < osel->sel_end)
	      head->offset = osel->sel_end;

	    head_offset = head->offset;

	    /* continue 1 second */
	    delta = time_to_frames (sounddata->format, 1.0);
	    bound = MIN ((osel->sel_end + delta), sounddata->nr_frames);
	    if (head_offset < bound) {
	      n = MIN (remaining, bound - head_offset);
	    }
	  }
	}
      } else {
	/* Find selection region that offset is or should be in */
	if (head->reverse) {
	  for (gl = g_list_last (sounddata->sels); gl; gl = gl->prev) {
	    sel = (sw_sel *)gl->data;
	    
	    if ((sw_framecount_t)head->offset > sel->sel_end)
	      head->offset = sel->sel_end;
	    
	    if ((sw_framecount_t)head->offset > sel->sel_start) {
	      n = MIN (remaining, (sw_framecount_t)head->offset - sel->sel_start);
	      break;
	    }
	  }
	} else {
	  for (gl = sounddata->sels; gl; gl = gl->next) {
	    sel = (sw_sel *)gl->data;
	    
	    if ((sw_framecount_t)head->offset < sel->sel_start)
	      head->offset = sel->sel_start;
	    
	    if ((sw_framecount_t)head->offset < sel->sel_end) {
	      n = MIN (remaining, sel->sel_end - (sw_framecount_t)head->offset);
	      break;
	    }
	  }
	}
      }

      g_mutex_unlock (sounddata->sels_mutex);

    } else { /* unrestricted */
      if (head->previewing && !head->scrubbing) {
	if (head->reverse) {
	  n = MIN (remaining, (sw_framecount_t)head->offset - head->stop_offset);
	} else {
	  n = MIN (remaining, head->stop_offset - (sw_framecount_t)head->offset);
	}
      } else {
	if (head->reverse) {
	  n = MIN (remaining, (sw_framecount_t)head->offset);
	} else {
	  n = MIN (remaining, sounddata->nr_frames - (sw_framecount_t)head->offset);
	}
      }
    }

  got_n:

    if (n == 0) {
      if (head->previewing) {
	head->offset = head->stop_offset;
      } else if (!head->restricted || sounddata->sels == NULL) {
	head->offset = head->reverse ? sounddata->nr_frames : 0;
      } else {
	g_mutex_lock (sounddata->sels_mutex);
	if (head->reverse) {
	  gl = g_list_last (sounddata->sels);
	  sel = (sw_sel *)gl->data;
	  head->offset = sel->sel_end;
	} else {
	  gl = sounddata->sels;
	  sel = (sw_sel *)gl->data;
	  head->offset = sel->sel_start;
	}
	g_mutex_unlock (sounddata->sels_mutex);
      }

      if (!head->looping || head->previewing) {
	head->going = FALSE;
      }
    } else {
      if (n < 0) {
#ifdef DEBUG
	printf ("n = %d\n", n);
#endif
	head->going = FALSE;
#ifdef DEUBG
      } else if (n > count) {
	printf ("n = %d \t>\tcount = %d\n", n, count);
#endif
      } else {
      written += head_read_unrestricted (head, buf, n, driver_rate);
      buf += (int)frames_to_samples (f, n);
      remaining -= n;
      }
    }
  }

 zero_pad:

  if (remaining > 0) {
    n = frames_to_bytes (f, remaining);
    memset (buf, 0, n);
    written += remaining;
  }

  return written;
}

/* initialise a head for playback */
static void
head_init_playback (sw_sample * s)
{
  sw_head * head = s->play_head;
  GList * gl;
  sw_sel * sel;
  sw_framecount_t sels_start, sels_end;
  sw_framecount_t delta;

  /*  g_mutex_lock (s->play_mutex);*/

  s->by_user = FALSE;

  if (!head->going) {
    head_set_offset (head, s->user_offset);
    head->delta = head->rate * s->rate;
  }


  if (head->restricted) {
    g_mutex_lock (s->sounddata->sels_mutex);

    if ((gl = s->sounddata->sels) != NULL) {
      sel = (sw_sel *)gl->data;
      sels_start = sel->sel_start;
      
      gl = g_list_last (s->sounddata->sels);
      
      sel = (sw_sel *)gl->data;
      sels_end = sel->sel_end;
      g_mutex_unlock (s->sounddata->sels_mutex);
      
      if (head->previewing) {
	/* preroll 1 second */
	delta = time_to_frames (s->sounddata->format, 1.0);
	
	if (head->reverse) {
	  head_set_offset (head, sels_end + delta);
	} else {
	  head_set_offset (head, sels_start - delta);
	}
	
	head_set_offset (head,
			 CLAMP((sw_framecount_t)head->offset, 0, s->sounddata->nr_frames));
      } else {
	if (head->reverse && head->offset <= sels_start) {
	  head_set_offset (head, sels_end);
	} else if (!head->reverse && head->offset >= sels_end) {
	  head_set_offset (head, sels_start);
	}
      }
    } else if (head->previewing) {
	/* preroll 1 second */
	delta = time_to_frames (s->sounddata->format, 1.0);
	
	if (head->reverse) {
	  head_set_offset (head, head->offset + delta);
	} else {
	  head_set_offset (head, head->offset - delta);
	}
	
	head_set_offset (head,
			 CLAMP(head->offset, 0, s->sounddata->nr_frames));
    }
  } else {

    if (head->previewing) {
      /* preroll 1 second */
      delta = time_to_frames (s->sounddata->format, 1.0);
      if (head->reverse) head_set_offset (head, head->offset + delta);
      else head_set_offset (head, head->offset - delta);
    }

    if (head->reverse && head->offset <= 0) {
      head_set_offset (head,  s->sounddata->nr_frames);
    } else if (!head->reverse && head->offset >= s->sounddata->nr_frames) {
      head_set_offset (head, 0);
    }
  }

  /*  g_mutex_unlock (s->play_mutex);*/
}

static void
channel_convert_adding (sw_audio_t * src, int src_channels,
			sw_audio_t * dest, int dest_channels,
			sw_framecount_t n)
{
  int j;
  sw_framecount_t i, b = 0;
  sw_audio_intermediate_t a;

  if (src_channels == 1) { /* mix mono data up */
    for (i = 0; i < n; i++) {
      for (j = 0; j < dest_channels; j++) {
	dest[b] += src[i];
	b++;
      }
    }
  } else if (dest_channels == 1) { /* mix down to mono */
    for (i = 0; i < n; i++) {
      a = 0.0;
      for (j = 0; j < src_channels; j++) {
	a += src[b];
	b++;
      }
      a /= (sw_audio_intermediate_t)src_channels;
      dest[i] += (sw_audio_t)a;
    }
  } else if (src_channels < dest_channels) { /* copy to first channels */
    for (i = 0; i < n; i++) {
      for (j = 0; j < src_channels; j++) {
	dest[i * dest_channels + j] += src[b];
	b++;
      }
    }
  } else if (dest_channels < src_channels) { /* copy first channels only */
    for (i = 0; i < n; i++) {
      for (j = 0; j < dest_channels; j++) {
	dest[b] += src[i * src_channels + j];
	b++;
      }
    }
  } else if (src_channels == dest_channels) { /* just add */
    for (i = 0; i < n * src_channels; i ++) {
      dest[i] += src[i];
    }
  }
}

static void
play_head_update_device (sw_head * head)
{
  g_mutex_lock (play_mutex);
  g_mutex_lock (head->head_mutex);

  if (head->monitor) {
    if (g_list_find (active_monitor_heads, head) == 0) {
      active_monitor_heads = g_list_append (active_monitor_heads, head);
    }
    active_main_heads = g_list_remove (active_main_heads, head);
  } else {
    if (g_list_find (active_main_heads, head) == 0) {
      active_main_heads = g_list_append (active_main_heads, head);
    }
    active_monitor_heads = g_list_remove (active_monitor_heads, head);
  }

  g_mutex_unlock (head->head_mutex);
  g_mutex_unlock (play_mutex);
}

#ifdef RECORD_DEMO_FILES
static gchar *
generate_demo_filename (void)
{
  return g_strdup_printf ("/tmp/sweep-demo-%d.au", getpid ());
}
#endif

#define PSIZ 64

static void
prepare_to_play_heads (GList * heads, sw_handle * handle)
{
  sw_head * head;
  sw_format * f;

  GList * gl;

  if ((gl = heads) != NULL) {
    head = (sw_head *)gl->data;

    f = head->sample->sounddata->format;
    
    if (f->channels > pbuf_chans) {
      pbuf = g_realloc (pbuf, PSIZ * f->channels * sizeof (sw_audio_t));
      pbuf_chans = f->channels;
    }
  }

#if 0
  {
    fd_set fds;
    
    FD_ZERO (&fds);
    FD_SET (handle->driver_fd, &fds);
    
    if (select (handle->driver_fd+1, &fds, NULL, NULL, &tv_instant) == 0);
  }
#else
  device_wait (handle);
#endif

  return;
}

static void
play_heads (GList ** heads, sw_handle * handle)
{
  sw_sample * s;
  sw_head * head;
  sw_format * f;
  sw_framecount_t n;

  GList * gl, * gl_next;

  if (*heads == NULL) return;

  n = PSIZ;
  
  for (gl = *heads; gl; gl = gl_next) {

    g_mutex_lock (play_mutex);
    head = (sw_head *)gl->data;
    gl_next = gl->next;
    g_mutex_unlock (play_mutex);

    if (!head) {
      /* XXX: wtf??? */
      return;
    } else if (!head->going || !sample_bank_contains (head->sample)) {
      g_mutex_lock (play_mutex);
      *heads = g_list_remove (*heads, head);
      g_mutex_unlock (play_mutex);
    } else {
      s = head->sample;
      f = s->sounddata->format;
	  
      if (f->channels > pbuf_chans) {
	pbuf = g_realloc (pbuf, n * f->channels * sizeof (sw_audio_t));
	pbuf_chans = f->channels;
      }

      head_read (head, pbuf, n, handle->driver_rate);
	  
#if 0
      if (f->channels != handle->driver_channels) {
	channel_convert (pbuf, f->channels, devbuf,
			 handle->driver_channels, n);
      }
#else
      channel_convert_adding (pbuf, f->channels, devbuf,
			      handle->driver_channels, n);
#endif
	
      /* XXX: store the head->offset NOW for device_offset referencing */
	  
      g_mutex_lock (s->play_mutex);
	  
#if 0
      head->realoffset = head->offset;
#else
      head->realoffset = device_offset (handle);
      if (head->realoffset == -1) {
	head->realoffset = head->offset;
      }
#endif
	
      head->offset = head->realoffset;
	  
      if (s->by_user /* && s->play_scrubbing */) {
	/*head->offset = s->user_offset;*/
      } else {
	if (!head->scrubbing) s->user_offset = head->realoffset;
      }
	  
      /*	if (!head->going) active = FALSE;*/
	  
      g_mutex_unlock (s->play_mutex);
    }
  }

  return;
}

/* how many inactive writes to do before closing */
#define INACTIVE_TIMEOUT 256

static gboolean
monitor_active (void)
{
  int * use_monitor;

  use_monitor = prefs_get_int (USE_MONITOR_KEY);

  if (use_monitor == NULL) return 0;
  else return (*use_monitor != 0);
}

static void
play_active_heads (void)
{
  sw_framecount_t count;
  int inactive_writes = 0;
  GList * gl;
  sw_head * head;
  sw_format * f;
  int max_driver_chans = 0;

  gboolean use_monitor;

#ifdef RECORD_DEMO_FILES
  gchar * filename;
  SNDFILE * sndfile = NULL;
  SF_INFO sfinfo;
#endif

  if (!active_main_heads && !active_monitor_heads) return;

  use_monitor = (monitor_handle != NULL);

  if (use_monitor) {
    if ((gl = active_monitor_heads)) {
      head = (sw_head *)gl->data;
      f = head->sample->sounddata->format;
      
      device_setup (monitor_handle, f);
    } else if ((gl = active_main_heads)) {
      head = (sw_head *)gl->data;
      f = head->sample->sounddata->format;
      
      device_setup (monitor_handle, f);
    }
  }

  if ((gl = active_main_heads)) {
    head = (sw_head *)gl->data;
    f = head->sample->sounddata->format;

    device_setup (main_handle, f);

#ifdef RECORD_DEMO_FILES    
    filename = generate_demo_filename ();
    sfinfo.samplerate = f->rate;
    sfinfo.channels = handle->driver_channels;
    sfinfo.format = SF_FORMAT_AU | SF_FORMAT_FLOAT | SF_ENDIAN_CPU;
    sndfile = sf_open (filename, SFM_WRITE, &sfinfo);
    if (sndfile == NULL) sf_perror (NULL);
    else printf ("Writing to %s\n", filename);
#endif
  } else if ((gl = active_monitor_heads)) {
    head = (sw_head *)gl->data;
    f = head->sample->sounddata->format;

    device_setup (main_handle, f);
  } else {
    return;
  }

  if (use_monitor) {
    max_driver_chans = MAX (main_handle->driver_channels,
			    monitor_handle->driver_channels);
  } else {
    max_driver_chans = main_handle->driver_channels;
  }

  if (max_driver_chans > devbuf_chans) {
    devbuf = g_realloc (devbuf,
			PSIZ * max_driver_chans * sizeof (sw_audio_t));
    devbuf_chans = max_driver_chans;
  }

  while (!stop_all && inactive_writes < INACTIVE_TIMEOUT) {

    if (active_main_heads == NULL && active_monitor_heads == NULL) {
      inactive_writes++;
    } else {
      inactive_writes = 0;
    }

    g_mutex_lock (play_mutex);
    if (use_monitor) {
      prepare_to_play_heads (active_monitor_heads, monitor_handle);
      prepare_to_play_heads (active_main_heads, main_handle);
    } else {
      prepare_to_play_heads (active_monitor_heads, main_handle);
      prepare_to_play_heads (active_main_heads, main_handle);
    }
    g_mutex_unlock (play_mutex);

    if (use_monitor) {
      count = PSIZ * monitor_handle->driver_channels;
      memset (devbuf, 0, count * sizeof (sw_audio_t));
      play_heads (&active_monitor_heads, monitor_handle);
      device_write (monitor_handle, devbuf, count, -1 /* offset reference */);  
      
      count = PSIZ * main_handle->driver_channels;
      memset (devbuf, 0, count * sizeof (sw_audio_t));
      play_heads (&active_main_heads, main_handle);
      device_write (main_handle, devbuf, count, -1 /* offset reference */);
    } else {
      count = PSIZ * main_handle->driver_channels;
      memset (devbuf, 0, count * sizeof (sw_audio_t));
      play_heads (&active_monitor_heads, main_handle);
      play_heads (&active_main_heads, main_handle);
      device_write (main_handle, devbuf, count, -1 /* offset reference */);  
    }

#ifdef RECORD_DEMO_FILES
    if (sndfile)
      sf_writef_float (sndfile, devbuf, count / main_handle->driver_channels);
#endif
  }

#if 0
  if (!head->looping) {
    device_drain (handle);

    HEAD_SET_GOING (head, FALSE);
  }
#endif

  if (use_monitor) {
    device_reset (monitor_handle);
    device_close (monitor_handle);
  }

  device_reset (main_handle);
  device_close (main_handle);

#ifdef RECORD_DEMO_FILES
  if (sndfile) {
    printf ("Closing %s\n", filename);
    sf_close (sndfile);
  }
#endif

  player_thread = (pthread_t) -1;
}

static gboolean
ensure_playing (void)
{
  sw_handle * h;

  if (player_thread == (pthread_t) -1) {
    if ((h = device_open (0, O_WRONLY)) != NULL) {
      main_handle = h;
      if (monitor_active()) {
	monitor_handle = device_open (1, O_WRONLY);
      } else {
	monitor_handle = NULL;
      }

      pthread_create (&player_thread, NULL, (void *) (*play_active_heads),
		      NULL);
      return TRUE;
    } else {
      return FALSE;
    }
  }

  return TRUE;
}

void
sample_play (sw_sample * sample)
{
  sw_head * head = sample->play_head;

  play_head_update_device (head);
  head_init_playback (sample);

  head_set_going (head, TRUE);

  sample_refresh_playmode (sample);

  if (ensure_playing()) {
    start_playmarker (sample);
  } else {
    head_set_going (head, FALSE);
    sample_refresh_playmode (sample);
  }
}

void
sample_update_device (sw_sample * sample)
{
  sw_head * head = sample->play_head;

  if (!head->going) return;

  play_head_update_device (head);
}

void
play_view_all (sw_view * view)
{
  sw_sample * s = view->sample;
  sw_head * head = s->play_head;

  sample_set_stop_offset (s);
  sample_set_previewing (s, FALSE);
    
  prev_sample = s;

  head_set_restricted (head, FALSE);

  sample_play (s);
}

void
play_view_sel (sw_view * view)
{
  sw_sample * s = view->sample;
  sw_head * head = s->play_head;

  if (s->sounddata->sels == NULL) {
    head_set_going (head, FALSE);
    sample_refresh_playmode (s);
    return;
  }

  sample_set_stop_offset (s);
  sample_set_previewing (s, FALSE);

  prev_sample = s;

  head_set_restricted (head, TRUE);

  sample_play (s);
}

void
play_preroll (sw_view * view)
{
  sw_sample * s = view->sample;
  sw_head * head = s->play_head;

  sample_set_stop_offset (s);
  sample_set_previewing (s, TRUE);
  sample_set_scrubbing (s, FALSE);
    
  prev_sample = s;

  head_set_restricted (head, FALSE);

  sample_play (s);
}

void
play_preview_cut (sw_view * view)
{
  sw_sample * s = view->sample;
  sw_head * head = s->play_head;

  if (s->sounddata->sels == NULL) {
    head_set_going (head, FALSE);
    sample_refresh_playmode (s);
    return;
  }

  sample_set_stop_offset (s);
  sample_set_previewing (s, TRUE);
  sample_set_scrubbing (s, FALSE);
    
  prev_sample = s;

  head_set_restricted (head, TRUE);

  sample_play (s);
}

void
play_view_all_pitch (sw_view * view, gfloat pitch)
{
  sw_sample * s = view->sample;
  sw_head * head = s->play_head;
  sw_framecount_t mouse_offset;

  mouse_offset =
    sample_display_get_mouse_offset (SAMPLE_DISPLAY(view->display));
  sample_set_playmarker (s, mouse_offset, TRUE);

  sample_set_stop_offset (s);
  sample_set_previewing (s, FALSE);

  prev_sample = s;

  head_set_restricted (head, FALSE);
  head_set_rate (head, pitch);

  sample_play (s);
}

void
stop_all_playback (void)
{
  stop_all = TRUE;
  g_list_free (active_main_heads);
  active_main_heads = NULL;
}

void
pause_playback (sw_sample * s)
{
  sw_head * head;

  if (s == NULL) return;

  head = s->play_head;

  if (head->going) {
    head_set_going (head, FALSE);
  }

  sample_set_stop_offset (s);
}

void
stop_playback (sw_sample * s)
{
  sw_head * head;

  if (s == NULL) return;

  head = s->play_head;

  if (head->going) {
    head_set_going (head, FALSE);
    sample_set_playmarker (s, head->stop_offset, TRUE);

    g_mutex_lock (play_mutex);
    active_main_heads = g_list_remove (active_main_heads, head);
    g_mutex_unlock (play_mutex);
  }
}

gboolean
any_playing (void)
{
  return ((player_thread != (pthread_t) -1) && (active_main_heads != NULL));
}

void
init_playback (void)
{
  play_mutex = g_mutex_new ();
}
