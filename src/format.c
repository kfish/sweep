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
 * Print a number of bytes to 3 significant figures
 * using standard abbreviations (GB, MB, kB, byte[s])
 */
void
snprint_bytes (gchar * s, gint n, glong nr_bytes)
{
  if (nr_bytes > (1L<<30)) {
    snprintf (s, n, "%0.3f GB",
	      (gfloat)nr_bytes / (1024.0 * 1024.0 * 1024.0));
  } else if (nr_bytes > (1L<<20)) {
    snprintf (s, n, "%0.3f MB",
	      (gfloat)nr_bytes / (1024.0 * 1024.0));
  } else if (nr_bytes > (1L<<10)) {
    snprintf (s, n, "%0.3f kB",
	      (gfloat)nr_bytes / (1024.0));
  } else if (nr_bytes == 1) {
    snprintf (s, n, "1 byte");
  } else {
    snprintf (s, n, "%ld bytes", nr_bytes);
  }
}

/*
 * Print a time in the format HH:MM:SS.sss
 */
void
snprint_time (gchar * s, gint n, sw_time_t time)
{
  int hrs, min;
  sw_time_t sec;
  char * sign;

  sign = (time < 0.0) ? "-" : "";

  if (time < 0.0) time = -time;

  hrs = (int) (time/3600.0);
  min = (int) ((time - ((sw_time_t)hrs * 3600.0)) / 60.0);
  sec = time - ((sw_time_t)hrs * 3600.0)- ((sw_time_t)min * 60.0);

  snprintf (s, n, "%s%d:%02d:%02.3f", sign, hrs, min, sec);
}


sw_format *
format_new (gint nr_channels, gint frame_rate)
{
  sw_format * format;

  format = g_malloc (sizeof(sw_format));
  format->channels = nr_channels;
  format->rate = frame_rate;

  return format;
}

sw_format *
format_copy (sw_format * f)
{
  sw_format * format;

  format = format_new (f->channels, f->rate);

  return format;
}

gint
format_equal (sw_format * f1, sw_format * f2)
{
  return (f1->channels == f2->channels &&
	  f1->rate == f2->rate);
}
