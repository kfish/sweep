/*
 * Sweep PulseAudio Driver
 *
 * Copyright (C) 2008 Martin Szulecki
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
#include <errno.h>

#include <sweep/sweep_types.h>
#include <sweep/sweep_sample.h>

#include "driver.h"
#include "pcmio.h"
#include "question_dialogs.h"

#ifdef DRIVER_PULSEAUDIO
#include <pulse/simple.h>
#include <pulse/error.h>

static sw_handle handle_ro = {
 0, -1, 0, 0, NULL
};

static sw_handle handle_wo = {
 0, -1, 0, 0, NULL
};

static sw_handle handle_rw = {
 0, -1, 0, 0, NULL
};

static GList *
pulse_get_names (void)
{
  GList * names = NULL;

  names = g_list_append (names, "Default");

  return names;
}

static sw_handle *
pulse_open (int monitoring, int flags)
{
  sw_handle * handle = &handle_rw;

  if (flags == O_RDONLY) {
    handle = &handle_ro;
  } else if (flags == O_WRONLY) {
    handle = &handle_wo;
  }

  handle->driver_flags = flags;
  handle->custom_data = NULL;

  return handle;
}

static void
pulse_setup (sw_handle * handle, sw_format * format)
{
  struct pa_sample_spec ss;
  pa_stream_direction_t dir;
  int error;

  if (format->channels > PA_CHANNELS_MAX) {
    fprintf(stderr, __FILE__": pulse_setup(): The maximum number of channels supported is %d, while %d have been requested.\n", PA_CHANNELS_MAX, format->channels);
    return;
  }
  
  ss.format = PA_SAMPLE_FLOAT32;
  ss.rate = format->rate;
  ss.channels = format->channels;

  if (handle->driver_flags == O_RDONLY) {
    dir = PA_STREAM_RECORD;
  } else if (handle->driver_flags == O_WRONLY) {
    dir = PA_STREAM_PLAYBACK;
  } else {
    return;
  }

  if (!(handle->custom_data = pa_simple_new(NULL, "Sweep", dir, NULL, "Sweep Stream", &ss, NULL, NULL, &error))) {
    fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
    return;
  }

  handle->driver_rate = ss.rate;
  handle->driver_channels = ss.channels;
}

static int
pulse_wait (sw_handle * handle)
{
  return 0;
}

static ssize_t
pulse_read (sw_handle * handle, sw_audio_t * buf, size_t count)
{
  struct pa_simple * pa ;
  int error;
  size_t byte_count;

  if ((pa = (struct pa_simple *)handle->custom_data) == NULL)
    return 0;

  byte_count = count * sizeof (sw_audio_t);

  if (pa_simple_read(pa, buf, byte_count, &error) < 0) {
    fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
    return 0;
  }
  return 1;
}

static ssize_t
pulse_write (sw_handle * handle, sw_audio_t * buf, size_t count)
{
  struct pa_simple * pa ;
  int error;
  size_t byte_count;

  if ((pa = (struct pa_simple *)handle->custom_data) == NULL)
    return 0;

  byte_count = count * sizeof (sw_audio_t);

  if (pa_simple_write(pa, buf, byte_count, &error) < 0) {
    fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
    return 0;
  }

  return 1;
}

sw_framecount_t
pulse_offset (sw_handle * handle)
{
  return -1;
}

static void
pulse_reset (sw_handle * handle)
{
}

static void
pulse_flush (sw_handle * handle)
{
  struct pa_simple * pa ;
  int error;

  if ((pa = (struct pa_simple *)handle->custom_data) == NULL)
    return;

  if (pa_simple_flush(pa, &error) < 0) {
    fprintf(stderr, __FILE__": pa_simple_flush() failed: %s\n", pa_strerror(error));
  }
}

static void
pulse_drain (sw_handle * handle)
{
  struct pa_simple * pa ;
  int error;

  if ((pa = (struct pa_simple *)handle->custom_data) == NULL)
    return;

  if (pa_simple_drain(pa, &error) < 0) {
    fprintf(stderr, __FILE__": pa_simple_drain() failed: %s\n", pa_strerror(error));
  }
}

static void
pulse_close (sw_handle * handle)
{
  struct pa_simple * pa ;

  pulse_drain(handle);

  if ((pa = (struct pa_simple *)handle->custom_data) == NULL)
    return;

  pa_simple_free(pa);
  handle->custom_data = NULL;
}

static sw_driver _driver_pulseaudio = {
  "PulseAudio",
  pulse_get_names,
  pulse_open,
  pulse_setup,
  pulse_wait,
  pulse_read,
  pulse_write,
  pulse_offset,
  pulse_reset,
  pulse_flush,
  pulse_drain,
  pulse_close,
  "pulseaudio_primary_sink",
  "pulseaudio_monitor_sink",
  "pulseaudio_log_frags"
};

#else

static sw_driver _driver_pulseaudio = {
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

#endif

sw_driver * driver_pulseaudio = &_driver_pulseaudio;
