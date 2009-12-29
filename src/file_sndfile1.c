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
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include <pthread.h>

#include <sndfile.h>

#define BUFFER_LEN 1024

#include <glib.h>
#include <gtk/gtk.h>

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_types.h>
#include <sweep/sweep_typeconvert.h>
#include <sweep/sweep_sample.h>
#include <sweep/sweep_undo.h>
#include <sweep/sweep_sounddata.h>

#include "sample.h"
#include "file_dialogs.h"
#include "file_sndfile.h"
#include "interface.h"
#include "question_dialogs.h"
#include "sw_chooser.h"
#include "view.h"

extern GtkStyle * style_wb;

#if defined (SNDFILE_1)

#include "../pixmaps/libsndfile.xpm"

typedef struct {
  int subformat;
  gboolean cap_mono;
  gboolean cap_stereo;
} sndfile_subformat_caps;

typedef struct {
  gboolean saving; /* loading or saving ? */
  sw_sample * sample;
  gchar * pathname;
  SF_INFO * sfinfo;
  GtkWidget * ok_button;
} sndfile_save_options;

static void
sweep_sndfile_perror (SNDFILE * sndfile, gchar * pathname)
{
  char buf[128];

  sf_error_str (sndfile, buf, sizeof (buf));

  sweep_perror (errno, "libsndfile: %s\n\n%s", buf,
		(pathname == NULL) ? "" : pathname);
}

static void
sndfile_save_options_dialog_ok_cb (GtkWidget * widget, gpointer data)
{
  sndfile_save_options * so = (sndfile_save_options *)data;
  sw_sample * sample = so->sample;
  GtkWidget * dialog;

  dialog = gtk_widget_get_toplevel (widget);
  gtk_widget_destroy (dialog);

  if (sample->file_info) {
    g_free (sample->file_info);
  }

  sample->file_info = so->sfinfo;

  sndfile_sample_save (sample, so->pathname);

  g_free (so);
}

static void
sndfile_save_options_dialog_cancel_cb (GtkWidget * widget, gpointer data)
{
  sndfile_save_options * so = (sndfile_save_options *)data;
  GtkWidget * dialog;

  dialog = gtk_widget_get_toplevel (widget);
  gtk_widget_destroy (dialog);

  g_free (so);

  /* if the sample bank is empty, quit the program */
  sample_bank_remove (NULL);
}

static void
update_ok_button (sndfile_save_options * so)
{
  g_return_if_fail (so != NULL);

  gtk_widget_set_sensitive (so->ok_button,
			    so->sfinfo->samplerate > 0 &&
			    sf_format_check (so->sfinfo));
}

static void
update_save_options_caps (sndfile_save_options * so)
{
  update_ok_button (so);
}

static void
update_save_options_values (sndfile_save_options * so)
{
  sw_sample * sample;
  sw_format * format;
  SF_INFO * sfinfo;

  sample = so->sample;
  format = sample->sounddata->format;

  sfinfo = so->sfinfo;

  update_ok_button (so);
}

static void
set_subformat_cb (GtkWidget * widget, gpointer data)
{
  sndfile_save_options * so = (sndfile_save_options *)data;
  SF_INFO * sfinfo = so->sfinfo;
  int subformat;

  sfinfo->format &= SF_FORMAT_TYPEMASK; /* clear submask */

  subformat =
    GPOINTER_TO_INT(g_object_get_data (G_OBJECT(widget), "default"));
  subformat &= SF_FORMAT_SUBMASK; /* get new subformat */

  sfinfo->format |= subformat;

  update_save_options_caps (so);
}

static void
set_rate_cb (GtkWidget * widget, gint samplerate, gpointer data)
{
  sndfile_save_options * so = (sndfile_save_options *)data;
  SF_INFO * sfinfo = so->sfinfo;

  sfinfo->samplerate = samplerate;

  update_ok_button (so);
}

static void
set_channels_cb (GtkWidget * widget, gint channels, gpointer data)
{
  sndfile_save_options * so = (sndfile_save_options *)data;
  SF_INFO * sfinfo = so->sfinfo;

  sfinfo->channels = channels;

  update_ok_button (so);
}

