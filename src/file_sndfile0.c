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

#define BUF_LEN 128

extern GtkStyle * style_wb;

#if ! defined (SNDFILE_1)

#include "../pixmaps/libsndfile.xpm"

typedef struct {
  int subformat;
  char * name;
} sndfile_subformat_names;

static sndfile_subformat_names subformat_names[] = {
  { SF_FORMAT_PCM, "PCM (Pulse Code Modulation)", },
  { SF_FORMAT_FLOAT, "32 bit floating point", },
  { SF_FORMAT_ULAW, "U-Law", },
  { SF_FORMAT_ALAW, "A-Law", },
  { SF_FORMAT_IMA_ADPCM, "IMA ADPCM", },
  { SF_FORMAT_MS_ADPCM, "Microsoft ADPCM", },
  { SF_FORMAT_PCM_BE, "Big endian PCM", },
  { SF_FORMAT_PCM_LE, "Little endian PCM", },
  { SF_FORMAT_PCM_S8, "Signed 8 bit PCM", },
  { SF_FORMAT_PCM_U8, "Unsigned 8 bit PCM", },
  { SF_FORMAT_SVX_FIB, "SVX Fibonacci Delta", },
  { SF_FORMAT_SVX_EXP, "SVX Exponential Delta", },
  { SF_FORMAT_GSM610, "GSM 6.10", },
  { SF_FORMAT_G721_32, "32kbs G721 ADPCM", },
  { SF_FORMAT_G723_24, "24kbs G723 ADPCM", },
  { SF_FORMAT_FLOAT_BE, "Big endian floating point", },
  { SF_FORMAT_FLOAT_LE, "Little endian floating point", },
};

/*
 * Subformat capabilities.
 *
 * Logic for these derived from sf_format_check() in libsndfile::sndfile.c
 */

typedef struct {
  int subformat;
  gboolean cap_8bit;
  gboolean cap_16bit;
  gboolean cap_24bit;
  gboolean cap_32bit;
  gboolean cap_mono;
  gboolean cap_stereo;
} sndfile_subformat_caps;

