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

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "sweep_types.h"

/*
 * Determine the number of samples occupied by a number of frames
 * in a given format.
 */
glong
frames_to_samples (sw_format * format, sw_framecount_t nr_frames)
{
  return (nr_frames * (glong)format->channels);
}

/*
 * Determine the size in bytes of a number of frames of a given format.
 */
glong
frames_to_bytes (sw_format * format, sw_framecount_t nr_frames)
{
  return (nr_frames * (glong)format->channels * sizeof(sw_audio_t));
}

/*
 * Convert a number of frames to seconds
 */
sw_time_t
frames_to_time (sw_format * format, sw_framecount_t nr_frames)
{
  return ((gfloat)nr_frames / (gfloat)format->rate);
}

/*
 * Convert seconds to frames
 */
sw_framecount_t
time_to_frames (sw_format * format, sw_time_t time)
{
  return ((sw_framecount_t)((sw_time_t)format->rate / time));
}
