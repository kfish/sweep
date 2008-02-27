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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <sndfile.h>

#include <sweep/sweep_types.h>
#include <sweep/sweep_sample.h>
#include <sweep/sweep_i18n.h>

#include "sweep_app.h"
#include "file_sndfile.h"
#include "sample.h"
#include "interface.h"
#include "sample-display.h"
#include "question_dialogs.h"
#include "preferences.h"

#define LAST_LOAD_KEY "Last_Load"
#define LAST_SAVE_KEY "Last_Save"

#ifdef HAVE_OGGVORBIS
extern sw_sample * vorbis_sample_reload (sw_sample * sample);
extern sw_sample * vorbis_sample_load (char * pathname);
extern int vorbis_save_options_dialog (sw_sample * sample, char * pathname);
#endif

#ifdef HAVE_SPEEX
extern sw_sample * speex_sample_reload (sw_sample * sample);
extern sw_sample * speex_sample_load (char * pathname);
extern int speex_save_options_dialog (sw_sample * sample, char * pathname);
#endif

#ifdef HAVE_MAD
extern sw_sample * mad_sample_reload (sw_sample * sample);
extern sw_sample * mad_sample_load (char * pathname);
#endif

void
mp3_unsupported_dialog (void)
{
  info_dialog_new (_("MP3 export unsupported"), NULL,
		   _("Export to MP3 format cannot legally be supported "
		     "in free software\n"
		     "due to patent licensing restrictions.\n\n"
		     "Please use Ogg Vorbis format instead, which\n"
		     "provides better quality and is free."));
}

/* This source code can compile against both version 0 and version 1
** of libsndfile.
*/

typedef struct {
  sw_file_format_t file_format;
  gchar * name;
  gchar * exts; /* comma separated extensions */
} sw_file_format_desc;

void
file_guess_method (sw_sample * sample, char * pathname);

#if ! (defined (SNDFILE_1))

static sw_file_format_desc file_formats [] = {
  {
    SWEEP_FILE_FORMAT_RAW,
    N_("Raw PCM (headerless)"),
    "raw",
  },
  {
    SWEEP_FILE_FORMAT_AIFF,
    "Apple/SGI AIFF,AIFF-C",
    "aiff,aifc"
  },
  {
    SWEEP_FILE_FORMAT_AU,
    "Sun/NeXT AU",
    "au",
  },
  {
    SWEEP_FILE_FORMAT_SVX,
    "Amiga IFF,SVX8,SVX16",
    "svx",
  },
  {
    SWEEP_FILE_FORMAT_PAF,
    "Ensoniq PARIS",
    "paf",
  },
  {
    SWEEP_FILE_FORMAT_IRCAM,
    "IRCAM/Berkeley/CARL SF",
    "sf",
  },
  {
    SWEEP_FILE_FORMAT_VOC,
    "Creative Labs VOC",
    "voc",
  },
  {
    SWEEP_FILE_FORMAT_WAV,
    "Microsoft WAV",
    "wav,riff",
  },
};

#endif

static gboolean
sweep_dir_exists (char * pathname)
{
  struct stat statbuf;

#undef BUF_LEN
#define BUF_LEN 512
  char buf[BUF_LEN];

  gchar * dirname;

  dirname = g_dirname (pathname);

  if (dirname && strcmp (dirname, "") && stat (dirname, &statbuf) == -1) {
    switch (errno) {
    case ENOENT:
      snprintf (buf, BUF_LEN, _("%s does not exist."), dirname);
      info_dialog_new (_("Directory does not exist"), NULL, buf);
      g_free (dirname);
      return FALSE;
      break;
    default:
      break;
    }
  }

  /* dirname is either NULL, "" or an existing directory */

  return TRUE;
}

#ifdef HAVE_MAD
static gboolean
file_guess_mp3 (char * pathname)
{
  gchar * ext;

  ext = strrchr (pathname, '.');

  if (ext != NULL) {
    ext++;
    if (!g_strncasecmp (ext, "mp3", 4)) return TRUE;
    if (!g_strncasecmp (ext, "mp2", 4)) return TRUE;
  }

  return FALSE;
}
#endif

