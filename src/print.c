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
#include <string.h>
#include <math.h>

#include <sweep/sweep_types.h>

/*
 * Print a number of bytes to 3 significant figures
 * using standard abbreviations (GB, MB, kB, byte[s])
 */
int
snprint_bytes (gchar * s, gint n, glong nr_bytes)
{
  if (nr_bytes > (1L<<30)) {
    return snprintf (s, n, "%0.3f GB",
		     (gfloat)nr_bytes / (1024.0 * 1024.0 * 1024.0));
  } else if (nr_bytes > (1L<<20)) {
    return snprintf (s, n, "%0.3f MB",
		     (gfloat)nr_bytes / (1024.0 * 1024.0));
  } else if (nr_bytes > (1L<<10)) {
    return snprintf (s, n, "%0.3f kB",
		     (gfloat)nr_bytes / (1024.0));
  } else if (nr_bytes == 1) {
    return snprintf (s, n, "1 byte");
  } else {
    return snprintf (s, n, "%ld bytes", nr_bytes);
  }
}

/*
 * Print a time in the format HH:MM:SS.sss
 */
int
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

  /* XXX: %02.3f workaround */
  if (sec < 10.0) {
    return snprintf (s, n, "%s%02d:%02d:0%2.3f", sign, hrs, min, sec);
  } else {
    return snprintf (s, n, "%s%02d:%02d:%02.3f", sign, hrs, min, sec);
  }
}

/*
 * Print a time in SMPTE format
 */
int
snprint_time_smpte (gchar * s, gint n, sw_time_t time, gint F)
{
  double hrs, min, sec, N;
  double dtime = (double)time;

  /* round dtime to resolution */
  dtime = rint(dtime * (double)F) / (double)F;
  hrs = floor(dtime/3600.0);
  min = floor((dtime - (hrs * 3600.0)) / 60.0);
  sec = floor(dtime - (hrs * 3600.0) - (min * 60.0));
  N = rint((dtime - (hrs * 3600.0) - (min * 60.0) - sec) * (double)F);

  if (hrs > 0.0)
    return snprintf (s, n, "PT%.0fH%.0fM%.0fS%.0fN%dF", hrs, min, sec, N, F);
  else if (min > 0.0)
    return snprintf (s, n, "PT%.0fM%.0fS%.0fN%dF", min, sec, N, F);
  else if (sec > 0.0)
    return snprintf (s, n, "PT%.0fS%.0fN%dF", sec, N, F);
  else if (N > 0.0)
    return snprintf (s, n, "PT%.0fN%dF", N, F);
  else
    return snprintf (s, n, "P0S");
}

double
strtime_to_seconds (char * str)
{
  int h=0,m=0, n;
  float s;
  double result;

  n = sscanf (str, "%d:%d:%f",  &h, &m, &s);
  if (n == 3) {
    goto done;
  }

  n = sscanf (str, "%d:%f",  &m, &s);
  if (n == 2) {
    h = 0;
    goto done;
  }

  n = sscanf (str, "%f", &s);
  if (n == 1) {
    h = 0; m = 0;
    goto done;
  }
  
  return -1.0;

done:

  result = ((h * 3600.0) + (m * 60.0) + s);

  return result;
}