static GtkWidget *
create_sndfile_encoding_options_dialog (sndfile_save_options * so)
{
  GtkWidget * dialog;
  GtkWidget * ebox;
  GtkWidget * pixmap;
  GtkWidget * main_vbox;
  GtkWidget * notebook;
  GtkWidget * vbox;
  GtkWidget * table;
  GtkWidget * hbox;
  GtkWidget * label;
  GtkWidget * option_menu;
  GtkWidget * menu;
  GtkWidget * menuitem;
  GtkWidget * entry;
  GtkWidget * ok_button, * button;

/*GtkStyle * style;*/

  GtkTooltips * tooltips;

  sw_sample * sample = so->sample;
  SF_INFO * sfinfo = so->sfinfo;
  int subformat;

  SF_FORMAT_INFO  info ;
  int	k, count ;

  SF_INFO tmp_sfinfo;
	
  dialog = gtk_dialog_new ();
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  
  attach_window_close_accel(GTK_WINDOW(dialog));

  /* Set up the action area first so the sensitivity of the ok button
   *  can be updated by formats below. */

  /* OK */

  ok_button = gtk_button_new_with_label (_("OK"));
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (ok_button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), ok_button,
		      TRUE, TRUE, 0);
  gtk_widget_show (ok_button);

  so->ok_button = ok_button;

  /* Cancel */

  button = gtk_button_new_with_label (_("Cancel"));
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), button,
		      TRUE, TRUE, 0);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK (sndfile_save_options_dialog_cancel_cb),
		      so);



  main_vbox = GTK_DIALOG(dialog)->vbox;

  ebox = gtk_event_box_new ();
  gtk_box_pack_start (GTK_BOX(main_vbox), ebox, FALSE, FALSE, 0);
  gtk_widget_set_style (ebox, style_wb);
  gtk_widget_show (ebox);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(ebox), vbox);
  gtk_widget_show (vbox);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  pixmap = create_widget_from_xpm (dialog, libsndfile_xpm);
  gtk_box_pack_start (GTK_BOX (hbox), pixmap, FALSE, FALSE, 0);
  gtk_widget_show (pixmap);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, ebox,
			_("Powered by libsndfile"),
			NULL);

  /* Filename */

/* pangoise?

  style = gtk_style_copy (style_wb);
  gdk_font_unref (style->font);
  style->font =
  gdk_font_load("-*-helvetica-medium-r-normal-*-*-180-*-*-*-*-*-*");
  gtk_widget_push_style (style);
*/
  label = gtk_label_new (g_basename (so->pathname));
  gtk_box_pack_start (GTK_BOX(vbox), label,
		      FALSE, FALSE, 8);
  gtk_widget_show (label);
  