sw_sample *
sample_reload (sw_sample * sample)
{
  sw_sample * new_sample = NULL;

#ifdef HAVE_OGGVORBIS
  new_sample = vorbis_sample_reload (sample);
#endif

  if (new_sample == NULL)
    new_sample = sndfile_sample_reload (sample, FALSE);

#ifdef HAVE_SPEEX
  if (new_sample == NULL)
    new_sample = speex_sample_reload (sample);
#endif

#ifdef HAVE_MAD
  if (new_sample == NULL && file_guess_mp3 (sample->pathname))
    new_sample = mad_sample_reload (sample);
#endif

  if (new_sample == NULL)
    new_sample = sndfile_sample_reload (sample, TRUE);

  return new_sample;
}

static sw_sample *
try_sample_load (char * pathname)
{
  sw_sample * sample = NULL;

#ifdef HAVE_OGGVORBIS
  sample = vorbis_sample_load (pathname);
#endif

  if (sample == NULL)
    sample = sndfile_sample_load (pathname, FALSE);

#ifdef HAVE_SPEEX
  if (sample == NULL)
    sample = speex_sample_load (pathname);
#endif

#ifdef HAVE_MAD
  if (sample == NULL && file_guess_mp3 (pathname))
    sample = mad_sample_load (pathname);
#endif

  if (sample == NULL)
    sample = sndfile_sample_load (pathname, TRUE);  
  
  if (sample != NULL)
        recent_manager_add_item (pathname);
        
  return sample;
}

sw_sample *
sample_load (char * pathname)
{
  if (sweep_dir_exists (pathname)) {
    if (strcmp (pathname, "-") == 0) {
      try_sample_load (pathname);
    } else if (access(pathname, R_OK) == -1) {
      switch (errno) {
      case ENOENT:
	create_sample_new_dialog_defaults (pathname);
	/*sweep_perror (errno, pathname);*/
	break;
      default:
	sweep_perror (errno, _("Unable to read\n%s"), pathname);
	break;
      }
    } else {
      prefs_set_string (LAST_LOAD_KEY, pathname);
      
      return try_sample_load (pathname);
    }
  }

  return NULL;
}

void
sample_load_cb(GtkWidget * widget, gpointer data)
{

  GtkWidget *dialog;
  gchar *load_current_file;
  gint win_width, win_height;
  GSList *filenames, *list;
 
  filenames = list = NULL;
    
  win_width = gdk_screen_width () / 2;
  win_height = gdk_screen_height () / 2;

  dialog = gtk_file_chooser_dialog_new (_("Sweep: Open Files"),
				      data,
				      GTK_FILE_CHOOSER_ACTION_OPEN,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      NULL);
    
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_widget_set_size_request (dialog, win_width, win_height);
  gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
  
  sweep_set_window_icon (GTK_WINDOW(dialog));
  attach_window_close_accel(GTK_WINDOW(dialog));
    
  load_current_file = prefs_get_string (LAST_LOAD_KEY);
  
  if (load_current_file) {
      gtk_file_chooser_set_filename (GTK_FILE_CHOOSER(dialog), load_current_file);
      
      g_free(load_current_file);
  }
    
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
      
   filenames = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER(dialog));      
   
   for (list = filenames; list; list = list->next) {

     if (list->data) {
       sample_load ((gchar *)list->data);
       g_free (list->data);
     }   
  }
      
  if (filenames)
    g_slist_free(filenames);
      
  } else {
    /* do nothing so exit */
     sample_bank_remove(NULL);
  }
    
  gtk_widget_destroy (dialog);
}

gboolean
sample_mtime_changed (sw_sample * sample)
{
  struct stat statbuf;

  /* a last_mtime of 0 indicates that the file is not from disk */
  if (sample->last_mtime == 0) return FALSE;

  if (stat (sample->pathname, &statbuf) == -1) {
    sweep_perror (errno, sample->pathname);
    return FALSE;
  }

  return (sample->last_mtime != statbuf.st_mtime);
}

void
sample_revert_ok_cb (GtkWidget * widget, gpointer data)
{
  sw_sample * sample = (sw_sample *)data;

  sample_reload (sample);
}

