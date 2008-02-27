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

#ifndef __QUESTION_DIALOGS_H__
#define __QUESTION_DIALOGS_H__

#include <sweep/sweep_types.h>

void
question_dialog_new (sw_sample * sample, char * title, char * question,
		     char * yes_answer, char * no_answer,
         char * yes_stock_id, char * no_stock_id,
		     GCallback yes_callback, gpointer yes_callback_data,
		     GCallback no_callback, gpointer no_callback_data,
		     sw_edit_mode edit_mode);

void
info_dialog_new (char * title, gpointer xpm_data, const char * fmt, ...);

void
sweep_perror (int thread_errno, const char * fmt, ...);

#endif /* __QUESTION_DIALOGS_H__ */
