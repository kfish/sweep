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

#include <gtk/gtk.h>

#include "callbacks.h"

#include "sweep_types.h"
#include "sweep_undo.h"
#include "sample.h"
#include "interface.h"
#include "edit.h"
#include "sample-display.h"
#include "driver.h"

/*
 * This _SHOULD_ be handled more nicely, and dynamically.
 *
 * But ... it isn't. Feel free to fix it :)
 */

#define PVA(n,p) \
void                                                                     \
play_view_all_##n##_cb (GtkWidget * widget, gpointer data)               \
{                                                                        \
  SampleDisplay * sd = SAMPLE_DISPLAY(data);                             \
  sw_view * v = sd->view;                                                \
                                                                         \
  play_view_all_pitch (v, ##p##);                                        \
}

PVA(C3,0.5);
PVA(Cs3,0.529732);
PVA(D3,0.561231);
PVA(Ds3,0.594604);
PVA(E3,0.629961);
PVA(F3,0.667420);
PVA(Fs3,0.707107);
PVA(G3,0.749154);
PVA(Gs3,0.793701);
PVA(A3,0.840896);
PVA(As3,0.890899);
PVA(B3,0.943874);

PVA(C4,1.0);

PVA(Cs4,1.059463);
PVA(D4,1.122462); 
PVA(Ds4,1.189207);
PVA(E4,1.259921);
PVA(F4,1.334840);
PVA(Fs4,1.414214);
PVA(G4,1.498307);
PVA(Gs4,1.587401);
PVA(A4,1.681793);
PVA(As4,1.781797);
PVA(B4,1.887749);
PVA(C5,2.0);
PVA(Cs5,2.118926);
PVA(D5,2.244924);
PVA(Ds5,2.378414);
PVA(E5,2.519842);
PVA(F5,2.669680);
PVA(Fs5,2.828427);
PVA(G5,2.996614);
PVA(Gs5,3.174802);
PVA(A5,3.363586);
PVA(As5,3.563595);
PVA(B5,3.775497);
