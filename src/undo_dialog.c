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
#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_undo.h>
#include <sweep/sweep_sample.h>
#include <sweep/sweep_sounddata.h>
#include <sweep/sweep_selection.h>

#include "sweep_app.h"
#include "edit.h"
#include "interface.h"

#include "../pixmaps/undo.xpm"
#include "../pixmaps/redo.xpm"
#include "../pixmaps/done.xpm"

/*#define DEBUG*/

static GtkWidget * undo_dialog = NULL;
static GtkWidget * undo_clist = NULL;
static GtkWidget * combo;
static GtkWidget * undo_button, * redo_button, * revert_button;
static sw_sample * ud_sample = NULL;

static void
undo_dialog_destroy (void)
{
  undo_dialog = NULL;
}

#if 0
static void
undo_dialog_ok_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;

  dialog = gtk_widget_get_toplevel (widget);
  gtk_widget_hide (dialog);
}
#endif

static void
undo_dialog_cancel_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget * dialog;

  dialog = gtk_widget_get_toplevel (widget);
  gtk_widget_hide (dialog);
}

/*
 * Must be called with sample->edit_mutex held
 */
void
undo_dialog_refresh_edit_mode (sw_sample * sample)
{
  if (sample != ud_sample) return;

  if (undo_dialog == NULL) return;

  switch (sample->edit_mode) {
  case SWEEP_EDIT_MODE_READY:
    gtk_widget_set_sensitive (undo_button, TRUE);
    gtk_widget_set_sensitive (redo_button, TRUE);
    gtk_widget_set_sensitive (undo_clist, TRUE);
    gtk_widget_set_sensitive (revert_button, TRUE);
    break;
  case SWEEP_EDIT_MODE_META:
  case SWEEP_EDIT_MODE_FILTER:
    gtk_widget_set_sensitive (undo_button, FALSE);
    gtk_widget_set_sensitive (redo_button, FALSE);
    gtk_widget_set_sensitive (undo_clist, FALSE);
    gtk_widget_set_sensitive (revert_button, FALSE);
    break;
  case SWEEP_EDIT_MODE_ALLOC:
    gtk_widget_set_sensitive (undo_button, FALSE);
    gtk_widget_set_sensitive (redo_button, FALSE);
    gtk_widget_set_sensitive (undo_clist, FALSE);
    gtk_widget_set_sensitive (revert_button, FALSE);
    break;
  default:
    g_assert_not_reached ();
  }
}

static void
_undo_dialog_set_sample (sw_sample * sample, gboolean select_current)
{
  GtkCList * clist;
  GList * gl;
  sw_op_instance * inst;
  gint i = 0;
  gchar * list_item[] = { "" };
  GdkColormap * colormap;
  GdkPixmap * pixmap_data;
  GdkBitmap * mask;
  gboolean done = FALSE;
  
  g_mutex_lock (sample->ops_mutex);

  clist = GTK_CLIST(undo_clist);

  colormap = gtk_widget_get_default_colormap ();
  pixmap_data =
    gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &mask,
					     NULL, done_xpm);
  gtk_clist_freeze (clist);

  gtk_clist_clear (clist);

  for (gl = g_list_last (sample->registered_ops); gl; gl = gl->prev) {
    inst = (sw_op_instance *)gl->data;

    gtk_clist_append (clist, list_item);
    gtk_clist_set_text (clist, i, 1, _(inst->description));

    if (gl == sample->current_undo) {
      done = TRUE;
      gtk_clist_moveto (clist, i, 0, 0.5, -1);
      if (select_current) gtk_clist_select_row (clist, i, 1);
    }

    if (done)
      gtk_clist_set_pixmap (clist, i, 0, pixmap_data, mask);

    i++;
  }

  gtk_clist_append (clist, list_item);
  gtk_clist_set_text (clist, i, 1, _("Original data"));
  gtk_clist_set_pixmap (clist, i, 0, pixmap_data, mask);

  if (sample->current_undo == NULL) {
    gtk_clist_moveto (clist, i, 0, 0.5, -1);
    if (select_current) gtk_clist_select_row (clist, i, 1);
  }

  gtk_clist_thaw (clist);

  g_mutex_unlock (sample->ops_mutex);

  ud_sample = sample;
  gtk_entry_set_text (GTK_ENTRY(GTK_COMBO(combo)->entry),
		      g_basename(sample->pathname));

  g_mutex_lock (ud_sample->edit_mutex);
  undo_dialog_refresh_edit_mode (ud_sample);
  g_mutex_unlock (ud_sample->edit_mutex);
}

void
undo_dialog_set_sample (sw_sample * sample)
{
  if (sample == NULL) return;

  if (undo_dialog == NULL || !GTK_WIDGET_VISIBLE(undo_dialog))
    return;

  _undo_dialog_set_sample (sample, (sample != ud_sample));
}