/*gtk_widget_pop_style ();*/

  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX(main_vbox), notebook, TRUE, TRUE, 4);
  gtk_widget_show (notebook);

  /* Options */

  /* XXX: prepend major format name */
  label = gtk_label_new (_("Encoding"));

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), vbox, label);
  gtk_container_set_border_width (GTK_CONTAINER(vbox), 4);
  gtk_widget_show (vbox);

  /* Encoding */

  table = gtk_table_new (4, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE(table), 8);
  gtk_table_set_col_spacings (GTK_TABLE(table), 8);
  gtk_box_pack_start (GTK_BOX(vbox), table, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER(table), 8);
  gtk_widget_show (table);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE(table), hbox, 0, 1, 0, 1,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (hbox);
  
  label = gtk_label_new (_("Encoding:"));
  gtk_box_pack_end (GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  option_menu = gtk_option_menu_new ();
  gtk_table_attach (GTK_TABLE(table), option_menu, 1, 2, 0, 1,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (option_menu);

  menu = gtk_menu_new ();

  /*------------------------------------------------------*/
  subformat = sfinfo->format & SF_FORMAT_SUBMASK;

  sf_command (NULL, SFC_GET_FORMAT_SUBTYPE_COUNT, &count, sizeof (int)) ;

  for (k = 0 ; k < count ; k++) {
    info.format = k ;
    sf_command (NULL, SFC_GET_FORMAT_SUBTYPE, &info, sizeof (info)) ;

    memset (&tmp_sfinfo, 0, sizeof (SF_INFO));
    tmp_sfinfo.channels = 1;
    tmp_sfinfo.format = (sfinfo->format & SF_FORMAT_TYPEMASK) | info.format;

    if (sf_format_check (&tmp_sfinfo)) {
      if (subformat == 0) {
	sfinfo->format |= info.format;
	subformat = info.format;
      }

      menuitem = gtk_menu_item_new_with_label (info.name);
      g_object_set_data (G_OBJECT(menuitem), "default", 
				GINT_TO_POINTER(info.format));
      g_signal_connect (G_OBJECT(menuitem), "activate",
			  G_CALLBACK(set_subformat_cb), so);
      gtk_menu_append (GTK_MENU(menu), menuitem);
      gtk_widget_show (menuitem);
      if (info.format == subformat) {
	gtk_menu_item_select (GTK_MENU_ITEM(menuitem));
      }
    }
  }

  

  /*------------------------------------------------------*/
  
  gtk_option_menu_set_menu (GTK_OPTION_MENU(option_menu), menu);
  gtk_widget_show (menu);

  /* Rate */

  if (!so->saving) {
    entry = samplerate_chooser_new (NULL);
    gtk_table_attach (GTK_TABLE(table), entry, 0, 2, 1, 2,
		      GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show (entry);

    g_signal_connect (G_OBJECT(entry), "number-changed",
			G_CALLBACK(set_rate_cb), so);

    if (sample == NULL)
      so->sfinfo->samplerate = samplerate_chooser_get_rate (entry);
    else
      samplerate_chooser_set_rate (entry, sample->sounddata->format->rate);

  } else {
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_table_attach (GTK_TABLE(table), hbox, 0, 1, 1, 2,
		      GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show (hbox);
    
    label = gtk_label_new (_("Sampling rate:"));
    gtk_box_pack_end (GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);
    
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_table_attach (GTK_TABLE(table), hbox, 1, 2, 1, 2,
		      GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show (hbox);

    label = gtk_label_new (g_strdup_printf ("%d Hz",
					    sample->sounddata->format->rate));
    gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);
  }

  /* Channels */

  entry = channelcount_chooser_new (NULL);
  gtk_table_attach (GTK_TABLE(table), entry, 0, 2, 2, 3,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (entry);
  
  g_signal_connect (G_OBJECT(entry), "number-changed",
		      G_CALLBACK(set_channels_cb), so);

  if (sample == NULL)
    so->sfinfo->channels = channelcount_chooser_get_count (entry);
  else
    channelcount_chooser_set_count (entry,
				    sample->sounddata->format->channels);

  /* About */

  label = gtk_label_new (_("About"));

  ebox = gtk_event_box_new ();
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), ebox, label);
  gtk_widget_set_style (ebox, style_wb);
  gtk_widget_show (ebox);

  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK(notebook), ebox,
				      TRUE, TRUE, GTK_PACK_END);

  vbox = gtk_vbox_new (FALSE, 16);
  gtk_container_add (GTK_CONTAINER(ebox), vbox);
  gtk_container_set_border_width (GTK_CONTAINER(vbox), 8);
  gtk_widget_show (vbox);

  label =
    gtk_label_new (_("Libsndfile is a C library by Erik de Castro Lopo\n"
		     "for reading and writing files containing sampled sound."));

  gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_set_style (label, style_wb);
  gtk_widget_show (label);

  button = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, FALSE, 8);
  gtk_widget_show (button);

  label = gtk_label_new (_("This user interface by Erik de Castro Lopo\n"
			   " and Conrad Parker,\n"
			   "Copyright (C) 2002 Erik de Castro Lopo\n"
			   "Copyright (C) 2002 CSIRO Australia.\n\n"));
  gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_set_style (label, style_wb);
  gtk_widget_show (label);


  gtk_widget_grab_default (ok_button);

  update_save_options_caps (so);
  update_save_options_values (so);

  return (dialog);
}

int
sndfile_save_options_dialog (sw_sample * sample, gchar * pathname)
{
  GtkWidget * dialog;
  sndfile_save_options * so;
  SF_INFO * sfinfo;
  sw_format * format;

  so = g_malloc0 (sizeof(*so));
  so->saving = TRUE;

  so->sample = sample;

  sfinfo = g_malloc0 (sizeof(SF_INFO));

  format = sample->sounddata->format;
  sfinfo->frames = (sf_count_t)sample->sounddata->nr_frames;
  sfinfo->samplerate = (int)format->rate;
  sfinfo->channels = (int)format->channels;

  sfinfo->format = sample->file_format;

  so->pathname = g_strdup (pathname);
  so->sfinfo = sfinfo;

  dialog = create_sndfile_encoding_options_dialog (so);
  gtk_window_set_title (GTK_WINDOW(dialog), _("Sweep: Save PCM options"));

  g_signal_connect (G_OBJECT(so->ok_button), "clicked",
		      G_CALLBACK (sndfile_save_options_dialog_ok_cb),
		      so);

  gtk_widget_show (dialog);

  return 0;
}

typedef struct _sf_data sf_data;

