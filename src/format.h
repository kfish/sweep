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

#ifndef __FORMAT_H__
#define __FORMAT_H__

#include <glib.h>

#include "sweep.h"

/*
 * Determine the number of samples occupied by a number of frames
 * in a given format.
 */
glong
frames_to_samples (sw_format * format, glong nr_frames);

/*
 * Determine the size in bytes of a number of frames of a given format.
 */
glong
frames_to_bytes (sw_format * format, glong nr_frames);

/*
 * Print a number of bytes to 3 significant figures
 * using standard abbreviations (GB, MB, kB, byte[s])
 */
void
snprint_bytes (gchar * s, gint n, glong nr_bytes);

/*
 * Convert a number of frames to seconds
 */
gfloat
frames_to_time (sw_format * format, glong nr_frames);

/*
 * Print a time in the format HH:MM:SS.sss
 */
void
snprint_time (gchar * s, gint n, gfloat time);

gint
format_equal (sw_format * f1, sw_format * f2);

sw_format *
format_new (gint nr_channels, gint sample_rate);

sw_format *
format_copy (sw_format * f);

#endif /* __FORMAT_H__ */

