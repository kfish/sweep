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

#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include <glib.h>
#include <gmodule.h>

#include "sweep.h"


GList * plugins = NULL;

void
sweep_plugin_init (const gchar * name)
{
  gchar * path;
  GModule * module;
  sw_plugin * m_plugin;
  GList * gl;

  path = g_module_build_path (PACKAGE_PLUGIN_DIR, name);

  module = g_module_open (path, 0);
  
  if (g_module_symbol (module, "plugin", (gpointer *)&m_plugin)) {
    for (gl = m_plugin->plugin_init ();
	 gl; gl = gl->next) {
      plugins = g_list_append (plugins, (sw_proc *)gl->data);
    }
  }
}

/* Initialise statically linked plugins */
static void
init_static_plugins (void)
{
#if 0
  plugins = g_list_append (plugins, &proc_normalise);
  plugins = g_list_append (plugins, &proc_reverse);
#endif
}

/* 
 * Implementation of stripname for matching plugins of form
 * lib(.*).so
 *
 * If you want to implement support for other dynamic library
 * naming schemes, the correct thing to do is implement another
 * one of these functions. The g_module stuff deals with the
 * actual filenames portably.
 */
static char *
stripname (char * d_name)
{
  int len;

  if (strncmp (d_name, "lib", 3)) return 0;

  len = strlen (d_name);
  if (strncmp (&d_name[len-3], ".so", 3)) return 0;

  d_name[len-3] = '\0';

  return &d_name[3];
}

/* Initialise dynamically linked plugins */
static void
init_dynamic_plugins (void)
{
  DIR * dir;
  struct dirent * dirent;
  char * name;

  dir = opendir (PACKAGE_PLUGIN_DIR);
  if (!dir) {
#if 0
    switch (errno) {
    case EACESS:
      /* Permission denied */
    case ENFILE:
      /* Too many file descriptors in use by process. */
      /* Too many files are currently open in the system */
    case ENOENT:
      /* Directory  does  not  exist */
    case ENOMEM:
      /* Insufficient memory to complete the operation. */
    case ENOTDIR:
      /* not a directory */
    default:
    }
#endif 
    return;
  }

  while ((dirent = readdir (dir)) != NULL) {
    if ((name = stripname (dirent->d_name)) != NULL) {
      sweep_plugin_init (name);
    }
  }
}

void
init_plugins (void)
{
  init_static_plugins ();

  if (g_module_supported ()) {
    init_dynamic_plugins ();
  }
}