void
sample_revert_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample;
#undef BUF_LEN
#define BUF_LEN 512
  char buf[BUF_LEN];
    
  sample = view->sample;

  snprintf (buf, BUF_LEN,
	    _("Are you sure you want to revert %s to\n%s?\n\n"
	      "All changes and undo information will be lost."),
	    g_basename (sample->pathname), sample->pathname);

  question_dialog_new (sample, _("Revert file"), buf,
		       _("Revert"), _("Don't revert"),
          "gtk-ok", "gtk-cancel",
		       G_CALLBACK (sample_revert_ok_cb), sample, NULL, NULL,
		       SWEEP_EDIT_MODE_ALLOC);
}

void
sample_store_and_free_pathname (sw_sample * sample, gchar * pathname)
{
  prefs_set_string (LAST_SAVE_KEY, pathname);

  sample_set_pathname (sample, pathname);

#ifdef DEVEL_CODE
  g_free (pathname);
#endif
}

void
file_guess_method (sw_sample * sample, char * pathname)
{
  int i;
  gchar * ext;

#if defined (SNDFILE_1)
  SF_FORMAT_INFO  info ;
  int	count ;
#else
  gchar ** exts, **e;
  sw_file_format_desc * desc;
#endif

  g_assert (sample->file_method == SWEEP_FILE_METHOD_BY_EXTENSION);

  if (pathname == NULL) goto guess_raw;

  ext = strrchr (pathname, '.');

  if (ext != NULL) {
    ext++;

#ifdef HAVE_OGGVORBIS
    if (!g_strncasecmp (ext, "ogg", SW_DIR_LEN)) {
      sample->file_method = SWEEP_FILE_METHOD_OGGVORBIS;
      sample->file_format = 0;
      return;
    }
#endif

#ifdef HAVE_SPEEX
    if (!g_strncasecmp (ext, "spx", SW_DIR_LEN)) {
      sample->file_method = SWEEP_FILE_METHOD_SPEEX;
      sample->file_format = 0;
      return;
    }
#endif

    /* MP3 has dummy annoying dialog support */
    if (!g_strncasecmp (ext, "mp3", SW_DIR_LEN)) {
      sample->file_method = SWEEP_FILE_METHOD_MP3;
      sample->file_format = 0;
      return;
    }

#if defined (SNDFILE_1)

    sf_command (NULL, SFC_GET_FORMAT_MAJOR_COUNT, &count, sizeof (int)) ;

    for (i = 0; i <  count ; i++) {
      info.format = i ;
      sf_command (NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof (info)) ;
		
      if (!g_strncasecmp (ext, info.extension, SW_DIR_LEN)) {
	sample->file_method = SWEEP_FILE_METHOD_LIBSNDFILE;
	sample->file_format = info.format;
	break; /* NB. this break is essential to ensure that eg. a file
		* ending in ".wav" is guessed as RIFF/WAV not Sphere/NIST,
		* which also may use the ending ".wav"; assuming RIFF/WAV is
		* more common we prefer to use this format when guessing.
		*/
      }
    }

    /* Catch some common conventions not offered by libsndfile */
    if (sample->file_method == SWEEP_FILE_METHOD_BY_EXTENSION) {
      if (!g_strncasecmp(ext, "aifc", SW_DIR_LEN) ||
	  !g_strncasecmp(ext, "aif", SW_DIR_LEN)) {
	sample->file_method = SWEEP_FILE_METHOD_LIBSNDFILE;
	sample->file_format = SF_FORMAT_AIFF;
      }
    }

#else

    for (i = 0; i <  sizeof(file_formats)/sizeof(sw_file_format_desc); i++) {
      desc = &file_formats[i];
      exts = g_strsplit (desc->exts, ",", 16);
      for (e = exts; *e; e++) {
	if (!g_strncasecmp (*e, ext, SW_DIR_LEN)) {
	  sample->file_method = SWEEP_FILE_METHOD_LIBSNDFILE;
	  sample->file_format = desc->file_format;
	}
      }
      g_strfreev (exts);
    }

#endif

  }

 guess_raw:
  
  if (sample->file_method == SWEEP_FILE_METHOD_BY_EXTENSION) {
    sample->file_method = SWEEP_FILE_METHOD_LIBSNDFILE;
#if defined (SNDFILE_1)
    sample->file_format = SF_FORMAT_RAW;
#else
    sample->file_format = SWEEP_FILE_FORMAT_RAW;
#endif
  }

  return;
}    

typedef struct {
  sw_sample * sample;
  char * pathname;
} save_as_data;

