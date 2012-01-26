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

#ifndef __PREFERENCES_H__
#define __PREFERENCES_H__

void prefs_init ();

int prefs_close (void);

int prefs_delete (const char * key);

int prefs_get_int (const char * key, int default_value);

int prefs_set_int (const char * key, int val);

long prefs_get_long (const char * key, long default_value);

int prefs_set_long (const char * key, long val);

float prefs_get_float (const char * key, float default_value);

int prefs_set_float (const char * key, float val);

void prefs_get_string (const char * key, char * value, size_t maxlen, const char * default_value);

int prefs_set_string (const char * key, const char * val);

#endif /* __PREFERENCES_H__ */
