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
#include <glib.h>
#include <gtk/gtk.h>

#include <sweep/sweep_version.h>
#include <sweep/sweep_i18n.h>

#include "file_ops.h"
#include "interface.h"
#include "plugin.h"

/*
 * initial_sample_load ()
 *
 * a gtk_idle one-shot function.
 *
 * the initial loading of samples is deferred
 * to be processed by gtk_main, to allow the
 * toolbox window to be shown before having to
 * wait for potentially large samples to load.
 */
static gint
initial_sample_load (gpointer data)
{
  char * arg = (char *)data;
 
  sample_load_with_view (arg);

  return FALSE;
}


/*
 * main
 */
int
main (int argc, char *argv[])
{
  int i;
  GtkWidget *toolbox;

  gboolean show_version = FALSE;
  gboolean show_help = FALSE;
  gboolean show_toolbox = TRUE;

#ifdef HAVE_PUTENV
  gchar *display_env;
#endif

#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif

  gtk_set_locale ();
  gtk_init (&argc, &argv);

#ifdef HAVE_PUTENV
  display_env = g_strconcat ("DISPLAY=", gdk_get_display (), NULL);
  putenv (display_env);
#endif

  g_thread_init (NULL);

  for(i = 1; i < argc; i++) {
    if ((strcmp (argv[i], "--help") == 0) ||
	(strcmp (argv[i], "-h") == 0)) {
      show_help = TRUE;
      argv[i] = NULL;
    } else if ((strcmp (argv[i], "--version") == 0) ||
	       (strcmp (argv[i], "-v") == 0)) {
      show_version = TRUE;
      argv[i] = NULL;
    } else if (strcmp (argv[i], "--no-toolbox") == 0) {
      show_toolbox = FALSE;
      argv[i] = NULL;
    } else if (argv[i][0] == '-') {
      show_help = TRUE;
    } else {
      gtk_idle_add ((GtkFunction)initial_sample_load, argv[i]);
    }
  }

  if (show_version) {
    g_print ( "%s %s\n", _("Sweep version"), VERSION);
    g_print ( "%s %d.%d.%d\n",_("Sweep plugin API version"),
	      SWEEP_PLUGIN_API_MAJOR, SWEEP_PLUGIN_API_MINOR,
	      SWEEP_PLUGIN_API_REVISION);
  }

  if (show_help) {
    g_print (_("Usage: %s [option ...] [files ...]\n"), argv[0]);
    g_print (_("Valid options are:\n"));   
    g_print (_("  -h --help                Output this help.\n"));
    g_print (_("  -v --version             Output version info.\n"));
    g_print (_("  --display <display>      Use the designated X display.\n"));
    g_print (_("  --no-toolbox             Do not show the toolbox window.\n"));
  }

  if (show_version || show_help) {
    exit (0);
  }

  /* initialise plugins */
  init_plugins ();

  if (show_toolbox) {
    toolbox = create_toolbox ();
    gtk_widget_show (toolbox);
  }

  gtk_main ();
  return 0;
}