struct _sf_data {
  SNDFILE * sndfile;
  SF_INFO * sfinfo;
};

static sw_sample *
_sndfile_sample_load (sw_sample * sample, gchar * pathname, SF_INFO * sfinfo,
		      gboolean try_raw);

static void
sndfile_load_options_dialog_ok_cb (GtkWidget * widget, gpointer data)
{
  sndfile_save_options * so = (sndfile_save_options *)data;
  sw_sample * sample = so->sample;
  gchar * pathname = so->pathname;
  SF_INFO * sfinfo = so->sfinfo;
  GtkWidget * dialog;

  dialog = gtk_widget_get_toplevel (widget);
  gtk_widget_destroy (dialog);

  _sndfile_sample_load (sample, pathname, sfinfo, TRUE);

  g_free (so->pathname);
  g_free (so);
}


static sw_sample *
sample_load_sf_data (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  sf_data * sf = (sf_data *)inst->do_data;
  SNDFILE * sndfile = sf->sndfile;
  SF_INFO * sfinfo = sf->sfinfo;
  sw_audio_t * d;
  sw_framecount_t remaining, n, run_total;
  sw_framecount_t cframes;
  gint percent;

  struct stat statbuf;

  gboolean active = TRUE;

  sf_command (sndfile, SFC_SET_NORM_FLOAT, NULL, SF_TRUE) ;

  remaining = sfinfo->frames;
  run_total = 0;

  d = sample->sounddata->data;

  cframes = sfinfo->frames / 100;
  if (cframes == 0) cframes = 1;

  while (active && remaining > 0) {
    g_mutex_lock (sample->ops_mutex);

    if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
      active = FALSE;
    } else {
      n = MIN (remaining, 1024);
      n = sf_readf_float (sndfile, d, n);

      if (n == 0) {
	sweep_sndfile_perror (sndfile, sample->pathname);
	active = FALSE;
      }

      remaining -= n;

      d += (n * sfinfo->channels);

      run_total += n;
      percent = run_total / cframes;
      sample_set_progress_percent (sample, percent);
    }

    g_mutex_unlock (sample->ops_mutex);
  }

  sf_close (sndfile) ;

  if (remaining <= 0) {
    stat (sample->pathname, &statbuf);
    sample->last_mtime = statbuf.st_mtime;
    sample->edit_ignore_mtime = FALSE;
    sample->modified = FALSE;
  }

  sample_set_edit_state (sample, SWEEP_EDIT_STATE_DONE);

  return sample;
}

static sw_operation sndfile_load_op = {
  SWEEP_EDIT_MODE_FILTER,
  (SweepCallback)sample_load_sf_data,
  (SweepFunction)NULL,
  (SweepCallback)NULL, /* undo */
  (SweepFunction)NULL,
  (SweepCallback)NULL, /* redo */
  (SweepFunction)NULL
};