static void
overwrite_ok_cb (GtkWidget * widget, gpointer data)
{
  save_as_data * sd = (save_as_data *)data;
  sw_sample * sample = sd->sample;
  char * pathname = sd->pathname;

  if (sample->file_method == SWEEP_FILE_METHOD_BY_EXTENSION) {
    file_guess_method (sample, pathname);
  }

  switch (sample->file_method) {
  case SWEEP_FILE_METHOD_LIBSNDFILE:
    sndfile_save_options_dialog (sample, pathname);
    break;
#ifdef HAVE_OGGVORBIS
  case SWEEP_FILE_METHOD_OGGVORBIS:
    vorbis_save_options_dialog (sample, pathname);
    break;
#endif
#ifdef HAVE_SPEEX
  case SWEEP_FILE_METHOD_SPEEX:
    speex_save_options_dialog (sample, pathname);
    break;
#endif
  case SWEEP_FILE_METHOD_MP3:
    mp3_unsupported_dialog ();
    break;
  default:
    g_assert_not_reached ();
    break;
  }

  g_free (sd);
}

static void
overwrite_cancel_cb (GtkWidget * widget, gpointer data)
{
  save_as_data * sd = (save_as_data *)data;
  gchar * msg;

  msg = g_strdup_printf (_("Save as %s cancelled"), g_basename (sd->pathname));
  sample_set_tmp_message (sd->sample, msg);
  g_free (msg);

  g_free (sd);
}

static void
file_set_format_cb (GtkWidget * widget, gpointer data)
{
  sw_sample * sample = (sw_sample *)data;

  sample->file_method = (sw_file_method_t)
     GPOINTER_TO_INT(g_object_get_data (G_OBJECT(widget), "method"));

  sample->file_format =
     GPOINTER_TO_INT(g_object_get_data (G_OBJECT(widget), "format"));
}

static GtkWidget *
create_save_menu (sw_sample * sample)
{
  GtkWidget * menu;
  GtkWidget * menuitem;
  int i;

#if defined (SNDFILE_1)
  SF_FORMAT_INFO  info ;
  int	count ;
#else
  sw_file_format_desc * desc;
#endif

  menu = gtk_menu_new ();

  sample->file_method = SWEEP_FILE_METHOD_BY_EXTENSION;

  menuitem = gtk_menu_item_new_with_label (_("By extension"));
  gtk_menu_append (GTK_MENU(menu), menuitem);
  gtk_object_set_data (GTK_OBJECT(menuitem), "method",
		       SWEEP_FILE_METHOD_BY_EXTENSION);
  gtk_object_set_data (GTK_OBJECT(menuitem), "format", 0);
  gtk_signal_connect (GTK_OBJECT(menuitem), "activate",
		      GTK_SIGNAL_FUNC(file_set_format_cb), sample);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new (); /* Separator */
  gtk_menu_append (GTK_MENU(menu), menuitem);
  gtk_widget_show (menuitem);

  /* libsndfile formats */

#if defined (SNDFILE_1)

  sf_command (NULL, SFC_GET_FORMAT_MAJOR_COUNT, &count, sizeof (int)) ;

  for (i = 0; i < count ; i++) {
    info.format = i ;
    sf_command (NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof (info)) ;

    menuitem = gtk_menu_item_new_with_label (info.name);
    gtk_menu_append (GTK_MENU(menu), menuitem);
    gtk_object_set_data (GTK_OBJECT(menuitem), "method",
			 GINT_TO_POINTER(SWEEP_FILE_METHOD_LIBSNDFILE));
    gtk_object_set_data (GTK_OBJECT(menuitem), "format",
			 GINT_TO_POINTER(info.format));
    gtk_signal_connect (GTK_OBJECT(menuitem), "activate",
			GTK_SIGNAL_FUNC(file_set_format_cb), sample);
    gtk_widget_show (menuitem);
  }

#else

  for (i = 0; i < sizeof(file_formats)/sizeof(sw_file_format_desc); i++) {
    desc = &file_formats[i];
    menuitem = gtk_menu_item_new_with_label (_(desc->name));
    gtk_menu_append (GTK_MENU(menu), menuitem);
    gtk_object_set_data (GTK_OBJECT(menuitem), "method",
			 GINT_TO_POINTER(SWEEP_FILE_METHOD_LIBSNDFILE));
    gtk_object_set_data (GTK_OBJECT(menuitem), "format",
			 GINT_TO_POINTER(desc->file_format));
    gtk_signal_connect (GTK_OBJECT(menuitem), "activate",
			GTK_SIGNAL_FUNC(file_set_format_cb), sample);
    gtk_widget_show (menuitem);
  }

#endif

  menuitem = gtk_menu_item_new (); /* Separator */
  gtk_menu_append (GTK_MENU(menu), menuitem);
  gtk_widget_show (menuitem);

#if 1
  /* MP3 (encoding not supported due to patent restrictions) */

  menuitem = gtk_menu_item_new_with_label (_("MP3 (Use Ogg Vorbis instead)"));
  gtk_widget_set_sensitive (menuitem, FALSE);
  gtk_menu_append (GTK_MENU(menu), menuitem);
  gtk_widget_show (menuitem);
#endif

#ifdef HAVE_OGGVORBIS
  /* Ogg Vorbis */

  menuitem = gtk_menu_item_new_with_label ("Ogg Vorbis (Xiph.org)");
  gtk_menu_append (GTK_MENU(menu), menuitem);
  gtk_object_set_data (GTK_OBJECT(menuitem), "method",
		       GINT_TO_POINTER(SWEEP_FILE_METHOD_OGGVORBIS));
  gtk_object_set_data (GTK_OBJECT(menuitem), "format",
		       GINT_TO_POINTER(0));
  gtk_signal_connect (GTK_OBJECT(menuitem), "activate",
		      GTK_SIGNAL_FUNC(file_set_format_cb), sample);
  gtk_widget_show (menuitem);
#endif

#ifdef HAVE_SPEEX
  /* Speex */

  menuitem = gtk_menu_item_new_with_label ("Speex (Xiph.org)");
  gtk_menu_append (GTK_MENU(menu), menuitem);
  gtk_object_set_data (GTK_OBJECT(menuitem), "method",
		       GINT_TO_POINTER(SWEEP_FILE_METHOD_SPEEX));
  gtk_object_set_data (GTK_OBJECT(menuitem), "format",
		       GINT_TO_POINTER(0));
  gtk_signal_connect (GTK_OBJECT(menuitem), "activate",
		      GTK_SIGNAL_FUNC(file_set_format_cb), sample);
  gtk_widget_show (menuitem);
#endif

  return menu;
}

