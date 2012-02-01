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
#include <string.h>
#include <time.h>

#include <X11/Xlib.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <sweep/sweep_version.h>
#include <sweep/sweep_i18n.h>
#include <sweep/sweep_types.h>

#include "preferences.h"
#include "file_dialogs.h"
#include "interface.h"
#include "plugin.h"
#include "cursors.h"
#include "driver.h"
#include "callbacks.h"
#include "question_dialogs.h"
#include "play.h"

extern void sweep_timeouts_init (void);
extern gboolean ignore_failed_tdb_lock;
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
  gchar pathname [512];

  if (!strncmp (g_dirname (arg), ".", 1)) {
    snprintf (pathname, sizeof (pathname), "%s/%s", g_get_current_dir(), arg);
  } else {
    snprintf (pathname, sizeof (pathname), "%s", arg);
  }

  sample_load (pathname);

  return FALSE;
}

static gint
initial_sample_ask (gpointer data)
{
  question_dialog_new (NULL, _("Welcome to Sweep"),
		       _("Hello, my name is Scrubby. "
		         "Welcome to Sweep!\n\n"
		         "Would you like to create a new file or "
		         "load an existing file?"),
		       _("Create new file"), _("Load existing file"),
		       G_CALLBACK (sample_new_empty_cb), NULL, G_CALLBACK (sample_load_cb), NULL, 0);

  return FALSE;
}

/*
 * main
 */
int
main (int argc, char *argv[])
{
  int i;

  gboolean show_version = FALSE;
  gboolean show_help = FALSE;
  /*  gboolean show_toolbox = TRUE;*/

  gboolean no_files = TRUE;

#ifdef HAVE_PUTENV
  gchar *display_env;
#endif

#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif

  gtk_set_locale ();

#ifdef DEVEL_CODE
  g_print (_("WARNING: Build includes incomplete development code.\n"));
#endif

  XInitThreads ();

  gtk_init (&argc, &argv);

#ifdef HAVE_PUTENV
  display_env = g_strconcat ("DISPLAY=", gdk_get_display (), NULL);
  putenv (display_env);
#endif

  g_thread_init (NULL);



  /* must be done before g_idle_add / g_timeout_add */
  sweep_timeouts_init ();

  for(i = 1; i < argc; i++) {
    if ((strcmp (argv[i], "--help") == 0) ||
	(strcmp (argv[i], "-h") == 0)) {
      show_help = TRUE;
      argv[i] = NULL;
    } else if ((strcmp (argv[i], "--version") == 0) ||
	       (strcmp (argv[i], "-v") == 0)) {
      show_version = TRUE;
      argv[i] = NULL;
	} else if ((strcmp (argv[i], "--ignore-failed-lock") == 0)) {
      ignore_failed_tdb_lock = TRUE;
      argv[i] = NULL;

#ifdef DEVEL_CODE
    } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
      /* check for unknown options, but allow "-" as stdin */
#else
    } else if (argv[i][0] == '-') {
      /* check for unknown options */
#endif
      show_help = TRUE;
    } else {
      g_idle_add ((GSourceFunc) initial_sample_load, argv[i]);
      no_files = FALSE;
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
    g_print (_("  --ignore-failed-lock     Continue when attempt to lock the\n"
                      "                           preferences file fails.  For use when\n"
                      "                           the users home directory is on an NFS\n"
	                  "                           file system. (possibly unsafe) \n" ));

  }

  if (show_version || show_help) {
    exit (0);
  }

  srandom ((unsigned int)time(NULL));


  if (no_files) {
    g_idle_add ((GSourceFunc)initial_sample_ask, NULL);
  }

  /* initialise preferences */
  prefs_init ();

  /* initialise plugins */
  init_plugins ();

  /* initialise cursors */
  init_cursors ();

  /* initialise interface components */
  init_ui ();

  /* initialise devices */
  init_devices ();

  /* init playback subsystem */
  init_playback ();


  gtk_main ();


  /* close preferences database */
  prefs_close ();

  /* save key bindings */
  save_accels ();

  /* initialise plugins */
  release_plugins ();

  exit (0);
}