static sw_sample *
_sndfile_sample_load (sw_sample * sample, gchar * pathname, SF_INFO * sfinfo,
		      gboolean try_raw)
{
  SNDFILE * sndfile;
  char buf[128];
  gchar * message;

  gboolean isnew = (sample == NULL);

  sndfile_save_options * so;
  GtkWidget * dialog;

  sw_view * v;

  sf_data * sf;

#define RAW_ERR_STR_1 \
"Bad format specified for file open."

#define RAW_ERR_STR_2 \
"File opened for read. Format not recognised."

  if (sfinfo == NULL)
    sfinfo = g_malloc0 (sizeof (SF_INFO));

  if (!(sndfile = sf_open (pathname, SFM_READ, sfinfo) )) {
    /* If we're not trying raw files anyway, just return NULL */
    if (!try_raw) {
      g_free (sfinfo);
      return NULL;
    }

    sf_error_str (NULL, buf, sizeof (buf));
    if (!strncmp (buf, RAW_ERR_STR_1, sizeof (buf)) ||
	!strncmp (buf, RAW_ERR_STR_2, sizeof (buf))) {

      so = g_malloc0 (sizeof(*so));
      so->saving = FALSE;
      so->sample = sample;
      so->pathname = g_strdup (pathname);
      so->sfinfo = sfinfo;

      sfinfo->format = (SF_FORMAT_RAW | SF_FORMAT_PCM_S8);
      sfinfo->samplerate = 44100;
      /*-sfinfo->pcmbitwidth = 8;-*/
      sfinfo->channels = 2;

      dialog = create_sndfile_encoding_options_dialog (so);
      gtk_window_set_title (GTK_WINDOW(dialog),
			    _("Sweep: Load Raw PCM options"));

      g_signal_connect (G_OBJECT(so->ok_button), "clicked",
			  G_CALLBACK (sndfile_load_options_dialog_ok_cb),
			  so);

      gtk_widget_show (dialog);

    } else {
      if (errno == 0) {
	message = g_strdup_printf ("%s:\n%s", pathname, buf);
	info_dialog_new (buf, NULL, message);
	g_free (message);
      } else {
	/* We've already got the error string so no need to call
	 * sweep_sndfile_perror() here */
	sweep_perror (errno, "libsndfile: %s\n\n%s", buf, pathname);
      }

      g_free (sfinfo);
    }

    return NULL;
  }

  if (sample == NULL) {
    sample = sample_new_empty(pathname, sfinfo->channels, sfinfo->samplerate,
			      (sw_framecount_t)sfinfo->frames);
  } else {
    sounddata_destroy (sample->sounddata);
    sample->sounddata =
      sounddata_new_empty (sfinfo->channels, sfinfo->samplerate,
			   (sw_framecount_t)sfinfo->frames);
  }

  if(!sample) {
    g_free (sfinfo);
    return NULL;
  }

  sample->file_method = SWEEP_FILE_METHOD_LIBSNDFILE;
  sample->file_info = sfinfo;

  sample_bank_add(sample);

  if (isnew) {
    v = view_new_all (sample, 1.0);
    sample_add_view (sample, v);
  } else {
    trim_registered_ops (sample, 0);
  }

  g_snprintf (buf, sizeof (buf), _("Loading %s"), g_basename (sample->pathname));

  sf = g_malloc0 (sizeof(sf_data));

  sf->sndfile = sndfile;
  sf->sfinfo = sfinfo;

  schedule_operation (sample, buf, &sndfile_load_op, sf);

  return sample;
}

sw_sample *
sndfile_sample_reload (sw_sample * sample, gboolean try_raw)
{
  if (sample == NULL) return NULL;

  return _sndfile_sample_load (sample, sample->pathname, NULL, try_raw);
}

sw_sample *
sndfile_sample_load (gchar * pathname, gboolean try_raw)
{
  if (pathname == NULL) return NULL;

  return _sndfile_sample_load (NULL, pathname, NULL, try_raw);
}

