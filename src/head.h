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

#ifndef __HEAD_H__
#define __HEAD_H__

#include <gtk/gtk.h>

#include "sweep_app.h"

#define HEAD_LOCK(h,e) \
  g_mutex_lock ((h)->head_mutex);    \
  (e);                               \
  g_mutex_unlock ((h)->head_mutex);

#define HEAD_SET_GOING(h,s) HEAD_LOCK((h), (h)->going = (s))

void
head_controller_set_head (sw_head_controller * hctl, sw_head * head);

GtkWidget *
head_controller_create (sw_head * head, GtkWidget * window,
			sw_head_controller ** hctl_ret);

sw_head *
head_new (sw_sample * sample, sw_head_t head_type);

void
head_set_scrubbing (sw_head * h, gboolean scrubbing);

void
head_set_previewing (sw_head * h, gboolean previewing);

void
head_set_looping (sw_head * h, gboolean looping);

void
head_set_reverse (sw_head * h, gboolean reverse);

void
head_set_mute (sw_head * h, gboolean mute);

void
head_set_going (sw_head * h, gboolean going);

void
head_set_restricted (sw_head * h, gboolean restricted);

void
head_set_stop_offset (sw_head * h, sw_framecount_t offset);

void
head_set_gain (sw_head * h, gfloat gain);

void
head_set_rate (sw_head * h, gfloat rate);

void
head_set_offset (sw_head * h, sw_framecount_t offset);

void
head_set_monitor (sw_head * h, gboolean monitor);

sw_framecount_t
head_read (sw_head * head, sw_audio_t * buf, sw_framecount_t count,
	   int driver_rate);

sw_framecount_t
head_write (sw_head * head, sw_audio_t * buf, sw_framecount_t count);

#endif /* __HEAD_H__ */