void
undo_dialog_refresh_history (sw_sample * sample)
{
  if (sample != ud_sample)
    return;

  if (undo_dialog == NULL || !GTK_WIDGET_VISIBLE(undo_dialog))
    return;
  
  _undo_dialog_set_sample (sample, FALSE);
}

void
undo_dialog_refresh_sample_list (void)
{
  GList * cbitems = NULL;

  if (undo_dialog == NULL)
    return;

  if ((cbitems = sample_bank_list_names ()) != NULL)
    gtk_combo_set_popdown_strings (GTK_COMBO(combo), cbitems);

  g_list_free (cbitems);
}

static void
undo_dialog_entry_changed_cb (GtkWidget * widget, gpointer data)
{
  GtkEntry * entry;
  gchar * new_text;
  sw_sample * sample;

  entry = GTK_ENTRY(GTK_COMBO(combo)->entry);
  new_text = gtk_entry_get_text (entry);

  sample = sample_bank_find_byname (new_text);

  if (sample == NULL) return;

  gtk_signal_handler_block_by_data (GTK_OBJECT(entry), NULL);
  _undo_dialog_set_sample (sample, TRUE);
  gtk_signal_handler_unblock_by_data (GTK_OBJECT(entry), NULL);
}

static void
undo_dialog_revert_cb (GtkWidget * widget, gpointer data)
{
  GList * gl, * sel_gl = NULL;
  /*  sw_op_instance * inst;*/
  gint i = 0, s;
  /*  gboolean need_undo = FALSE;*/

  s = GPOINTER_TO_INT((GTK_CLIST(undo_clist)->selection)->data);

  g_mutex_lock (ud_sample->ops_mutex);

  for (gl = g_list_last (ud_sample->registered_ops); gl; gl = gl->prev) {
#if 0
    inst = (sw_op_instance *)gl->data;

    if (gl == ud_sample->current_undo) {
      need_undo = TRUE;
    }
#endif

    if (i == s) {
      sel_gl = gl;
      break;
    }

    i++;
  }

  g_mutex_unlock (ud_sample->ops_mutex);

#if 0
  if (need_undo) {
    while (ud_sample->current_undo != sel_gl)
      undo_current (ud_sample);
  } else {
    while (ud_sample->current_undo != sel_gl)
      redo_current (ud_sample);
  }
#endif

  revert_op (ud_sample, sel_gl);
}

static void
ud_undo_cb (GtkWidget * widget, gpointer data)
{
  undo_current (ud_sample);
}

static void
ud_redo_cb (GtkWidget * widget, gpointer data)
{
  redo_current (ud_sample);
}

static GtkWidget *
ud_create_pixmap_button (GtkWidget * widget, gchar ** xpm_data,
			 const gchar * label_text, const gchar * tip_text,
			 GtkSignalFunc clicked)
{
  GtkWidget * hbox;
  GtkWidget * label;
  GtkWidget * pixmap;
  GtkWidget * button;
  GtkTooltips * tooltips;

  button = gtk_button_new ();

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER(button), hbox);
  gtk_container_set_border_width (GTK_CONTAINER(button), 8);
  gtk_widget_show (hbox);

  if (xpm_data != NULL) {
    pixmap = create_widget_from_xpm (widget, xpm_data);
    gtk_box_pack_start (GTK_BOX(hbox), pixmap, FALSE, FALSE, 8);
    gtk_widget_show (pixmap);
  }

  if (label_text != NULL) {
    label = gtk_label_new (label_text);
    gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 8);
    gtk_widget_show (label);
  }

  if (tip_text != NULL) {
    tooltips = gtk_tooltips_new ();
    gtk_tooltips_set_tip (tooltips, button, tip_text, NULL);
  }

  if (clicked != NULL) {
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC(clicked), NULL);
  }

  return button;
}

