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
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

#include <glib.h>
#include <gmodule.h>

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_version.h>
#include <sweep/sweep_types.h>

#include "sweep_compat.h"

GList * plugins = NULL;

static gint
cmp_proc_names (sw_procedure * a, sw_procedure * b)
{
  return strcmp (_(a->name), _(b->name));
}

void
sweep_plugin_init (const gchar * path)
{
  GModule * module;
  sw_plugin * m_plugin;
  GList * gl;

  module = g_module_open (path, G_MODULE_BIND_LAZY);

  if (!module) {
#ifdef DEBUG
    fprintf (stderr, "sweep_plugin_init: Error opening %s: %s\n",
	     path, g_module_error());
#endif
    return;
  }
  
  if (g_module_symbol (module, "plugin", (gpointer *)&m_plugin)) {
    for (gl = m_plugin->plugin_init (); gl; gl = gl->next) {
      plugins = g_list_insert_sorted (plugins, (sw_procedure *)gl->data,
				      (GCompareFunc)cmp_proc_names);
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

static void
init_dynamic_plugins_dir (gchar * dirname)
{
  DIR * dir;
  struct dirent * dirent;
  char * name, * path;
  struct stat statbuf;

  dir = opendir (dirname);
  if (!dir) {
    /* fail silently */
    return;
  }

  while ((dirent = readdir (dir)) != NULL) {
    name = dirent->d_name;
    path = g_module_build_path (PACKAGE_PLUGIN_DIR, dirent->d_name);
    if (stat (path, &statbuf) == -1) {
      /* system error -- non-fatal, ignore for plugin loading */
    } else if (sw_stat_regular (statbuf.st_mode)) {
      sweep_plugin_init (path);
    }
  }
}

/* Initialise dynamically linked plugins */
static void
init_dynamic_plugins (void)
{
  init_dynamic_plugins_dir (PACKAGE_PLUGIN_DIR);
}

void
init_plugins (void)
{
  init_static_plugins ();

  if (g_module_supported ()) {
    init_dynamic_plugins ();
  }


}