void
sample_save_as_cb(GtkWidget * widget, gpointer data)
{
  GtkWidget *dialog;
  gint win_width, win_height;
  sw_view * view = (sw_view *)data;
  sw_sample * sample;
  GtkWidget * save_options;
  GtkWidget * frame;
  GtkWidget * hbox;
  GtkWidget * label;
  GtkWidget * option_menu;
  GtkWidget * save_menu;
  struct stat statbuf;
  gchar *filename;
  gint retval;

  save_as_data * sd;

#undef BUF_LEN
#define BUF_LEN 512
  char buf[BUF_LEN];

  char * last_save;

  win_width = gdk_screen_width () / 2;
  win_height = gdk_screen_height () / 2;
    
  sample = view->sample;

  dialog = gtk_file_chooser_dialog_new (_("Sweep: Save file"),
				      GTK_WINDOW(view->window),
				      GTK_FILE_CHOOSER_ACTION_SAVE,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				      NULL);
          
    
  //gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  attach_window_close_accel(GTK_WINDOW(dialog));
  sweep_set_window_icon (GTK_WINDOW(dialog));
    
  save_options = gtk_hbox_new (TRUE, 1);

  frame = gtk_frame_new (_("Save Options"));
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (save_options), frame, TRUE, TRUE, 4);
  
  hbox = gtk_hbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  gtk_widget_show (hbox);
  
  label = gtk_label_new (_("Determine File Type:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);
  
  option_menu = gtk_option_menu_new ();
  gtk_box_pack_start (GTK_BOX (hbox), option_menu, TRUE, TRUE, 0);
  gtk_widget_show (option_menu);
  
  save_menu = create_save_menu (sample);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), save_menu);
  
  gtk_widget_show (frame);

  /* pack the containing save_options hbox into the save-dialog */
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
		    save_options, FALSE, FALSE, 0);

  gtk_widget_show (save_options);
    
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_widget_set_size_request (dialog, win_width, win_height);
    
  if (strcmp (g_path_get_dirname(sample->pathname), ".") == 0) {

    last_save = prefs_get_string (LAST_SAVE_KEY);

    if (last_save != NULL) {
      gchar * last_save_dir = g_dirname (last_save);
            
	  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog),
				      last_save_dir);			      
      gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER(dialog),
				      sample->pathname);

      g_free (last_save_dir);
      g_free (last_save);

    } 
  } else {
     retval =  gtk_file_chooser_set_filename (GTK_FILE_CHOOSER(dialog), 
				    sample->pathname);
        /* FIXME: bug (local only?) causes gtk_file_chooser_set_filename
           to fail silently in some cases*/
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        //printf("filename pre: %s\n", filename);
        //printf("sample->pathname: %s\n", sample->pathname);

  }
 
  retval = gtk_dialog_run (GTK_DIALOG (dialog));
                           
  filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
  //printf("filename post: %s\n", filename);
  sample = view->sample;
  sd = g_malloc (sizeof (save_as_data));
  sd->sample = sample;
  sd->pathname = filename;

  if (retval == GTK_RESPONSE_ACCEPT) {
    
    if (!sweep_dir_exists (filename)) {
      g_free (sd);  
      g_free (filename);
      return;
    }

    if (stat (filename, &statbuf) == -1) {
      switch (errno) {
      case ENOENT:
        /* If it doesn't exist, it's ok to save as */
        overwrite_ok_cb (NULL, sd);
        break;
      default:
        sweep_perror (errno, filename);
        break;
      }
    } else {
      /* file exists */
        
      if (access(filename, W_OK) == -1) {
        sweep_perror (errno, _("You are not allowed to write to\n%s"), filename);
      } else {
        snprintf (buf, BUF_LEN, _("%s exists. Overwrite?"), filename);

        question_dialog_new (sample, _("File exists"), buf,
		  	   _("Overwrite"), _("Don't overwrite"),
          "gtk-ok", "gtk-cancel",
			     G_CALLBACK (overwrite_ok_cb), sd, G_CALLBACK (overwrite_cancel_cb), sd,
			     SWEEP_EDIT_MODE_META);
      }
    }
    /* FIXME: wrapped this due to the above gtk_file_chooser_set_filename problem */
    } else if (sd->pathname != NULL) {
      gchar * msg;

      msg = g_strdup_printf (_("Save as %s cancelled"), g_basename (sd->pathname));
      sample_set_tmp_message (sd->sample, msg);
      g_free (msg);
      
    } else {
  
    g_free (sd);
    g_free (filename);
    }
  gtk_widget_destroy (dialog);
    
    
}

