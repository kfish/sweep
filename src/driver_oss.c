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

#include <glib.h>
#include <gtk/gtk.h>

#include <sweep/sweep_types.h>
#include <sweep/sweep_sample.h>

#include "driver.h"
#include "question_dialogs.h"
#include "preferences.h"

#include "pcmio.h"

#ifdef DRIVER_OSS

#include <sys/soundcard.h>

#ifdef DEVEL_CODE
/*#define DEBUG*/
#endif

/* #define DEBUG_OFFSET */

typedef struct _oss_play_offset oss_play_offset;

struct _oss_play_offset {
  int framenr;
  sw_framecount_t offset;
};


static oss_play_offset offsets[1<<LOG_FRAGS_MAX];

static int nfrags = 0;


/*static char * configured_devicename = DEV_DSP;*/

static int oindex;
static int current_frame;
static int frame;

static sw_handle oss_handle = {
 0, -1, 0, 0, NULL
};

/* driver functions */

static GList *
oss_get_names (void)
{
  GList * names = NULL;

  names = g_list_append (names, "/dev/dsp");
  names = g_list_append (names, "/dev/dsp1");
  names = g_list_append (names, "/dev/sound/dsp");

  return names;
}

static sw_handle *
oss_open (int monitoring, int flags)
{
  char * dev_name;
  int dev_dsp;
  sw_handle * handle = &oss_handle;
  int i;

  if (monitoring) {
    if (pcmio_get_use_monitor())
      dev_name = pcmio_get_monitor_dev ();
    else
      return NULL;
  } else {
    dev_name = pcmio_get_main_dev ();
  }

  flags &= O_RDONLY|O_WRONLY|O_RDWR; /* mask out direction flags */

  if((dev_dsp = open(dev_name, flags, 0)) == -1) {
    sweep_perror (errno, "Unable to open device %s", dev_name);
    return NULL;
  }

  handle->driver_flags = flags;
  handle->driver_fd = dev_dsp;

  oindex = 0;
  current_frame = 0;
  for (i = 0; i < (LOGFRAGS_TO_FRAGS(LOG_FRAGS_MAX)); i++) {
    offsets[i].framenr = 0;
    offsets[i].offset = -1;
  }
  frame = 0;

  return handle;
}

static void
oss_setup (sw_handle * handle, sw_format * format)
{
  int dev_dsp;
  /*  int mask, format, stereo, frequency;*/

  int stereo = 0;
  int bits;
  int i, want_channels, channels;
  int srate;
  int error;
  int fragsize, frag;
  int fmt;

  if (handle == NULL) {
#ifdef DEBUG
    g_print ("handle NULL in setup()\n");
#endif
    return;
  }

  dev_dsp = handle->driver_fd;

  if (ioctl (dev_dsp, SNDCTL_DSP_STEREO, &stereo) == -1) {
    /* Fatal error */
    perror("open_dsp_device 2 ") ;
    exit (1);
  } ;

  if (ioctl (dev_dsp, SNDCTL_DSP_RESET, 0)) {
    perror ("open_dsp_device 3 ") ;
    exit (1) ;
  } ;

  nfrags = LOGFRAGS_TO_FRAGS(pcmio_get_log_frags());
  fragsize = 8;
  frag = (nfrags << 16) | fragsize;
  if ((error = ioctl (dev_dsp, SNDCTL_DSP_SETFRAGMENT, &frag)) != 0) {
    perror ("OSS: error setting fragments");
  }

  fragsize = (frag & 0xffff);
  nfrags = (frag & 0x7fff000)>>16;
#ifdef DEBUG
  g_print ("Got %d frags of size 2^%d\n", nfrags, fragsize);
#endif
    
  bits = 16 ;
  if ((error = ioctl (dev_dsp, SOUND_PCM_WRITE_BITS, &bits)) != 0) {
    perror ("open_dsp_device 4 ");
    exit (1);
  }

  for (i=1; i <= format->channels; i *= 2) {
    channels = format->channels / i;
    want_channels = channels;

    if ((error = ioctl (dev_dsp, SOUND_PCM_WRITE_CHANNELS, &channels)) == -1) {
      perror ("open_dsp_device 5 ") ;
      exit (1) ;
    }

    if (channels == want_channels) break;
  }

  handle->driver_channels = channels;

  srate = format->rate;

  if ((error = ioctl (dev_dsp, SOUND_PCM_WRITE_RATE, &srate)) != 0) {
    perror ("open_dsp_device 6 ") ;
    exit (1) ;
  }

  handle->driver_rate = srate;

  if ((error = ioctl (dev_dsp, SNDCTL_DSP_SYNC, 0)) != 0) {
    perror ("open_dsp_device 7 ") ;
    exit (1) ;
  }

  fmt = AFMT_QUERY;
  if ((error = ioctl (dev_dsp, SOUND_PCM_SETFMT, &fmt)) != 0) {
    perror ("open_dsp_device 8") ;
    exit (1) ;
  }

  handle->custom_data = GINT_TO_POINTER(0);

#ifdef WORDS_BIGENDIAN
  if (fmt == AFMT_S16_LE || fmt == AFMT_U16_LE) {
    handle->custom_data = GINT_TO_POINTER(1);
  }
#else
  if (fmt == AFMT_S16_BE || fmt == AFMT_U16_BE) {
    handle->custom_data = GINT_TO_POINTER(1);
  }
#endif

#ifdef DEBUG
  {
    int caps;

    if (ioctl (dev_dsp, SNDCTL_DSP_GETCAPS, &caps) == -1) {
      sweep_perror (errno, "OSS: Unable to get device capabilities");
    }
    /* CAP_REALTIME tells whether or not this device can give exact
     * DMA pointers via GETOSPACE/GETISPACE. If this is true, then
     * the device reports with byte accuracy. If it is false it reports
     * to at least the nearest fragment bound, which is still pretty
     * good for small fragments, so it's not much of a problem if
     * this capability is not present.
     */
    g_print ("Realtime: %s\n", caps & DSP_CAP_REALTIME ? "YES" : "NO");
  }
#endif
}

