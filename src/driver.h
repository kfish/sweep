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

#ifndef __DRIVER_H__
#define __DRIVER_H__

#include <unistd.h>

#include "sweep_app.h"

#define PBUF_SIZE 256

typedef struct _sw_handle sw_handle;
typedef struct _sw_driver sw_driver;

struct _sw_handle {
  int driver_flags;
  int driver_fd;
  int driver_channels;
  int driver_rate;
  void * custom_data;
};

struct _sw_driver {
  const char * name;
  GList * (*get_names) (void);
  sw_handle * (*open) (int cueing, int flags);
  void (*setup) (sw_handle * handle, sw_format * format);
  int (*wait) (sw_handle * handle);
  ssize_t (*read) (sw_handle * handle, sw_audio_t * buf, size_t count);
  ssize_t (*write) (sw_handle * handle, sw_audio_t * buf, size_t count);
  sw_framecount_t (*offset) (sw_handle * handle);
  void (*reset) (sw_handle * handle);
  void (*flush) (sw_handle * handle);
  void (*drain) (sw_handle * handle);
  void (*close) (sw_handle * handle);
  
  char * primary_device_key;
  char * monitor_device_key;
  char * log_frags_key;
};

void
device_config (void);

sw_handle *
device_open (int cueing, int flags);

void
device_setup (sw_handle * handle, sw_format * format);

int
device_wait (sw_handle * handle);

/*
 * For recording, ie. reading pcm data from the device.
 */
ssize_t
device_read (sw_handle * handle, sw_audio_t * buf, size_t count);

ssize_t
device_write (sw_handle * handle, sw_audio_t * buf, size_t count);

/* As far as I'm aware the method
 * used to monitor latency in OSS and Solaris etc. is different to that which
 * ALSA uses, and different again from JACK and PortAudio.
 * 
 * Basically, when the device is written to, the driver is also told what the
 * offset into the file is for that bit of sound.
 * 
 * Then, when the GUI thread goes to draw the cursor, it asks the driver
 * what the offset of the sound that's currently coming out of the speaker is
 * and draws it there (which may be a little behind or ahead of where the
 * user is scrubbing) -- hence the sound and the vision are kept in sync.
 * 
 * However, this is all currently disabled, and going to change, as its not
 * properly monitoring the latency of multiple files being played
 * simultaneously; plus it may have to change with respect to ALSA and JACK and
 * PortAudio. So for now, just return -1.
 */
sw_framecount_t
device_offset (sw_handle * handle);

/*
 * Reset should stop the device immediately (ie. not bother emptying the
 * buffers, simply stop making any sound). The other half of what the RESET
 * ioctl does in OSS is put the device back into an initialised state where
 * it can accept new parameters, but sweep's not actually making use of that.
 */
void
device_reset (sw_handle * handle);

void
device_flush (sw_handle * handle);

/*
 * Drain empties the buffers out, ie. plays out any data that's been written
 * (otherwise you often lose the last bit of sound when closing the device).
 */
void
device_drain (sw_handle * handle);

void
device_close (sw_handle * handle);

void
init_devices (void);

void
stop_playback (sw_sample * s) ;

#endif /* __DRIVER_H__ */
