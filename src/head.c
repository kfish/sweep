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
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include <gtk/gtk.h>

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_types.h>
#include <sweep/sweep_typeconvert.h>
#include "sweep_app.h"

#include "head.h"
#include "callbacks.h"
#include "interface.h"
#include "print.h"
#include "play.h"
#include "record.h"
#include "sample.h"

#include "../pixmaps/playrev.xpm"
#include "../pixmaps/loop.xpm"
#include "../pixmaps/recpausesel.xpm"
#include "../pixmaps/stop.xpm"
#include "../pixmaps/prevtrk.xpm"
#include "../pixmaps/nexttrk.xpm"
#include "../pixmaps/ff.xpm"
#include "../pixmaps/rew.xpm"
#include "../pixmaps/upleft.xpm"
#include "../pixmaps/lowleft.xpm"
#include "../pixmaps/upright.xpm"
#include "../pixmaps/lowright.xpm"

#define FRAMERATE 10

extern GtkStyle * style_wb;
extern GtkStyle * style_LCD;
extern GtkStyle * style_light_grey;
extern GtkStyle * style_red_grey;
extern GtkStyle * style_green_grey;
extern GtkStyle * style_red;

static void
head_init_repeater (sw_head * head, GtkFunction function, gpointer data)
{
  function (data);

  g_mutex_lock (&head->head_mutex);

  if (head->repeater_tag > 0) {
    g_source_remove (head->repeater_tag);
  }

  head->repeater_tag = g_timeout_add ((guint32)1000/FRAMERATE,
					function, data);

  g_mutex_unlock (&head->head_mutex);
}

void
hctl_repeater_released_cb (GtkWidget * widget, gpointer data)
{
  sw_head_controller * hctl = (sw_head_controller *)data;
  sw_head * head = hctl->head;

  g_mutex_lock (&head->head_mutex);

  if (head->repeater_tag > 0) {
    g_source_remove (head->repeater_tag);
    head->repeater_tag = 0;
  }

  g_mutex_unlock (&head->head_mutex);
}

void
hctl_loop_toggled_cb (GtkWidget * widget, gpointer data)
{
  sw_head_controller * hctl = (sw_head_controller *)data;
  sw_head * head = hctl->head;
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
  head_set_looping (head, active);
}

void
hctl_reverse_toggled_cb (GtkWidget * widget, gpointer data)
{
  sw_head_controller * hctl = (sw_head_controller *)data;
  sw_head * head = hctl->head;
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
  head_set_reverse (head, active);
}

void
hctl_reverse_toggle_cb (GtkWidget * widget, gpointer data)
{
  sw_head_controller * hctl = (sw_head_controller *)data;
  sw_head * head = hctl->head;

  head_set_reverse (head, !head->reverse);
}

void
hctl_mute_toggled_cb (GtkWidget * widget, gpointer data)
{
  sw_head_controller * hctl = (sw_head_controller *)data;
  sw_head * head = hctl->head;
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
  head_set_mute (head, active);
}

void
hctl_mute_toggle_cb (GtkWidget * widget, gpointer data)
{
  sw_head_controller * hctl = (sw_head_controller *)data;
  sw_head * head = hctl->head;

  head_set_mute (head, !head->mute);
}

void
hctl_record_cb (GtkWidget * widget, gpointer data)
{
  sw_head_controller * hctl = (sw_head_controller *)data;
  sw_head * head = hctl->head;

  record_cb (widget, head);
}

void
hctl_record_stop_cb (GtkWidget * widget, gpointer data)
{
  sw_head_controller * hctl = (sw_head_controller *)data;
  sw_head * head = hctl->head;

  stop_recording (head->sample);
}

void
hctl_refresh_looping (sw_head_controller * hctl)
{
  sw_head * head = hctl->head;

  if (hctl->loop_toggle) {
  g_signal_handlers_block_matched (GTK_OBJECT(hctl->loop_toggle), G_SIGNAL_MATCH_DATA, 0,
									0, 0, 0, hctl);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(hctl->loop_toggle),
				head->looping);
  g_signal_handlers_unblock_matched (GTK_OBJECT(hctl->loop_toggle), G_SIGNAL_MATCH_DATA, 0,
									0, 0, 0, hctl);
  }
}

