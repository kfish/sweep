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
#include <gtk/gtk.h>

void
create_bitmap_and_mask_from_xpm (GdkBitmap ** bitmap,
				 GdkBitmap ** mask,
				 gchar ** xpm)
{
  int height, width, colors;
  char pixmap_buffer [(32 * 32)/8];
  char mask_buffer [(32 * 32)/8];
  int x, y, pix;
  int transparent_color, black_color;

  sscanf (xpm [0], "%d %d %d %d", &height, &width, &colors, &pix);

  g_assert (height == 32);
  g_assert (width == 32);
  g_assert (colors == 3);

  transparent_color = ' ';
  black_color = '.';

  for (y = 0; y < 32; y++) {
    for (x = 0; x < 32;) {
      char value = 0, maskv = 0;

      for (pix = 0; pix < 8; pix++, x++) {
	if (xpm [4+y][x] != transparent_color) {
	  maskv |= 1 << pix;

	  if (xpm [4+y][x] != black_color) {
	    value |= 1 << pix;
	  }
	}
      }

      pixmap_buffer [(y * 4 + x/8) - 1] = value;
      mask_buffer [(y * 4 + x/8) - 1] = maskv;
    }
  }

  *bitmap = gdk_bitmap_create_from_data (NULL, pixmap_buffer, 32, 32);
  *mask = gdk_bitmap_create_from_data (NULL, mask_buffer, 32, 32);
}