static void
sample_save_ok_cb(GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;

  switch (sample->file_method) {
  case SWEEP_FILE_METHOD_LIBSNDFILE:
    sndfile_sample_save (sample, sample->pathname);
    break;
#ifdef HAVE_OGGVORBIS
  case SWEEP_FILE_METHOD_OGGVORBIS:
    vorbis_save_options_dialog (sample, sample->pathname);
    break;
#endif
#ifdef HAVE_SPEEX
  case SWEEP_FILE_METHOD_SPEEX:
    speex_save_options_dialog (sample, sample->pathname);
    break;
#endif
  case SWEEP_FILE_METHOD_MP3:
    mp3_unsupported_dialog ();
    break;
  default:
#if 0
    g_assert_not_reached ();
#else
    sample_save_as_cb (widget, data);
#endif
    break;
  }
}

void
sample_save_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample;
#undef BUF_LEN
#define BUF_LEN 512
  char buf[BUF_LEN];
    
  sample = view->sample;

  if (sample->last_mtime == 0 ||
      sample->file_method == SWEEP_FILE_METHOD_MP3) {
    sample_save_as_cb (widget, data);
  } else if (sample_mtime_changed (sample)) {
    snprintf (buf, BUF_LEN,
	      _("%s\n has changed on disk.\n\n"
		"Are you sure you want to save?"),
	      sample->pathname);
    
    question_dialog_new (sample, _("File modified"), buf,
			 _("Save"), _("Don't save"),
      "gtk-save", "gtk-cancel",
			  G_CALLBACK (sample_save_ok_cb), view, NULL, NULL,
			 SWEEP_EDIT_MODE_ALLOC);
  } else {
    sample_save_ok_cb (widget, view);
  }
}