void
hctl_refresh_reverse (sw_head_controller * hctl)
{
  sw_head * head = hctl->head;

  if (hctl->reverse_toggle) {
  g_signal_handlers_block_matched (GTK_OBJECT(hctl->reverse_toggle), G_SIGNAL_MATCH_DATA, 0,
									0, 0, 0, hctl);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(hctl->reverse_toggle),
				head->reverse);
  g_signal_handlers_unblock_matched (GTK_OBJECT(hctl->reverse_toggle), G_SIGNAL_MATCH_DATA, 0,
									0, 0, 0, hctl);
  }
}

void
hctl_refresh_mute (sw_head_controller * hctl)
{
  sw_head * head = hctl->head;

  if (hctl->mute_toggle) {
  g_signal_handlers_block_matched (GTK_OBJECT(hctl->mute_toggle), G_SIGNAL_MATCH_DATA, 0,
									0, 0, 0, hctl);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(hctl->mute_toggle),
				head->mute);
  g_signal_handlers_unblock_matched (GTK_OBJECT(hctl->mute_toggle), G_SIGNAL_MATCH_DATA, 0,
									0, 0, 0, hctl);
  }
}

void
hctl_refresh_going (sw_head_controller * hctl)
{
  sw_head * head = hctl->head;

  if (hctl->go_toggle) {
  g_signal_handlers_block_matched (GTK_OBJECT(hctl->go_toggle), G_SIGNAL_MATCH_DATA, 0,
									0, 0, 0, hctl);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(hctl->go_toggle),
				head->going);
  g_signal_handlers_unblock_matched (GTK_OBJECT(hctl->go_toggle), G_SIGNAL_MATCH_DATA, 0,
									0, 0, 0, hctl);
  }
}

void
hctl_refresh_offset (sw_head_controller * hctl)
{
  sw_head * head = hctl->head;
  sw_sample * sample = head->sample;
  sw_framecount_t offset = head->offset;

  char buf[16];

  snprint_time (buf, sizeof (buf),
		frames_to_time (sample->sounddata->format, offset));

  gtk_label_set_text (GTK_LABEL(hctl->pos_label), buf);
}

void
hctl_refresh_gain (sw_head_controller * hctl)
{
}

static gboolean
h_rewind_step (gpointer data)
{
  sw_head * head = (sw_head *)data;
  sw_sample * sample = head->sample;
  gint step;

  step = sample->sounddata->format->rate; /* 1 second */
  head_set_offset (head, head->offset - step);

  return TRUE;
}

static gboolean
h_ffwd_step (gpointer data)
{
  sw_head * head = (sw_head *)data;
  sw_sample * sample = head->sample;
  gint step;

  step = sample->sounddata->format->rate; /* 1 second */
  head_set_offset (head, head->offset + step);

  return TRUE;
}

void
hctl_rewind_pressed_cb (GtkWidget * widget, gpointer data)
{
  sw_head_controller * hctl = (sw_head_controller *)data;
  sw_head * head = hctl->head;

  head_init_repeater (head, (GtkFunction)h_rewind_step, head);
}

void
hctl_ffwd_pressed_cb (GtkWidget * widget, gpointer data)
{
  sw_head_controller * hctl = (sw_head_controller *)data;
  sw_head * head = hctl->head;

  head_init_repeater (head, (GtkFunction)h_ffwd_step, head);
}

void
hctl_goto_start_cb (GtkWidget * widget, gpointer data)
{
  sw_head_controller * hctl = (sw_head_controller *)data;
  sw_head * head = hctl->head;

  /*head_set_scrubbing (head, FALSE);*/
  head_set_offset (head, 0);
}

void
hctl_goto_end_cb (GtkWidget * widget, gpointer data)
{
  sw_head_controller * hctl = (sw_head_controller *)data;
  sw_head * head = hctl->head;
  sw_sample * sample = head->sample;

  /*head_set_scrubbing (head, FALSE);*/
  head_set_offset (head, sample->sounddata->nr_frames);
}

void
hctl_destroy_cb (GtkWidget * widget, gpointer data)
{
  sw_head_controller * hctl = (sw_head_controller *)data;

  hctl->head->controllers = g_list_remove (hctl->head->controllers, hctl);

  g_free (hctl);
}

