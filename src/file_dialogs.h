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

#ifndef __FILE_DIALOGS_H__
#define __FILE_DIALOGS_H__

void
sample_load_cb(GtkWidget * wiget, gpointer data);

void
sample_revert_cb (GtkWidget * widget, gpointer data);

void
sample_save_as_cb(GtkWidget * wiget, gpointer data);

void
sample_save_cb(GtkWidget * wiget, gpointer data);

#endif /* __FILE_DIALOGS_H__ */
