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

#include <sweep/sweep_types.h>

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