void
head_controller_set_head (sw_head_controller * hctl, sw_head * head)
{
  sw_head * old_head = hctl->head;

  if (old_head != NULL) {
    old_head->controllers = g_list_remove (old_head->controllers, hctl);
  }

  hctl->head = head;

  if (g_list_find (head->controllers, hctl) == 0) {
    head->controllers = g_list_append (head->controllers, hctl);
  }

  hctl_refresh_looping (hctl);
  hctl_refresh_reverse (hctl);
  hctl_refresh_mute (hctl);
  hctl_refresh_going (hctl);
  hctl_refresh_offset (hctl);
  hctl_refresh_gain (hctl);
}

GtkWidget *
head_controller_create (sw_head * head, GtkWidget * window,
			sw_head_controller ** hctl_ret)
{
  sw_head_controller * hctl;

  GtkWidget * handlebox;
  GtkWidget * hbox;
  GtkWidget * tool_hbox;
  GtkWidget * frame;
  GtkWidget * lcdbox;
  GtkWidget * imagebox;
  GtkWidget * pixmap;
  GtkWidget * button;
  GtkWidget * label;

  GtkStyle * style;

  GtkTooltips * tooltips;

  g_assert (head != NULL);

  hctl = g_malloc0 (sizeof (sw_head_controller));
  hctl->head = head;


  if (head->type == SWEEP_HEAD_RECORD) {
      style = style_red_grey;
  } else {
      style = style_green_grey;
  }

    handlebox = gtk_handle_box_new ();
    /*    gtk_box_pack_start (GTK_BOX (main_vbox), handlebox, FALSE, TRUE, 0);
    gtk_widget_show (handlebox);
    */

    gtk_widget_set_style (handlebox, style);

    g_signal_connect (G_OBJECT (handlebox), "destroy",
			G_CALLBACK (hctl_destroy_cb), hctl);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_container_add (GTK_CONTAINER (handlebox), hbox);
    gtk_widget_show (hbox);

    gtk_widget_set_size_request(hbox, -1, 24);

    frame = gtk_frame_new (NULL);
    gtk_widget_set_style (frame, style_light_grey);
    gtk_box_pack_start (GTK_BOX(hbox), frame, TRUE, TRUE, 0);
    gtk_widget_show (frame);

    lcdbox = gtk_event_box_new ();
    gtk_widget_set_style (lcdbox, style_LCD);
    gtk_container_add (GTK_CONTAINER(frame), lcdbox);
    gtk_widget_show (lcdbox);

    tooltips = gtk_tooltips_new ();
    gtk_tooltips_set_tip (tooltips, lcdbox,
			  _("Cursor position (indicator)"), NULL);

    tool_hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER(lcdbox), tool_hbox);
    gtk_widget_show (tool_hbox);

    imagebox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX(tool_hbox), imagebox, FALSE, FALSE, 0);
    gtk_widget_show (imagebox);

    pixmap = create_widget_from_xpm (window, upleft_xpm);
    gtk_widget_show (pixmap);
    gtk_box_pack_start (GTK_BOX(imagebox), pixmap, FALSE, FALSE, 0);

    pixmap = create_widget_from_xpm (window, lowleft_xpm);
    gtk_widget_show (pixmap);
    gtk_box_pack_end (GTK_BOX(imagebox), pixmap, FALSE, FALSE, 0);

    label = gtk_label_new ("00:00:00.000");
    gtk_box_pack_start (GTK_BOX(tool_hbox), label, TRUE, TRUE, 0);
    gtk_widget_show (label);

    hctl->pos_label = label;

    imagebox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX(tool_hbox), imagebox, FALSE, FALSE, 0);
    gtk_widget_show (imagebox);

    pixmap = create_widget_from_xpm (window, upright_xpm);
    gtk_widget_show (pixmap);
    gtk_box_pack_start (GTK_BOX(imagebox), pixmap, FALSE, FALSE, 0);

    pixmap = create_widget_from_xpm (window, lowright_xpm);
    gtk_widget_show (pixmap);
    gtk_box_pack_end (GTK_BOX(imagebox), pixmap, FALSE, FALSE, 0);

    tool_hbox = gtk_hbox_new (TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), tool_hbox, FALSE, TRUE, 0);
    gtk_widget_show (tool_hbox);

    button = create_pixmap_button (window, playrev_xpm,
				   _("Reverse mode (toggle)"),
				   style, SW_TOOLBAR_TOGGLE_BUTTON,
				   G_CALLBACK (hctl_reverse_toggled_cb), NULL, NULL, hctl);

	g_signal_handlers_block_matched (GTK_OBJECT(button), G_SIGNAL_MATCH_DATA, 0,
									0, 0, 0, hctl);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button),
				  head->reverse);

	g_signal_handlers_unblock_matched (GTK_OBJECT(button), G_SIGNAL_MATCH_DATA, 0,
									0, 0, 0, hctl);

    gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
    gtk_widget_show (button);

    hctl->reverse_toggle = button;

    button = create_pixmap_button (window, loop_xpm,
				   _("Loop mode recording (toggle)"),
				   style, SW_TOOLBAR_TOGGLE_BUTTON,
				   G_CALLBACK (hctl_loop_toggled_cb), NULL, NULL, hctl);
    g_signal_handlers_block_matched (GTK_OBJECT(button), G_SIGNAL_MATCH_DATA, 0,
									0, 0, 0, hctl);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button),
				  head->looping);
    g_signal_handlers_unblock_matched (GTK_OBJECT(button), G_SIGNAL_MATCH_DATA, 0,
									0, 0, 0, hctl);
    gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
    gtk_widget_show (button);

    hctl->loop_toggle = button;

    tool_hbox = gtk_hbox_new (TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), tool_hbox, TRUE, TRUE, 0);
    gtk_widget_show (tool_hbox);

    button = create_pixmap_button (window, recpausesel_xpm,
				   _("Record into selection"),
				   style, SW_TOOLBAR_TOGGLE_BUTTON,
				   G_CALLBACK (hctl_record_cb), NULL, NULL, hctl);
    gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
    gtk_widget_show (button);
    g_signal_handlers_block_matched (GTK_OBJECT(button), G_SIGNAL_MATCH_DATA, 0,
									0, 0, 0, hctl);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button), head->going);
    g_signal_handlers_unblock_matched (GTK_OBJECT(button), G_SIGNAL_MATCH_DATA, 0,
									0, 0, 0, hctl);

    hctl->go_toggle = button;

    button = create_pixmap_button (window, stop_xpm, _("Stop"),
				   style, SW_TOOLBAR_BUTTON,
				   G_CALLBACK (hctl_record_stop_cb), NULL, NULL, hctl);
    gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
    gtk_widget_show (button);


    tool_hbox = gtk_hbox_new (TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), tool_hbox, FALSE, TRUE, 0);
    gtk_widget_show (tool_hbox);

    button
      = create_pixmap_button (window, prevtrk_xpm,
			      _("Go to beginning"),
			      style, SW_TOOLBAR_BUTTON,
			      G_CALLBACK (hctl_goto_start_cb), NULL, NULL, hctl);
    gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
    gtk_widget_show (button);

    /*NOALLOC(button);*/

    /* Rewind */

    button
      = create_pixmap_button (window, rew_xpm, _("Rewind"),
			      style, SW_TOOLBAR_BUTTON,
			      NULL,
			      G_CALLBACK (hctl_rewind_pressed_cb),
			      G_CALLBACK (hctl_repeater_released_cb), hctl);
    gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
    gtk_widget_show (button);

    /*    NOALLOC(button);*/

    /* Fast forward */

    button
      = create_pixmap_button (window, ff_xpm, _("Fast forward"),
			      style, SW_TOOLBAR_BUTTON,
			      NULL,
			      G_CALLBACK (hctl_ffwd_pressed_cb),
			      G_CALLBACK (hctl_repeater_released_cb),
			      hctl);
    gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
    gtk_widget_show (button);

    /*NOALLOC(button);*/

    /* End */

    button
      = create_pixmap_button (window, nexttrk_xpm,
			      _("Go to the end"),
			      style, SW_TOOLBAR_BUTTON,
			      G_CALLBACK (hctl_goto_end_cb), NULL, NULL, hctl);
    gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
    gtk_widget_show (button);

    /*NOALLOC(button);*/

    head->controllers = g_list_append (head->controllers, hctl);

    *hctl_ret = hctl;

    return handlebox;
}

