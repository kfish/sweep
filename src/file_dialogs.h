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

#include <sweep/sweep_types.h>

void
mp3_unsupported_dialog (void);

sw_file_format_t
guess_file_format (gchar * pathname);

gboolean
sample_mtime_changed (sw_sample * sample);

sw_sample *
sample_load(char * pathname);

void
sample_load_cb(GtkWidget * wiget, gpointer data);

void
sample_revert_ok_cb (GtkWidget * widget, gpointer data);

void
sample_revert_cb (GtkWidget * widget, gpointer data);

void
sample_save_as_cb(GtkWidget * wiget, gpointer data);

void
sample_save_cb(GtkWidget * wiget, gpointer data);

void
sample_store_and_free_pathname (sw_sample * sample, gchar * pathname);

#endif /* __FILE_DIALOGS_H__ */