void
undo_dialog_create (sw_sample * sample)
{
  GtkWidget * vbox;
  GtkWidget * hbox /* , *button_hbox */;
  GtkWidget * label;
  /*  GtkWidget * ok_button;*/
  GtkWidget * button;
  GtkWidget * scrolled;
  gchar * titles[] = { "", N_("Action") };

  GtkAccelGroup * accel_group;

  if (undo_dialog == NULL) {
    undo_dialog = gtk_dialog_new ();
    gtk_window_set_wmclass(GTK_WINDOW(undo_dialog), "undo_dialog", "Sweep");
    gtk_window_set_title(GTK_WINDOW(undo_dialog), _("Sweep: History"));
    gtk_window_set_policy(GTK_WINDOW(undo_dialog), FALSE, FALSE, FALSE);
    gtk_window_position(GTK_WINDOW(undo_dialog), GTK_WIN_POS_MOUSE);
    gtk_container_border_width (GTK_CONTAINER(undo_dialog), 8);

    accel_group = gtk_accel_group_new ();
    gtk_window_add_accel_group (GTK_WINDOW(undo_dialog), accel_group);

    gtk_signal_connect(GTK_OBJECT(undo_dialog), "destroy",
		       (GtkSignalFunc) undo_dialog_destroy, NULL);

    gtk_accel_group_add (accel_group, GDK_w, GDK_CONTROL_MASK, GDK_NONE,
			 GTK_OBJECT(undo_dialog), "hide");

    vbox = GTK_DIALOG(undo_dialog)->vbox;

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_box_pack_start (GTK_BOX(vbox), hbox, TRUE, TRUE, 8);
    gtk_widget_show (hbox);

    label = gtk_label_new (_("File:"));
    gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    combo = gtk_combo_new ();
    gtk_box_pack_start (GTK_BOX(hbox), combo, TRUE, TRUE, 0);
    gtk_widget_show (combo);

    gtk_entry_set_editable (GTK_ENTRY(GTK_COMBO(combo)->entry), FALSE);

    gtk_signal_connect (GTK_OBJECT(GTK_COMBO(combo)->entry), "changed",
			GTK_SIGNAL_FUNC(undo_dialog_entry_changed_cb), NULL);

    hbox = gtk_hbox_new (TRUE, 8);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show (hbox);

    button = ud_create_pixmap_button (undo_dialog, undo_xpm, _("Undo"), _("Undo"),
				   ud_undo_cb);
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
    gtk_widget_show (button);
    gtk_widget_add_accelerator (button, "clicked", accel_group,
				GDK_z, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    undo_button = button;

    button = ud_create_pixmap_button (undo_dialog, redo_xpm, _("Redo"), _("Redo"),
				   ud_redo_cb);
    gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
    gtk_widget_show (button);
    gtk_widget_add_accelerator (button, "clicked", accel_group,
				GDK_r, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    redo_button = button;

    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled),
				    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_box_pack_start (GTK_BOX(GTK_DIALOG(undo_dialog)->vbox), scrolled,
			FALSE, FALSE, 0);
    gtk_widget_set_usize (scrolled, 360, 240);
    gtk_widget_show (scrolled);

    undo_clist = gtk_clist_new_with_titles (2, titles);
    gtk_clist_set_column_width (GTK_CLIST(undo_clist), 0, 20);
    gtk_clist_set_selection_mode (GTK_CLIST(undo_clist), GTK_SELECTION_BROWSE);
    gtk_clist_column_titles_passive (GTK_CLIST(undo_clist));
    /* set title actively for i18n */
    gtk_clist_set_column_title(GTK_CLIST(undo_clist), 1, _(titles[1]));
    gtk_container_add (GTK_CONTAINER(scrolled), undo_clist);
    gtk_widget_show (undo_clist);

    button = gtk_button_new_with_label (_("Revert to selected state"));
    GTK_WIDGET_SET_FLAGS (GTK_WIDGET (button), GTK_CAN_DEFAULT);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(undo_dialog)->action_area),
			button, TRUE, TRUE, 0);
    gtk_widget_show (button);
    gtk_signal_connect (GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC (undo_dialog_revert_cb),
			NULL);
    revert_button = button;

#if 0

    /* OK */

    ok_button = gtk_button_new_with_label (_("OK"));
    GTK_WIDGET_SET_FLAGS (GTK_WIDGET (ok_button), GTK_CAN_DEFAULT);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(undo_dialog)->action_area),
			ok_button, TRUE, TRUE, 0);
    gtk_widget_show (ok_button);
    gtk_signal_connect (GTK_OBJECT(ok_button), "clicked",
			GTK_SIGNAL_FUNC (undo_dialog_ok_cb),
			NULL);
#endif

    /* Cancel */

    button = gtk_button_new_with_label (_("Close"));
    GTK_WIDGET_SET_FLAGS (GTK_WIDGET (button), GTK_CAN_DEFAULT);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(undo_dialog)->action_area),
			button, FALSE, FALSE, 0);
    gtk_widget_show (button);
    gtk_signal_connect (GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC (undo_dialog_cancel_cb),
			NULL);

    gtk_widget_grab_default (button);

#if 0
    gtk_widget_grab_default (ok_button);
#endif


  }

  undo_dialog_refresh_sample_list ();
  _undo_dialog_set_sample (sample, TRUE);
  
  if (!GTK_WIDGET_VISIBLE(undo_dialog)) {
    gtk_widget_show (undo_dialog);
  } else {
    gdk_window_raise (undo_dialog->window);
  }
}