/* head functions */

#define PBOOL(p) ((p) ? "TRUE" : "FALSE" )

int
head_dump (sw_head * head)
{
  printf ("head %p\toffset: %f", head, head->offset);
  printf ("\tstop:\t%" PRId64 "\n", head->stop_offset);
  printf ("\tgoing:\t%s\n", PBOOL(head->going));
  printf ("\trestricted:\t%s\n", PBOOL(head->restricted));
  printf ("\tlooping:\t%s\n", PBOOL(head->looping));
  printf ("\tpreviewing:\t%s\n", PBOOL(head->previewing));
  printf ("\treverse:\t%s\n", PBOOL(head->reverse));
  printf ("\tmute:\t%s\n", PBOOL(head->mute));
  printf ("\tgain:\t%f\n", head->gain);
  printf ("\trate:\t%f\n", head->rate);
  printf ("\tmix:\t%f\n", head->mix);
  return 0;
}

sw_head *
head_new (sw_sample * sample, sw_head_t head_type)
{
  sw_head * head;

  head = g_malloc0 (sizeof (sw_head));

  head->sample = sample;

  g_mutex_init (&head->head_mutex);
  head->type = head_type;
  head->stop_offset = 0;
  head->offset = 0;
  head->going = FALSE;
  head->restricted = FALSE;
  head->looping = FALSE;
  head->previewing = FALSE;
  head->reverse = FALSE;
  head->mute = FALSE;
  head->gain = (head->type == SWEEP_HEAD_PLAY ? 0.7 : 1.0);
  head->rate = 1.0;
  head->mix = 0.0;

  head->repeater_tag = -1;
  head->controllers = NULL;

  return head;
}

