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

#ifndef __FILE_OPS_H__
#define __FILE_OPS_H__

#include <sweep/sweep_types.h>

sw_sample *
sndfile_sample_reload (sw_sample * sample, gboolean try_raw);

sw_sample *
sndfile_sample_load (gchar * pathname, gboolean try_raw);

int
sndfile_sample_save(sw_sample * s, gchar * pathname);

int
sndfile_save_options_dialog (sw_sample * sample, gchar * pathname);

#endif /* __FILE_OPS_H__ */
