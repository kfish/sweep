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
#include <errno.h>

#include <sweep/sweep_types.h>
#include <sweep/sweep_sample.h>

#include "driver.h"
#include "question_dialogs.h"

#ifdef DRIVER_SOLARIS_AUDIO
#include <sys/audioio.h>
#include <stropts.h>
#include <sys/conf.h>
#define DEV_AUDIO "/dev/audio"

static sw_handle dev_audio_handle = {
 0, -1, 0, 0, NULL
};

static sw_handle *
open_dev_audio (int cueing, int flags)
{
  int dev_audio;
  sw_handle * handle = &dev_audio_handle;

  if (cueing) return NULL;

  if((dev_audio = open(DEV_AUDIO, O_WRONLY, 0)) == -1) {
    sweep_perror (errno, "Unable to open device " DEV_AUDIO);
    return NULL;
  }

  handle->driver_flags = flags;
  handle->driver_fd = dev_audio;

  return handle;
}

static void
setup_dev_audio (sw_handle * handle, sw_format * format)
{
  audio_info_t info;

  AUDIO_INITINFO(&info);
  info.play.precision = 16;	/* cs4231 doesn't handle 16-bit linear PCM */
  info.play.encoding = AUDIO_ENCODING_LINEAR;
  info.play.channels = format->channels;
  info.play.sample_rate = format->rate;
  if(ioctl(handle->driver_fd, AUDIO_SETINFO, &info) < 0)
    sweep_perror(errno, "Unable to configure audio device");

  handle->driver_channels = info.play.channels;
}

static ssize_t
write_dev_audio (sw_handle * handle, void * buf, size_t count)
{
  return write (handle->driver_fd, buf, count);
}

static void
reset_dev_audio (sw_handle * handle)
{
}

static void
flush_dev_audio (sw_handle * handle)
{
  if (ioctl(handle->driver_fd, I_FLUSH, FLUSHW) == -1)
    perror("I_FLUSH");
}

static void
drain_dev_audio (sw_handle * handle)
{
  if(ioctl(handle->driver_fd, AUDIO_DRAIN, 0) == -1)
      perror("AUDIO_DRAIN");
}

static void
close_dev_audio (sw_handle * handle)
{
  close (handle->driver_fd);
  handle->driver_fd = -1;
}

static sw_driver _driver_solaris = {
  "Solaris",
  NULL, /* config */
  open_dev_audio,
  setup_dev_audio,
  NULL,
  write_dev_audio,
  reset_dev_audio,
  flush_dev_audio,
  drain_dev_audio,
  close_dev_audio,
  "solaris_primary_device",
  "solaris_monitor_device",
  "solaris_log_frags"
};

#else

static sw_driver _driver_solaris = {
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

#endif

sw_driver * driver_solaris = &_driver_solaris;