void
head_set_scrubbing (sw_head * h, gboolean scrubbing)
{
  g_mutex_lock (&h->head_mutex);

  h->scrubbing = scrubbing;

  g_mutex_unlock (&h->head_mutex);
}

void
head_set_previewing (sw_head * h, gboolean previewing)
{
  g_mutex_lock (&h->head_mutex);

  h->previewing = previewing;

  g_mutex_unlock (&h->head_mutex);
}

void
head_set_looping (sw_head * h, gboolean looping)
{
  GList * gl;
  sw_head_controller * hctl;

  g_mutex_lock (&h->head_mutex);

  h->looping = looping;

  for (gl = h->controllers; gl; gl = gl->next) {
    hctl = (sw_head_controller *)gl->data;
    hctl_refresh_looping (hctl);
  }

  g_mutex_unlock (&h->head_mutex);
}

void
head_set_reverse (sw_head * h, gboolean reverse)
{
  GList * gl;
  sw_head_controller * hctl;

  g_mutex_lock (&h->head_mutex);

  h->reverse = reverse;

  for (gl = h->controllers; gl; gl = gl->next) {
    hctl = (sw_head_controller *)gl->data;
    hctl_refresh_reverse (hctl);
  }

  g_mutex_unlock (&h->head_mutex);
}

void
head_set_mute (sw_head * h, gboolean mute)
{
  GList * gl;
  sw_head_controller * hctl;

  g_mutex_lock (&h->head_mutex);

  h->mute = mute;

  for (gl = h->controllers; gl; gl = gl->next) {
    hctl = (sw_head_controller *)gl->data;
    hctl_refresh_mute (hctl);
  }

  g_mutex_unlock (&h->head_mutex);
}

void
head_set_going (sw_head * h, gboolean going)
{
  GList * gl;
  sw_head_controller * hctl;

  g_mutex_lock (&h->head_mutex);

  h->going = going;

  for (gl = h->controllers; gl; gl = gl->next) {
    hctl = (sw_head_controller *)gl->data;
    hctl_refresh_going (hctl);
  }

  g_mutex_unlock (&h->head_mutex);
}

void
head_set_restricted (sw_head * h, gboolean restricted)
{
  GList * gl;
  sw_head_controller * hctl;

  g_mutex_lock (&h->head_mutex);

  h->restricted = restricted;

  for (gl = h->controllers; gl; gl = gl->next) {
    hctl = (sw_head_controller *)gl->data;
    hctl_refresh_going (hctl);
  }

  g_mutex_unlock (&h->head_mutex);
}

void
head_set_stop_offset (sw_head * h, sw_framecount_t offset)
{
  g_mutex_lock (&h->head_mutex);

  h->stop_offset = offset;

  g_mutex_unlock (&h->head_mutex);
}