static sndfile_subformat_caps wav_caps[] = {
  { SF_FORMAT_PCM, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_IMA_ADPCM, FALSE, TRUE, FALSE, FALSE, TRUE, TRUE, },
  { SF_FORMAT_MS_ADPCM, FALSE, TRUE, FALSE, FALSE, TRUE, TRUE, },
  { SF_FORMAT_GSM610, FALSE, TRUE, FALSE, FALSE, TRUE, FALSE, },
  { SF_FORMAT_ULAW, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_ALAW, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_FLOAT, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, },
  { 0, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
};

static sndfile_subformat_caps aiff_caps[] = {
  { SF_FORMAT_PCM, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_PCM_LE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_PCM_BE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_FLOAT, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, },
  { 0, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
};

static sndfile_subformat_caps au_caps[] = {
  { SF_FORMAT_PCM, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_ULAW, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_ALAW, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_G721_32, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, },
  { SF_FORMAT_G723_24, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, },
  { SF_FORMAT_FLOAT, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, },
  { 0, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
};

static sndfile_subformat_caps raw_caps[] = {
  { SF_FORMAT_PCM_S8, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, },
  { SF_FORMAT_PCM_U8, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, },
  { SF_FORMAT_PCM_BE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_PCM_LE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { 0, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
};

static sndfile_subformat_caps paf_caps[] = {
  { SF_FORMAT_PCM_S8, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, },
  { SF_FORMAT_PCM_BE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_PCM_LE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { 0, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
};

static sndfile_subformat_caps svx_caps[] = {
  { SF_FORMAT_PCM, TRUE, TRUE, FALSE, FALSE, TRUE, TRUE },
  { 0, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
};

static sndfile_subformat_caps nist_caps[] = {
  { SF_FORMAT_PCM, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_PCM_BE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_PCM_LE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { 0, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
};

static sndfile_subformat_caps ircam_caps[] = {
  { SF_FORMAT_PCM, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_PCM_BE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_PCM_LE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_FLOAT, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, },  
  { SF_FORMAT_FLOAT_BE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, },  
  { SF_FORMAT_FLOAT_LE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_ULAW, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { SF_FORMAT_ALAW, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, },
  { 0, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
};

typedef struct {
  int format;
  sndfile_subformat_caps * subformat_caps;
} sndfile_format_caps;

static sndfile_format_caps format_caps[] = {
  { SF_FORMAT_WAV, wav_caps, },
  { SF_FORMAT_AIFF, aiff_caps, },
  { SF_FORMAT_AU, au_caps, },
  { SF_FORMAT_AULE, au_caps, },
  { SF_FORMAT_RAW, raw_caps },
  { SF_FORMAT_PAF, paf_caps, },
  { SF_FORMAT_SVX, svx_caps, },
  { SF_FORMAT_NIST, nist_caps, },
  { SF_FORMAT_IRCAM, ircam_caps, },
};

typedef struct {
  gboolean saving; /* loading or saving ? */
  sw_sample * sample;
  char * pathname;
  SF_INFO * sfinfo;
#if 0
  GtkWidget * radio_mono;
  GtkWidget * radio_stereo;
#endif
  GtkWidget * radio_8bit;
  GtkWidget * radio_16bit;
  GtkWidget * radio_24bit;
  GtkWidget * radio_32bit;
  GtkWidget * ok_button;
} sndfile_save_options;

static char *
get_subformat_name (int subformat)
{
  int i;

  subformat &= SF_FORMAT_SUBMASK;

  for (i = 0; i < sizeof(subformat_names)/sizeof(sndfile_subformat_names); i++) {
    if (subformat_names[i].subformat == subformat)
      return subformat_names[i].name;
  }

  return NULL;
}

static sndfile_subformat_caps *
get_format_caps (int format)
{
  int i;

  format &= SF_FORMAT_TYPEMASK;

  for (i = 0; i < sizeof(format_caps)/sizeof(sndfile_format_caps); i++) {
    if (format_caps[i].format == format)
      return format_caps[i].subformat_caps;
  }

  return NULL;
}

static sndfile_subformat_caps *
get_subformat_caps (sndfile_subformat_caps * format_subcaps, int subformat)
{
  subformat &= SF_FORMAT_SUBMASK;

  while (format_subcaps) {
    if (format_subcaps->subformat == subformat) {
      return format_subcaps;
    }
    format_subcaps++;
  }

  return NULL;
}

static void
sweep_sndfile_perror (SNDFILE * sndfile, char * pathname)
{
#undef BUF_LEN
#define BUF_LEN 128
  char buf[BUF_LEN];

  sf_error_str (sndfile, buf, BUF_LEN);
  
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

#if 0
  sample_store_and_free_pathname (sample, so->pathname);
#endif

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
  SF_INFO * sfinfo;
  sndfile_subformat_caps * fcaps, * caps;

  sfinfo = so->sfinfo;
  fcaps = get_format_caps (sfinfo->format);
  caps = get_subformat_caps (fcaps, sfinfo->format);

  if (caps) {
#if 0
    gtk_widget_set_sensitive (so->radio_mono, caps->cap_mono);
    gtk_widget_set_sensitive (so->radio_stereo, caps->cap_stereo);
#endif
    gtk_widget_set_sensitive (so->radio_8bit, caps->cap_8bit);
    gtk_widget_set_sensitive (so->radio_16bit, caps->cap_16bit);
    gtk_widget_set_sensitive (so->radio_24bit, caps->cap_24bit);
    gtk_widget_set_sensitive (so->radio_32bit, caps->cap_32bit);
  } else {
#if 0
    gtk_widget_set_sensitive (so->radio_mono, FALSE);
    gtk_widget_set_sensitive (so->radio_stereo, FALSE);
#endif
    gtk_widget_set_sensitive (so->radio_8bit, FALSE);
    gtk_widget_set_sensitive (so->radio_16bit, FALSE);
    gtk_widget_set_sensitive (so->radio_24bit, FALSE);
    gtk_widget_set_sensitive (so->radio_32bit, FALSE);
  }

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

#define BLOCK_AND_SET(r,d) \
  gtk_signal_handler_block_by_data(GTK_OBJECT((r)), (d));       \
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON((r)), TRUE);  \
  gtk_signal_handler_unblock_by_data(GTK_OBJECT((r)), (d));

  switch (sfinfo->pcmbitwidth) {
  case 8:
    BLOCK_AND_SET(so->radio_8bit, so);
    break;
  case 16:
    BLOCK_AND_SET(so->radio_16bit, so);
    break;
  case 24:
    BLOCK_AND_SET(so->radio_24bit, so);
    break;
    case 32:
      BLOCK_AND_SET(so->radio_32bit, so);
      break;
  default:
    break;
  }

#if 0
  switch (sfinfo->channels) {
  case 1:
    BLOCK_AND_SET(so->radio_mono, so);
    break;
  case 2:
    BLOCK_AND_SET(so->radio_stereo, so);
    break;
  default:
    break;
  }
#endif

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
    GPOINTER_TO_INT(gtk_object_get_user_data (GTK_OBJECT(widget)));
  subformat &= SF_FORMAT_SUBMASK; /* get new subformat */

  sfinfo->format |= subformat;

  update_save_options_caps (so);
}

static void
set_bits_cb (GtkWidget * widget, gpointer data)
{
  sndfile_save_options * so = (sndfile_save_options *)data;
  SF_INFO * sfinfo = so->sfinfo;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget))) {
    sfinfo->pcmbitwidth = (int) gtk_object_get_user_data (GTK_OBJECT(widget));
  }

  update_ok_button (so);

  return;
}

static void
set_rate_cb (GtkWidget * widget, gint samplerate, gpointer data)
{
  sndfile_save_options * so = (sndfile_save_options *)data;
  SF_INFO * sfinfo = so->sfinfo;
#if 0
  gchar * entry_text;

  entry_text = gtk_entry_get_text (GTK_ENTRY(widget));
  sfinfo->samplerate = atoi (entry_text);
#else
  sfinfo->samplerate = samplerate;
#endif

  update_ok_button (so);
}

static void
set_channels_cb (GtkWidget * widget, gint channels, gpointer data)
{
  sndfile_save_options * so = (sndfile_save_options *)data;
  SF_INFO * sfinfo = so->sfinfo;

#if 0
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget))) {
    sfinfo->channels = (int) gtk_object_get_user_data (GTK_OBJECT(widget));
  }
#else
  sfinfo->channels = channels;
#endif

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
  GtkWidget * radio;

  GtkStyle * style;

  GtkTooltips * tooltips;

  sw_sample * sample = so->sample;
  SF_INFO * sfinfo = so->sfinfo;
  sndfile_subformat_caps * fcaps;
  int subformat;
  char * name;

  dialog = gtk_dialog_new ();
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);

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
  gtk_signal_connect (GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC (sndfile_save_options_dialog_cancel_cb),
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

  style = gtk_style_copy (style_wb);
  gdk_font_unref (style->font);
  style->font =
    gdk_font_load("-*-helvetica-medium-r-normal-*-*-180-*-*-*-*-*-*");
  gtk_widget_push_style (style);

  label = gtk_label_new (g_basename (so->pathname));
  gtk_box_pack_start (GTK_BOX(vbox), label,
		      FALSE, FALSE, 8);
  gtk_widget_show (label);
  
  gtk_widget_pop_style ();

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

  fcaps = get_format_caps (sfinfo->format);
  
  subformat = sfinfo->format & SF_FORMAT_SUBMASK;

  if (fcaps) {
    while (fcaps && (fcaps->subformat != 0)) {
      name = get_subformat_name(fcaps->subformat);
      menuitem = gtk_menu_item_new_with_label (name);
      gtk_object_set_user_data (GTK_OBJECT(menuitem),
				GINT_TO_POINTER(fcaps->subformat));
      gtk_signal_connect (GTK_OBJECT(menuitem), "activate",
			  GTK_SIGNAL_FUNC(set_subformat_cb), so);
      gtk_menu_append (GTK_MENU(menu), menuitem);
      gtk_widget_show (menuitem);
      if (fcaps->subformat == subformat) {
	gtk_menu_item_select (GTK_MENU_ITEM(menuitem));
      }
      fcaps++;
    }
  }

  gtk_option_menu_set_menu (GTK_OPTION_MENU(option_menu), menu);
  gtk_widget_show (menu);

  /* Rate */

#if 1
  if (!so->saving) {
    entry = samplerate_chooser_new (NULL);
    gtk_table_attach (GTK_TABLE(table), entry, 0, 2, 1, 2,
		      GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show (entry);

    gtk_signal_connect (GTK_OBJECT(entry), "number-changed",
			GTK_SIGNAL_FUNC(set_rate_cb), so);

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

#else
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

  if (sample == NULL) {
    entry = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_widget_show (entry);
    
    snprintf (buf, BUF_LEN, "%d", 44100);
    gtk_entry_set_text (GTK_ENTRY (entry), buf); 

    gtk_signal_connect (GTK_OBJECT(entry), "changed", set_rate_cb, so);

    label = gtk_label_new ("Hz");
    gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 2);
    gtk_widget_show (label);
  } else {
    label = gtk_label_new (g_strdup_printf ("%d Hz",
					    sample->sounddata->format->rate));
    gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);
  }
#endif

  /* Channels */

#if 1
  entry = channelcount_chooser_new (NULL);
  gtk_table_attach (GTK_TABLE(table), entry, 0, 2, 2, 3,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (entry);
  
  gtk_signal_connect (GTK_OBJECT(entry), "number-changed",
		      GTK_SIGNAL_FUNC(set_channels_cb), so);

  if (sample == NULL)
    so->sfinfo->channels = channelcount_chooser_get_count (entry);
  else
    channelcount_chooser_set_count (entry,
				    sample->sounddata->format->channels);
#else
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE(table), hbox, 0, 1, 2, 3,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Channels:"));
  gtk_box_pack_end (GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  hbox = gtk_hbox_new (TRUE, 0);
  gtk_table_attach (GTK_TABLE(table), hbox, 1, 2, 2, 3,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (hbox);

  if (sample == NULL || sample->sounddata->format->channels == 1) {
    radio = gtk_radio_button_new_with_label (NULL, _("Mono"));
  } else {
    radio = gtk_radio_button_new_with_label (NULL, _("Mono (mixdown)"));
  }
  gtk_box_pack_start (GTK_BOX(hbox), radio, TRUE, TRUE, 0);
  gtk_object_set_user_data (GTK_OBJECT(radio), (gpointer)1);
  gtk_signal_connect (GTK_OBJECT(radio), "toggled",
		      set_channels_cb, so);
  gtk_widget_show (radio);

  so->radio_mono = radio;

  if (sample == NULL || sample->sounddata->format->channels == 2) {
    radio = gtk_radio_button_new_with_label
      (gtk_radio_button_group (GTK_RADIO_BUTTON(radio)), _("Stereo"));
  } else {
    radio = gtk_radio_button_new_with_label
      (gtk_radio_button_group (GTK_RADIO_BUTTON(radio)),
       _("Stereo (duplicate)"));
  }
  gtk_box_pack_start (GTK_BOX(hbox), radio, TRUE, TRUE, 0);
  gtk_object_set_user_data (GTK_OBJECT(radio), (gpointer)2);
  gtk_signal_connect (GTK_OBJECT(radio), "toggled",
		      set_channels_cb, so);
  gtk_widget_show (radio);

  so->radio_stereo = radio;
#endif

  /* Bits */

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE(table), hbox, 0, 1, 3, 4,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Bitwidth:"));
  gtk_box_pack_end (GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  hbox = gtk_hbox_new (TRUE, 0);
  gtk_table_attach (GTK_TABLE(table), hbox, 1, 2, 3, 4,
		    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
  gtk_widget_show (hbox);

  radio = gtk_radio_button_new_with_label (NULL, _("8 bit"));
  gtk_box_pack_start (GTK_BOX(hbox), radio, FALSE, FALSE, 0);
  gtk_object_set_user_data (GTK_OBJECT(radio), (gpointer)8);
  gtk_signal_connect (GTK_OBJECT(radio), "toggled",
		      set_bits_cb, so);
  gtk_widget_show (radio);

  so->radio_8bit = radio;

  radio = gtk_radio_button_new_with_label
    (gtk_radio_button_group (GTK_RADIO_BUTTON(radio)), _("16 bit"));
  gtk_box_pack_start (GTK_BOX(hbox), radio, FALSE, FALSE, 0);
  gtk_object_set_user_data (GTK_OBJECT(radio), (gpointer)16);
  gtk_signal_connect (GTK_OBJECT(radio), "toggled",
		      set_bits_cb, so);
  gtk_widget_show (radio);

  so->radio_16bit = radio;

  radio = gtk_radio_button_new_with_label
    (gtk_radio_button_group (GTK_RADIO_BUTTON(radio)), _("24 bit"));
  gtk_box_pack_start (GTK_BOX(hbox), radio, FALSE, FALSE, 0);
  gtk_object_set_user_data (GTK_OBJECT(radio), (gpointer)24);
  gtk_signal_connect (GTK_OBJECT(radio), "toggled",
		      set_bits_cb, so);
  gtk_widget_show (radio);

  so->radio_24bit = radio;

  radio = gtk_radio_button_new_with_label
    (gtk_radio_button_group (GTK_RADIO_BUTTON(radio)), _("32 bit"));
  gtk_box_pack_start (GTK_BOX(hbox), radio, FALSE, FALSE, 0);
  gtk_object_set_user_data (GTK_OBJECT(radio), (gpointer)32);
  gtk_signal_connect (GTK_OBJECT(radio), "toggled",
		      set_bits_cb, so);
  gtk_widget_show (radio);

  so->radio_32bit = radio;

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

  /* Conrad wrote the libsndfile0 interface, Erik ported it to libsndfile1 */
  label = gtk_label_new (_("This user interface by Conrad Parker,\n"
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
sndfile_save_options_dialog (sw_sample * sample, char * pathname)
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
  sfinfo->samples = (unsigned int)sample->sounddata->nr_frames;
  sfinfo->samplerate = (unsigned int)format->rate;
  sfinfo->channels = (unsigned int)format->channels;

  so->pathname = pathname;
  so->sfinfo = sfinfo;

  switch (sample->file_format) {
  case SWEEP_FILE_FORMAT_RAW:
    sfinfo->format = (SF_FORMAT_RAW);
    break;
  case SWEEP_FILE_FORMAT_AIFF:
    sfinfo->format = (SF_FORMAT_AIFF | SF_FORMAT_PCM);
    break;
  case SWEEP_FILE_FORMAT_AU:
    sfinfo->format = (SF_FORMAT_AU | SF_FORMAT_PCM);
    break;
  case SWEEP_FILE_FORMAT_PAF:
    sfinfo->format = (SF_FORMAT_PAF);
    break;
  case SWEEP_FILE_FORMAT_SVX:
    sfinfo->format = (SF_FORMAT_SVX);
    break;
  case SWEEP_FILE_FORMAT_IRCAM:
    sfinfo->format = (SF_FORMAT_IRCAM);
    break;
  case SWEEP_FILE_FORMAT_VOC:
    sfinfo->format = (SF_FORMAT_VOC);
    break;
  case SWEEP_FILE_FORMAT_WAV:
  default:
    sfinfo->format = (SF_FORMAT_WAV | SF_FORMAT_PCM);
    break;
  }

  dialog = create_sndfile_encoding_options_dialog (so);
  gtk_window_set_title (GTK_WINDOW(dialog), _("Sweep: Save PCM options"));

  gtk_signal_connect (GTK_OBJECT(so->ok_button), "clicked",
		      GTK_SIGNAL_FUNC (sndfile_save_options_dialog_ok_cb),
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
_sndfile_sample_load (sw_sample * sample, char * pathname, SF_INFO * sfinfo,
		      gboolean try_raw);

static void
sndfile_load_options_dialog_ok_cb (GtkWidget * widget, gpointer data)
{
  sndfile_save_options * so = (sndfile_save_options *)data;
  sw_sample * sample = so->sample;
  char * pathname = so->pathname;
  SF_INFO * sfinfo = so->sfinfo;
  GtkWidget * dialog;

  dialog = gtk_widget_get_toplevel (widget);
  gtk_widget_destroy (dialog);

  _sndfile_sample_load (sample, pathname, sfinfo, TRUE);

  /* TEST: g_free (so->pathname); */
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

  sf_command  (sndfile, "norm float", "on", 0) ;

  remaining = sfinfo->samples;
  run_total = 0;

  d = sample->sounddata->data;

  cframes = sfinfo->samples / 100;
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
_sndfile_sample_load (sw_sample * sample, char * pathname, SF_INFO * sfinfo,
		      gboolean try_raw)
{
  SNDFILE * sndfile;

#undef BUF_LEN
#define BUF_LEN 128
  char buf[BUF_LEN];
  gchar * message;

  gboolean isnew = (sample == NULL);

  sndfile_save_options * so;
  GtkWidget * dialog;

  sw_view * v;

  sf_data * sf;

#define RAW_ERR_STR \
"Error while opening RAW file for read. Must specify format, pcmbitwidth and channels."

  if (sfinfo == NULL)
    sfinfo = g_malloc0 (sizeof (SF_INFO));

  if (!(sndfile = sf_open_read (pathname, sfinfo))) {
    /* If we're not trying raw files anyway, just return NULL */
    if (!try_raw) {
      g_free (sfinfo);
      return NULL;
    }

    sf_error_str (NULL, buf, BUF_LEN);
    if (!strncmp (buf, RAW_ERR_STR, BUF_LEN)) {

      so = g_malloc0 (sizeof(*so));
      so->sample = sample;
      so->pathname = g_strdup (pathname);
      so->sfinfo = sfinfo;

      sfinfo->format = (SF_FORMAT_RAW | SF_FORMAT_PCM_S8);
      sfinfo->samplerate = 44100;
      sfinfo->pcmbitwidth = 8;
      sfinfo->channels = 2;

      dialog = create_sndfile_encoding_options_dialog (so);
      gtk_window_set_title (GTK_WINDOW(dialog),
			    _("Sweep: Load Raw PCM options"));

      gtk_signal_connect (GTK_OBJECT(so->ok_button), "clicked",
			  GTK_SIGNAL_FUNC (sndfile_load_options_dialog_ok_cb),
			  so);

      gtk_widget_show (dialog);

    } else {
      if (errno == 0) {
	message = g_strdup_printf ("%s:\n%s", pathname, buf);
	info_dialog_new (buf, NULL, message);
	g_free (message);
      } else {
	sweep_perror (errno, "libsndfile: %s\n\n%s", buf, pathname);
      }

      g_free (sfinfo);
    }

    return NULL;
  }

  if (sample == NULL) {
    sample = sample_new_empty(pathname, sfinfo->channels, sfinfo->samplerate,
			      (sw_framecount_t)sfinfo->samples);
  } else {
    sounddata_destroy (sample->sounddata);
    sample->sounddata =
      sounddata_new_empty (sfinfo->channels, sfinfo->samplerate,
			   (sw_framecount_t)sfinfo->samples);
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

  g_snprintf (buf, BUF_LEN, _("Loading %s"), g_basename (sample->pathname));

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
sndfile_sample_load (char * pathname, gboolean try_raw)
{
  if (pathname == NULL) return NULL;

  return _sndfile_sample_load (NULL, pathname, NULL, try_raw);
}

static int
sndfile_sample_save_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  char * pathname = (char *)inst->do_data;

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
  sfinfo->samples     = (int)sample->sounddata->nr_frames;

  if (!(sndfile = sf_open_write (pathname, sfinfo))) {      
    sweep_sndfile_perror (NULL, pathname);
    return -1;
  }

  sf_command  (sndfile, "norm float", "on", 0) ;

  cframes = sfinfo->samples / 100;
  if (cframes == 0) cframes = 1;

  if ((int)format->channels == sfinfo->channels) {
    fbuf = sample->sounddata->data;
    while (active && nwritten < sfinfo->samples) {
      g_mutex_lock (sample->ops_mutex);

      if (sample->edit_mode == SWEEP_EDIT_MODE_META) {
	len = MIN (sfinfo->samples - nwritten, 1024);
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
    while (active && nwritten < sfinfo->samples) {
      g_mutex_lock (sample->ops_mutex);

      if (sample->edit_mode == SWEEP_EDIT_MODE_META) {
	len = MIN (sfinfo->samples - nwritten, 512);
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
    while (active && nwritten < sfinfo->samples) {
      g_mutex_lock (sample->ops_mutex);

      if (sample->edit_mode == SWEEP_EDIT_MODE_META) {
	len = MIN (sfinfo->samples - nwritten, 1024);
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
    while (active && nwritten < sfinfo->samples) {
      g_mutex_lock (sample->ops_mutex);

      if (sample->edit_mode == SWEEP_EDIT_MODE_META) {
	len = MIN (sfinfo->samples - nwritten, 1024);
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

  if (nwritten >= sfinfo->samples) {
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
sndfile_sample_save (sw_sample * sample, char * pathname)
{
  char buf[BUF_LEN];

  g_snprintf (buf, BUF_LEN, _("Saving %s"), g_basename (pathname));

  schedule_operation (sample, buf, &sndfile_save_op, pathname);

  return 0;
}

#endif
