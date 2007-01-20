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

#ifndef __PCMIO_H__
#define __PCMIO_H__

#include <glib.h>

#define USE_MONITOR_KEY "UseMonitor"

#define DEFAULT_LOG_FRAGS 6
#define LOG_FRAGS_MIN 1
#define LOG_FRAGS_MAX 10

#define DEFAULT_USE_MONITOR FALSE

#define LOGFRAGS_TO_FRAGS(l) (1 << ((int)(floor((l)) - 1)))

char *
pcmio_get_main_dev (void);

char *
pcmio_get_monitor_dev (void);

gboolean
pcmio_get_use_monitor (void);

int
pcmio_get_log_frags (void);


#endif /* __PCMIO_H__ */