void
head_set_offset (sw_head * h, sw_framecount_t offset)
{
  GList * gl;
  sw_head_controller * hctl;

  g_mutex_lock (&h->head_mutex);

  offset = CLAMP (offset, 0, h->sample->sounddata->nr_frames);

  h->offset = offset;

  for (gl = h->controllers; gl; gl = gl->next) {
    hctl = (sw_head_controller *)gl->data;
    hctl_refresh_offset (hctl);
  }

  if (h == h->sample->rec_head)
    sample_refresh_rec_marker (h->sample);

  g_mutex_unlock (&h->head_mutex);
}

void
head_set_gain (sw_head * h, gfloat gain)
{
  GList * gl;
  sw_head_controller * hctl;

  g_mutex_lock (&h->head_mutex);

  h->gain = gain;

  for (gl = h->controllers; gl; gl = gl->next) {
    hctl = (sw_head_controller *)gl->data;
    hctl_refresh_gain (hctl);
  }

  g_mutex_unlock (&h->head_mutex);
}

void
head_set_rate (sw_head * h, gfloat rate)
{
  g_mutex_lock (&h->head_mutex);

  h->rate = rate;

  g_mutex_unlock (&h->head_mutex);
}

void
head_set_monitor (sw_head * h, gboolean monitor)
{
  g_mutex_lock (&h->head_mutex);

  h->monitor = monitor;

  g_mutex_unlock (&h->head_mutex);

  sample_update_device (h->sample);
}

static sw_framecount_t
head_write_unrestricted (sw_head * head, float * buf,
			 sw_framecount_t count)
{
  sw_sample * sample = head->sample;
  sw_sounddata * sounddata = sample->sounddata;
  sw_format * f = sounddata->format;
  gpointer d;
  float * rd;
  sw_framecount_t i, j, t, b;

  d = sounddata->data +
    (int)frames_to_bytes (f, head->offset);
  rd = (float *)d;

  if (head->reverse) {
    b = 0;

    for (i = 0; i < count; i++) {
      for (j = 0; j < f->channels; j++) {
	t = (count-1 - i) * f->channels + j;
	rd[t] *= head->mix;
	rd[t] += (buf[b] * head->gain);
	b++;
      }
    }

    head->offset -= count;

  } else {
    for (i = 0; i < count * f->channels; i++) {
      rd[i] *= head->mix;
      rd[i] += (buf[i] * head->gain);
    }

    head->offset += count;

  }

  return count;
}

sw_framecount_t
head_write (sw_head * head, float * buf, sw_framecount_t count)
{
  sw_sample * sample = head->sample;
  sw_sounddata * sounddata = sample->sounddata;
  sw_format * f = sounddata->format;
  sw_framecount_t remaining = count, written = 0, n = 0;
  GList * gl;
  sw_sel * sel;

  while (head->restricted && remaining > 0) {
    g_mutex_lock (&sounddata->sels_mutex);

    /* Find selection region that offset is or should be in */
    if (head->reverse) {
      for (gl = g_list_last (sounddata->sels); gl; gl = gl->prev) {
	sel = (sw_sel *)gl->data;

	if (head->offset > sel->sel_end)
	  head->offset = sel->sel_end;

	if (head->offset > sel->sel_start) {
	  n = MIN (remaining, head->offset - sel->sel_start);
	  break;
	}
      }
    } else {
      for (gl = sounddata->sels; gl; gl = gl->next) {
	sel = (sw_sel *)gl->data;

	if (head->offset < sel->sel_start)
	  head->offset = sel->sel_start;

	if (head->offset < sel->sel_end) {
	  n = MIN (remaining, sel->sel_end - head->offset);
	  break;
	}
      }
    }

    g_mutex_unlock (&sounddata->sels_mutex);

    if (gl == NULL) {
      if (head->looping) {
	head->offset = head->reverse ? sounddata->nr_frames : 0;
      } else {
	head->going = FALSE;
	return written;
      }
    } else {
      written += head_write_unrestricted (head, buf, n);
      buf += (int)frames_to_samples (f, n);
      remaining -= n;
    }
  }

  if (remaining > 0) {
    written += head_write_unrestricted (head, buf, remaining);
  }

  return written;
}
