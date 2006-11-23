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
#include <gtk/gtk.h>

#include "cursors.h"

/* static cursor definitions */
#include "../pixmaps/horiz.xpm"
#include "../pixmaps/horiz_plus.xpm"
#include "../pixmaps/horiz_minus.xpm"

#include "../pixmaps/hand.xbm"
#include "../pixmaps/hand_mask.xbm"
#include "../pixmaps/needle.xbm"
#include "../pixmaps/needle_mask.xbm"
#include "../pixmaps/zoom_in.xbm"
#include "../pixmaps/zoom_in_mask.xbm"
#include "../pixmaps/zoom_out.xbm"
#include "../pixmaps/zoom_out_mask.xbm"

GdkCursor * sweep_cursors[SWEEP_CURSOR_MAX];


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

void
init_cursors (void)
{
  GdkBitmap * bitmap;
  GdkBitmap * mask;
  GdkColor white = {0, 0xffff, 0xffff, 0xffff};
  GdkColor black = {0, 0x0000, 0x0000, 0x0000};

  sweep_cursors[SWEEP_CURSOR_CROSSHAIR] = gdk_cursor_new (GDK_XTERM);
  /*  sweep_cursors[SWEEP_CURSOR_MOVE] = gdk_cursor_new (GDK_FLEUR);*/
  sweep_cursors[SWEEP_CURSOR_PENCIL] = gdk_cursor_new (GDK_PENCIL);
  sweep_cursors[SWEEP_CURSOR_NOISE] = gdk_cursor_new (GDK_SPRAYCAN);
  sweep_cursors[SWEEP_CURSOR_HAND] = gdk_cursor_new (GDK_HAND1);

  create_bitmap_and_mask_from_xpm (&bitmap, &mask, horiz_xpm);
  
  sweep_cursors[SWEEP_CURSOR_HORIZ] =
    gdk_cursor_new_from_pixmap (bitmap, mask, &white, &black, 8, 8);

  create_bitmap_and_mask_from_xpm (&bitmap, &mask, horiz_plus_xpm);
  
  sweep_cursors[SWEEP_CURSOR_HORIZ_PLUS] =
    gdk_cursor_new_from_pixmap (bitmap, mask, &white, &black, 8, 8);

  create_bitmap_and_mask_from_xpm (&bitmap, &mask, horiz_minus_xpm);
  
  sweep_cursors[SWEEP_CURSOR_HORIZ_MINUS] =
    gdk_cursor_new_from_pixmap (bitmap, mask, &white, &black, 8, 8);

  bitmap =
    gdk_bitmap_create_from_data (NULL, zoom_in_bits,
				 zoom_in_width, zoom_in_height);
  mask =
    gdk_bitmap_create_from_data (NULL, zoom_in_mask_bits,
				 zoom_in_mask_width, zoom_in_mask_height);

  sweep_cursors[SWEEP_CURSOR_ZOOM_IN] =
    gdk_cursor_new_from_pixmap (bitmap, mask, &white, &black,
				zoom_in_x_hot, zoom_in_y_hot);

  bitmap =
    gdk_bitmap_create_from_data (NULL, zoom_out_bits,
				 zoom_out_width, zoom_out_height);
  mask =
    gdk_bitmap_create_from_data (NULL, zoom_out_mask_bits,
				 zoom_out_mask_width, zoom_out_mask_height);

  sweep_cursors[SWEEP_CURSOR_ZOOM_OUT] =
    gdk_cursor_new_from_pixmap (bitmap, mask, &white, &black,
				zoom_out_x_hot, zoom_out_y_hot);

  bitmap =
    gdk_bitmap_create_from_data (NULL, (const gchar *) needle_bits,
				 needle_width, needle_height);
  mask =
    gdk_bitmap_create_from_data (NULL, (const gchar *) needle_mask_bits,
				 needle_mask_width, needle_mask_height);

  sweep_cursors[SWEEP_CURSOR_NEEDLE] =
    gdk_cursor_new_from_pixmap (bitmap, mask, &white, &black,
				needle_x_hot, needle_y_hot);


  bitmap =
    gdk_bitmap_create_from_data (NULL, (const gchar *) hand_bits,
				 hand_width, hand_height);
  mask =
    gdk_bitmap_create_from_data (NULL, (const gchar *) hand_mask_bits,
				 hand_mask_width, hand_mask_height);

  sweep_cursors[SWEEP_CURSOR_MOVE] =
    gdk_cursor_new_from_pixmap (bitmap, mask, &black, &white,
				hand_x_hot, hand_y_hot);


}
