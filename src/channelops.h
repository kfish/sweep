/*
 * Sweep, a sound wave editor.
 *
 * Copyright (C) 2000 Conrad Parker
 * Copyright (C) 2002 CSIRO Australia
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

#ifndef __CHANNELOPS_H__
#define __CHANNELOPS_H__

void
dup_stereo_cb (GtkWidget * widget, gpointer data);

void
dup_channels_dialog_new_cb (GtkWidget * widget, gpointer data);

void
mono_mixdown_cb (GtkWidget * widget, gpointer data);

void
remove_left_cb (GtkWidget * widget, gpointer data);

void
remove_right_cb (GtkWidget * widget, gpointer data);

void
stereo_swap_cb (GtkWidget * widget, gpointer data);

void
channels_dialog_new_cb (GtkWidget * widget, gpointer data);

#endif /* __CHANNELOPS_H__ */