static int
sndfile_sample_save_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  gchar * pathname = (gchar *)inst->do_data;

  SNDFILE *sndfile;
  SF_INFO * sfinfo;
  sw_format * format;
  sw_audio_t * fbuf, * d;
  sw_framecount_t nwritten = 0, len, n;
  sw_framecount_t cframes;
  int i, j;
  gint percent;

  gboolean active = TRUE;

  struct stat statbuf;

  if (sample == NULL) return -1;
  if (pathname == NULL) return -1;

  format = sample->sounddata->format;

  sfinfo = (SF_INFO *)sample->file_info;

  if (sfinfo == NULL) {
    sfinfo = g_malloc0 (sizeof(*sfinfo));
    sample->file_info = sfinfo;
  }

  sfinfo->samplerate  = (int)format->rate;
  sfinfo->frames     = (sf_count_t)sample->sounddata->nr_frames;

  if (!(sndfile = sf_open (pathname, SFM_WRITE, sfinfo))) {
    sweep_sndfile_perror (NULL, pathname);
    return -1;
  }

  /* Reset sample count which gets destroyed in open for write call. */
  sfinfo->frames     = (sf_count_t)sample->sounddata->nr_frames;

  sf_command (sndfile, SFC_SET_NORM_FLOAT, NULL, SF_TRUE) ;
  sf_command (sndfile, SFC_SET_ADD_DITHER_ON_WRITE, NULL, SF_TRUE);

  cframes = sfinfo->frames / 100;
  if (cframes == 0) cframes = 1;

  if ((int)format->channels == sfinfo->channels) {
    fbuf = sample->sounddata->data;
    while (active && nwritten < sfinfo->frames) {
      g_mutex_lock (sample->ops_mutex);

      if (sample->edit_mode == SWEEP_EDIT_MODE_META) {
	len = MIN (sfinfo->frames - nwritten, 1024);
	n = sf_writef_float (sndfile, fbuf, len);

	if (n == 0) {
	  sweep_sndfile_perror (sndfile, pathname);
	  active = FALSE;
	}

	fbuf += n * sfinfo->channels;
	nwritten += n;
	percent = nwritten / cframes;
	sample_set_progress_percent (sample, percent);
      } else {
	active = FALSE;
      }

      g_mutex_unlock (sample->ops_mutex);
    }
  } else if (format->channels == 1 && sfinfo->channels == 2) {
    /* Duplicate mono to stereo */
    fbuf = (sw_audio_t *)alloca(1024 * sizeof(sw_audio_t));
    d = sample->sounddata->data;
    while (active && nwritten < sfinfo->frames) {
      g_mutex_lock (sample->ops_mutex);

      if (sample->edit_mode == SWEEP_EDIT_MODE_META) {
	len = MIN (sfinfo->frames - nwritten, 512);
	for (i = 0; i < len; i++) {
	  fbuf[i*2] = fbuf[i*2+1] = *d++;
	}
	n = sf_writef_float (sndfile, fbuf, len);

	if (n == 0) {
	  sweep_sndfile_perror (sndfile, pathname);
	  active = FALSE;
	}

	nwritten += n;
	percent = nwritten / cframes;
	sample_set_progress_percent (sample, percent);
      } else {
	active = FALSE;
      }

      g_mutex_unlock (sample->ops_mutex);
    }
  } else if (format->channels == 2 && sfinfo->channels == 1) {
    /* Mix down stereo to mono */
    fbuf = (sw_audio_t *)alloca(1024 * sizeof(sw_audio_t));
    d = sample->sounddata->data;
    while (active && nwritten < sfinfo->frames) {
      g_mutex_lock (sample->ops_mutex);

      if (sample->edit_mode == SWEEP_EDIT_MODE_META) {
	len = MIN (sfinfo->frames - nwritten, 1024);
	for (i = 0; i < len; i++) {
	  fbuf[i] = *d++;
	  fbuf[i] += *d++;
	  fbuf[i] /= 2.0;
	}
	n = sf_writef_float (sndfile, fbuf, len);

	if (n == 0) {
	  sweep_sndfile_perror (sndfile, pathname);
	  active = FALSE;
	}

	nwritten += n;
	percent = nwritten / cframes;
	sample_set_progress_percent (sample, percent);
      } else {
	active = FALSE;
      }

      g_mutex_unlock (sample->ops_mutex);
    }
  } else {
    gint min_channels = MIN (format->channels, sfinfo->channels);
    size_t buf_size = 1024 * sizeof (sw_audio_t) * sfinfo->channels;

    /* Copy corresponding channels as much as possible */
    fbuf = (sw_audio_t *)alloca(buf_size);
    memset (fbuf, 0, buf_size);
    d = sample->sounddata->data;
    while (active && nwritten < sfinfo->frames) {
      g_mutex_lock (sample->ops_mutex);

      if (sample->edit_mode == SWEEP_EDIT_MODE_META) {
	len = MIN (sfinfo->frames - nwritten, 1024);
	for (i = 0; i < len; i++) {
	  for (j = 0; j < min_channels; j++) {
	    fbuf[i*sfinfo->channels + j] = d[j];
	  }
	  d += format->channels;
	}

	n = sf_writef_float (sndfile, fbuf, len);

	if (n == 0) {
	  sweep_sndfile_perror (sndfile, pathname);
	  active = FALSE;
	}

	nwritten += n;
	percent = nwritten / cframes;
	sample_set_progress_percent (sample, percent);
      } else {
	active = FALSE;
      }

      g_mutex_unlock (sample->ops_mutex);
    }
  }

  sf_close (sndfile) ;

  if (nwritten >= sfinfo->frames) {
    stat (pathname, &statbuf);
    sample->last_mtime = statbuf.st_mtime;
    sample->edit_ignore_mtime = FALSE;
    sample->modified = FALSE;

    sample_store_and_free_pathname (sample, pathname);
  }

  sample_set_edit_state (sample, SWEEP_EDIT_STATE_DONE);

  return 0;
}

static sw_operation sndfile_save_op = {
  SWEEP_EDIT_MODE_META,
  (SweepCallback)sndfile_sample_save_thread,
  (SweepFunction)NULL,
  (SweepCallback)NULL, /* undo */
  (SweepFunction)NULL,
  (SweepCallback)NULL, /* redo */
  (SweepFunction)NULL
};

int
sndfile_sample_save (sw_sample * sample, gchar * pathname)
{
  char buf[128];

  g_snprintf (buf, sizeof (buf), _("Saving %s"), g_basename (pathname));

  schedule_operation (sample, buf, &sndfile_save_op, g_strdup (pathname));

  return 0;
}

#endif
