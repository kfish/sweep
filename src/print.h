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

#ifndef __PRINT_H__
#define __PRINT_H__

#include <sweep/sweep_types.h>

/*
 * Print a number of bytes to 3 significant figures
 * using standard abbreviations (GB, MB, kB, byte[s])
 */
int
snprint_bytes (gchar * s, gint n, glong nr_bytes);

/*
 * Print a time in the format HH:MM:SS.sss
 */
int
snprint_time (gchar * s, gint n, sw_time_t time);

/*
 * Print a time in SMPTE format
 */
int
snprint_time_smpte (gchar * s, gint n, sw_time_t time, gint F);

/*
 * Parse a time format string (eg. 1:30:43.34) to a double
 */
double
strtime_to_seconds (char * str);

#endif /* __PRINT_H__ */
