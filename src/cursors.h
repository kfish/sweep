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

#ifndef __CURSORS_H__
#define __CURSORS_H__

enum {
  SWEEP_CURSOR_CROSSHAIR,
  SWEEP_CURSOR_MOVE,
  SWEEP_CURSOR_HORIZ,
  SWEEP_CURSOR_HORIZ_PLUS,
  SWEEP_CURSOR_HORIZ_MINUS,
  SWEEP_CURSOR_ZOOM_IN,
  SWEEP_CURSOR_ZOOM_OUT,
  SWEEP_CURSOR_NEEDLE,
  SWEEP_CURSOR_PENCIL,
  SWEEP_CURSOR_NOISE,
  SWEEP_CURSOR_HAND_OPEN,
  SWEEP_CURSOR_HAND_CLOSE,
  SWEEP_CURSOR_MAX
};

void
create_bitmap_and_mask_from_xpm (GdkBitmap ** bitmap,
				 GdkBitmap ** mask,
				 gchar ** xpm);

void
init_cursors (void);

#endif