#define RECORD_SCALE (SW_AUDIO_T_MAX / 32768.0)

static int
oss_wait (sw_handle * handle)
{
  return 0;
}

static ssize_t
oss_read (sw_handle * handle, sw_audio_t * buf, size_t count)
{
  gint16 * bbuf;
  size_t byte_count;
  ssize_t bytes_read;
  int need_bswap;
  int i;

  byte_count = count * sizeof (gint16);
  bbuf = alloca (byte_count);
  bytes_read = read (handle->driver_fd, bbuf, byte_count);

  if (bytes_read == -1) {
    sweep_perror (errno, "Error reading from OSS audio device");
    return -1;
  }

  need_bswap = GPOINTER_TO_INT(handle->custom_data);

  if (need_bswap) {
    unsigned char * ucptr = (unsigned char *)bbuf;
    unsigned char temp;
    
    for (i = 0; i < count; i++) {
      temp = ucptr[2 * i];
      ucptr[2 * i] = ucptr [2 * i + 1];
      ucptr[2 * i + 1] = temp;
    }
  }

  for (i = 0; i < count; i++) {
    buf[i] = (sw_audio_t)(bbuf[i] * RECORD_SCALE);
  }

  return (bytes_read / sizeof (gint16));
}

#define PLAYBACK_SCALE (32768 / SW_AUDIO_T_MAX)

static ssize_t
oss_write (sw_handle * handle, sw_audio_t * buf, size_t count)
{
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

  current_frame += count;
  offsets[oindex].framenr = current_frame;
  offsets[oindex].offset = -1;
  oindex++; oindex %= nfrags;

  byte_count = count * sizeof (gint16);
  bbuf = alloca (byte_count);

  for (i = 0; i < count; i++) {
    bbuf[i] = (gint16)(PLAYBACK_SCALE * buf[i]);
  }

  need_bswap = GPOINTER_TO_INT(handle->custom_data);

  if (need_bswap) {
    unsigned char * ucptr = (unsigned char *)bbuf;
    unsigned char temp;
    
    for (i = 0; i < count; i++) {
      temp = ucptr[2 * i];
      ucptr[2 * i] = ucptr [2 * i + 1];
      ucptr[2 * i + 1] = temp;
    }
  }

  bytes_written = write (handle->driver_fd, bbuf, byte_count);

  if (bytes_written == -1) {
    sweep_perror (errno, "Error writing to OSS audio device");
    return -1;
  } else {
    return (bytes_written / sizeof(gint16));
  }
}

static sw_framecount_t
oss_offset (sw_handle * handle)
{
  count_info info;
  int i, o;

  if (handle == NULL) {
#ifdef DEBUG
    g_print ("handle NULL in offset()\n");
#endif
    return -1;
  }

  if (ioctl (handle->driver_fd, SNDCTL_DSP_GETOPTR, &info) == -1) {
#ifdef DEBUG_OFFSET
    g_print ("error in GETOPTR\n");
#endif
    return -1;
  }

  frame = info.bytes;
#ifdef DEBUG_OFFSET
  g_print ("frame: %d\n", frame);
#endif

  o = oindex+1;
  for (i = 0; i < nfrags; i++) {
    o %= nfrags;
#ifdef DEBUG_OFFSET
    g_print ("\t(%d) Checking %d: %d\n", frame, o, offsets[o].framenr);
#endif
    if (offsets[o].framenr >= frame) {
      return offsets[o].offset;
    }
    o++;
  }

  return -1;
}

static void
oss_reset (sw_handle * handle)
{
  if (handle == NULL) {
#ifdef DEBUG
    g_print ("handle NULL in reset()\n");
#endif
    return;
  }

  if(ioctl (handle->driver_fd, SNDCTL_DSP_RESET, 0) == -1) {
    sweep_perror (errno, "Error resetting OSS audio device");
  }
}

static void
oss_flush (sw_handle * handle)
{
}

void
oss_drain (sw_handle * handle)
{
  if (handle == NULL) {
    g_print ("handle NULL in drain ()\n");
    return;
  }

  if(ioctl (handle->driver_fd, SNDCTL_DSP_POST, 0) == -1) {
    sweep_perror (errno, "POST error on OSS audio device");
  }

  if (ioctl (handle->driver_fd, SNDCTL_DSP_SYNC, 0) == -1) {
    sweep_perror (errno, "SYNC error on OSS audio device");
  }
}

static void
oss_close (sw_handle * handle)
{
  close (handle->driver_fd);
  handle->driver_fd = -1;
}

static sw_driver _driver_oss = {
  "OSS",
  oss_get_names,
  oss_open,
  oss_setup,
  oss_wait,
  oss_read,
  oss_write,
  oss_offset,
  oss_reset,
  oss_flush,
  oss_drain,
  oss_close,
  "oss_primary_device",
  "oss_monitor_device",
  "oss_log_frags"
};

#else

static sw_driver _driver_oss = {
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

#endif

sw_driver * driver_oss = &_driver_oss;
