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

#ifndef __NOTES_H___
#define __NOTES_H__

/*
 * This _SHOULD_ be handled more nicely, and dynamically.
 *
 * But ... it isn't. Feel free to fix it :)
 */

#define PVAh(n) \
void                                                                     \
play_view_all_##n##_cb (GtkWidget * widget, gpointer data)

PVAh(C3);
PVAh(Cs3);
PVAh(D3);
PVAh(Ds3);
PVAh(E3);
PVAh(F3);
PVAh(Fs3);
PVAh(G3);
PVAh(Gs3);
PVAh(A3);
PVAh(As3);
PVAh(B3);

PVAh(C4);

PVAh(Cs4);
PVAh(D4);
PVAh(Ds4);
PVAh(E4);
PVAh(F4);
PVAh(Fs4);
PVAh(G4);
PVAh(Gs4);
PVAh(A4);
PVAh(As4);
PVAh(B4);
PVAh(C5);
PVAh(Cs5);
PVAh(D5);
PVAh(Ds5);
PVAh(E5);
PVAh(F5);
PVAh(Fs5);
PVAh(G5);
PVAh(Gs5);
PVAh(A5);
PVAh(As5);
PVAh(B5);

#endif /* __NOTES_H__ */
