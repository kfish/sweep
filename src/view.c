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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "view.h"
#include "callbacks.h"
#include "interface.h"
#include "format.h"
#include "param.h"
#include "sample.h"
#include "sample-display.h"
#include "file_dialogs.h"
#include "driver.h"
#include "notes.h"

#include "view_pixmaps.h"

/* Default initial dimensions.
 * 
 * Golden ratio. Oath.
 * (sqrt(5)-1)/2 = 0.61803398874989484820
 * 2/(sqrt(5)-1) = 1.61803398874989484820
 *
 * Keep width:height ratio equal to one of these
 * for pleasing dimensions.
 */
#define VIEW_MIN_WIDTH 197
#define VIEW_MAX_WIDTH 517
#define VIEW_DEFAULT_HEIGHT 320

#define NO_TIME ""

extern GList * plugins;

/* Global */
sw_view * last_tmp_view = NULL; /* last used tmp_view */


/* proc_instance:
 * a type for applying a procedure to a sample
 */
typedef struct _sw_proc_instance sw_proc_instance;

struct _sw_proc_instance {
  sw_proc * proc;
  sw_view * view;
};

static sw_proc_instance *
sw_proc_instance_new (sw_proc * proc, sw_view * view)
{
  sw_proc_instance * pi;

  /* XXX: where to clean this up? */
  pi = g_malloc (sizeof (sw_proc_instance));
  pi->proc = proc;
  pi->view = view;

  return pi;
}

static void
apply_procedure_cb (GtkWidget * widget, gpointer data)
{
  sw_proc_instance * pi = (sw_proc_instance *)data;
  sw_proc * proc = pi->proc;
  sw_sample * sample = pi->view->sample;
  sw_param_set pset;

  if (proc->nr_params == 0) {
    pset = NULL;
    proc->proc_apply (sample, pset);
  } else {
    pset = sw_param_set_new (proc);
    proc->proc_suggest (sample, pset);
    create_param_set_adjuster (proc, pi->view, pset);
  }

}

static void
create_proc_menuitem (sw_proc * proc, sw_view * view,
		      GtkWidget * submenu, GtkAccelGroup * accel_group)
{
  sw_proc_instance * pi;
  GtkWidget * menuitem;

  pi = sw_proc_instance_new (proc, view);

  menuitem = gtk_menu_item_new_with_label(proc->proc_name);
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(apply_procedure_cb), pi);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      proc->accel_key, proc->accel_mods,
			      GTK_ACCEL_VISIBLE);
}

/*
 * Populate a GtkMenu or GtkMenubar m
 */
static void
create_view_menu (sw_view * view, GtkWidget * m)
{
  GtkWidget * menuitem;
  GtkWidget * submenu, *subsubmenu;
  GtkAccelGroup *accel_group;
  SampleDisplay * s = SAMPLE_DISPLAY(view->v_display);
  void *append_func(GtkWidget * widget, GtkWidget * child);

  /* Plugin handling */
  GList * gl;
  sw_proc * proc;

#define MENU_APPEND(w,c) \
  if (GTK_IS_MENU_BAR(##w##)) {                               \
    gtk_menu_bar_append(GTK_MENU_BAR(##w##), ##c##);          \
  } else if (GTK_IS_MENU(##w##)) {                            \
    gtk_menu_append(GTK_MENU(##w##), ##c##);                  \
  }

  /* Create a GtkAccelGroup and add it to the window. */
  accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group (GTK_WINDOW(view->v_window), accel_group);

  /* File */
  menuitem = gtk_menu_item_new_with_label(_("File"));
  MENU_APPEND(m, menuitem);
  gtk_widget_show(menuitem);
  submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

  menuitem = gtk_menu_item_new_with_label(_("New"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(sample_new_empty_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_n, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label(_("Open..."));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(sample_load_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_o, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label(_("Save"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(sample_save_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_s, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label(_("Save As..."));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(sample_save_as_cb), s);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Revert"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(sample_revert_cb), s);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Close"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(view_close_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_w, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label(_("Quit"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(exit_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_q, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  /* Edit */
  menuitem = gtk_menu_item_new_with_label(_("Edit"));
  MENU_APPEND(m, menuitem);
  gtk_widget_show(menuitem);
  submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

  menuitem = gtk_menu_item_new_with_label(_("Undo"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(undo_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_z, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);


  menuitem = gtk_menu_item_new_with_label(_("Redo"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(redo_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_r, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Delete"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(delete_cb), s);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Cut"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(cut_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_x, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label(_("Copy"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(copy_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_c, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label(_("Paste"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(paste_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_v, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label(_("Paste as New"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(paste_as_new_cb), s);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Clear"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(clear_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_k, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);


  /* Select */
  menuitem = gtk_menu_item_new_with_label(_("Select"));
  MENU_APPEND(m, menuitem);
  gtk_widget_show(menuitem);
  submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

  menuitem = gtk_menu_item_new_with_label(_("Invert"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(select_invert_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_i, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label(_("All"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(select_all_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_a, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label(_("None"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(select_none_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_a, GDK_SHIFT_MASK|GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  /* View */
  menuitem = gtk_menu_item_new_with_label(_("View"));
  MENU_APPEND(m, menuitem);
  gtk_widget_show(menuitem);
  submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

  menuitem = gtk_menu_item_new_with_label(_("Zoom In"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(zoom_in_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_equal, GDK_NONE,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label(_("Zoom Out"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(zoom_out_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_minus, GDK_NONE,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label(_("Zoom to selection"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(zoom_to_sel_cb), s);
  gtk_widget_show(menuitem);

#if 0
  menuitem = gtk_menu_item_new_with_label(_("Left"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(zoom_left_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_Left, GDK_NONE,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label(_("Right"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(zoom_right_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_Right, GDK_NONE,
			      GTK_ACCEL_VISIBLE);
#endif

  menuitem = gtk_menu_item_new_with_label(_("Zoom"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);
  subsubmenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), subsubmenu);

  menuitem = gtk_menu_item_new_with_label(_("50%"));
  gtk_menu_append(GTK_MENU(subsubmenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(zoom_2_1_cb), s);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("100%"));
  gtk_menu_append(GTK_MENU(subsubmenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(zoom_1_1_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_1, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);


  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("New View"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(view_new_all_cb), s);
  gtk_widget_show(menuitem);

  /* Sample */
  menuitem = gtk_menu_item_new_with_label(_("Sample"));
  MENU_APPEND(m, menuitem);
  gtk_widget_show(menuitem);
  submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

  menuitem = gtk_menu_item_new_with_label(_("Duplicate"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(sample_new_copy_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_d, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  /* Filters */
  menuitem = gtk_menu_item_new_with_label(_("Filters"));
  MENU_APPEND(m, menuitem);
  gtk_widget_show(menuitem);
  submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

  /* Filter plugins */
  for (gl = plugins; gl; gl = gl->next) {
    proc = (sw_proc *)gl->data;

    create_proc_menuitem (proc, view, submenu, accel_group);
  }

  /* Playback */
  menuitem = gtk_menu_item_new_with_label(_("Playback"));
  MENU_APPEND(m, menuitem);
  gtk_widget_show(menuitem);
  submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

  menuitem = gtk_menu_item_new_with_label(_("Play sample"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(play_view_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_space, GDK_NONE,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label(_("Loop sample"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(play_view_all_loop_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_space, GDK_SHIFT_MASK,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label(_("Play selection"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(play_view_sel_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_space, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_label(_("Loop selection"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(play_view_sel_loop_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_space, GDK_SHIFT_MASK | GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Note"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);
  subsubmenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), subsubmenu);

#define PVAm(sn,n,k)                                                     \
  menuitem = gtk_menu_item_new_with_label(_(##sn##));                    \
  gtk_menu_append(GTK_MENU(subsubmenu), menuitem);                       \
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",                   \
		     GTK_SIGNAL_FUNC(play_view_all_##n##_cb), s);        \
  gtk_widget_show(menuitem);                                             \
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,         \
			      GDK_##k##, GDK_NONE,                       \
			      GTK_ACCEL_VISIBLE);

  PVAm("C3", C3, z);
  PVAm("C#3",Cs3,s);
  PVAm("D3", D3, x);
  PVAm("Eb3",Ds3,d);
  PVAm("E3", E3, c);
  PVAm("F3", F3, v);
  PVAm("F#3",Fs3,g);
  PVAm("G3", G3, b);
  PVAm("G#3",Gs3,h);
  PVAm("A3", A3, n);
  PVAm("Bb3",As3,j);
  PVAm("B3", B3, m);

   PVAm("C4", C4, q);

  PVAm("C#4",Cs4,2);
  PVAm("D4", D4, w);
  PVAm("Eb4",Ds4,3);
  PVAm("E4", E4, e);
  PVAm("F4", F4, r);
  PVAm("F#4",Fs4,5);
  PVAm("G4", G4, t);
  PVAm("G#4",Gs4,6);
  PVAm("A4", A4, y);
  PVAm("Bb4",As4,7);
  PVAm("B4", B4, u);

  PVAm("C5", C5, i);
  PVAm("C#5",Cs5,9);
  PVAm("D5", D5, o);
  PVAm("D#5",Ds5,0);
  PVAm("E5", E5, p);
#if 0
  PVAm("F5", F5, bracketleft);
  PVAm("F#5",Fs5);
  PVAm("G5", G5, );
  PVAm("G#5",Gs5, );
  PVAm("A5", A5, );
  PVAm("Bb5",As5, );
  PVAm("B5", B5, );
#endif

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Stop"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
		     GTK_SIGNAL_FUNC(stop_playback_cb), s);
  gtk_widget_show(menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", accel_group,
			      GDK_Escape, GDK_NONE,
			      GTK_ACCEL_VISIBLE);
}

static void
view_destroy_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;

  sample_display_stop_marching_ants (SAMPLE_DISPLAY(view->v_display));

  sample_remove_view(view->sample, view);
}

static void
view_vol_changed_cb (GtkWidget * widget, gpointer data)
{
  sw_view * v = (sw_view *)data;

  v->v_vol = 1.0 - GTK_ADJUSTMENT (v->v_vol_adj)->value;
}

static void
view_set_pos_indicator_cb (GtkWidget * widget, gpointer data)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(data);
  sw_view * view = sd->view;

#define BUF_LEN 16
  char buf[BUF_LEN];

  if (sd->mouse_offset >= 0) {
    snprint_time (buf, BUF_LEN,
		  frames_to_time (view->sample->sdata->format,
				  sd->mouse_offset));
    gtk_label_set_text (GTK_LABEL(view->v_pos), buf);
  } else {
    gtk_label_set_text (GTK_LABEL(view->v_pos), NO_TIME);
  }


#undef BUF_LEN
}

static gint
menu_button_handler (GtkWidget * widget, GdkEvent * event)
{
  GtkMenu * menu;
  GdkEventButton *event_button;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  menu = GTK_MENU (widget);
  
  if (event->type == GDK_BUTTON_PRESS) {
    event_button = (GdkEventButton *) event;
    gtk_menu_popup (menu, NULL, NULL, NULL, NULL, 
		    event_button->button, event_button->time);
    return TRUE;
  }
  
  return FALSE;
}

sw_view *
view_new(sw_sample * sample, glong start, glong end, gfloat vol)
{
  sw_view * view;

  gint win_width;

  GtkWidget * window;
  GtkWidget * main_vbox;
  GtkWidget * table;
  GtkWidget * hbox;
  GtkWidget * vbox;
  GtkWidget * hruler;
  GtkWidget * scrollbar;
  GtkObject * vol_adj;
  GtkWidget * vol_vscale;
  GtkWidget * menu_button;
  GtkWidget * arrow;
  GtkWidget * pixmap;
  GtkWidget * play_button, * stop_button;
  GtkWidget * frame;
  GtkWidget * label;

  view = g_malloc (sizeof(sw_view));

  view->sample = sample;
  view->v_start = start;
  view->v_end = end;
  view->v_vol = vol;

  win_width = CLAMP (sample->sdata->s_length / 150,
		     VIEW_MIN_WIDTH, VIEW_MAX_WIDTH);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW(window),
			       win_width, VIEW_DEFAULT_HEIGHT);
  gtk_widget_show(window);
  view->v_window = window;

  gtk_signal_connect (GTK_OBJECT(window), "destroy",
		      GTK_SIGNAL_FUNC(view_destroy_cb), view);

  main_vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(window), main_vbox);
  gtk_widget_show (main_vbox);

  view->v_menubar = gtk_menu_bar_new ();
  gtk_box_pack_start (GTK_BOX(main_vbox), view->v_menubar, FALSE, TRUE, 0);
  gtk_widget_show (view->v_menubar);

  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_col_spacing (GTK_TABLE(table), 0, 1);
  gtk_table_set_col_spacing (GTK_TABLE(table), 1, 2);
  gtk_table_set_row_spacing (GTK_TABLE(table), 0, 1);
  gtk_table_set_row_spacing (GTK_TABLE(table), 1, 2);
  gtk_container_border_width (GTK_CONTAINER(table), 2);
  gtk_box_pack_start (GTK_BOX(main_vbox), table, TRUE, TRUE, 0);
  gtk_widget_show (table);

  /* menu button */
  menu_button = gtk_button_new ();
  gtk_table_attach (GTK_TABLE(table), menu_button,
		    0, 1, 0, 1,
		    GTK_FILL, GTK_FILL,
		    0, 0);
  gtk_widget_show (menu_button);


  arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (menu_button), arrow);
  gtk_widget_show (arrow);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE(table), vbox,
		    0, 1, 1, 3,
		    GTK_FILL, GTK_EXPAND|GTK_FILL|GTK_SHRINK,
		    0, 0);
  gtk_widget_show (vbox);

  /* playback buttons */
  play_button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX(vbox), play_button, FALSE, FALSE, 0);
  gtk_widget_show (play_button);

  pixmap = create_widget_from_xpm (window, play_xpm);
  gtk_widget_show (pixmap);
  gtk_container_add (GTK_CONTAINER(play_button), pixmap);

  stop_button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX(vbox), stop_button, FALSE, FALSE, 0);
  gtk_widget_show (stop_button);

  pixmap = create_widget_from_xpm (window, stop_xpm);
  gtk_widget_show (pixmap);
  gtk_container_add (GTK_CONTAINER(stop_button), pixmap);


  /* hruler */
  hruler = gtk_hruler_new ();
  gtk_table_attach (GTK_TABLE(table), hruler,
		    1, 2, 0, 1,
		    GTK_EXPAND|GTK_FILL|GTK_SHRINK, GTK_FILL,
		    0, 0);
  gtk_ruler_set_range (GTK_RULER(hruler),
		       start, end,
		       start, end);
  gtk_widget_show (hruler);
  view->v_hruler = hruler;

  gtk_signal_connect_object (GTK_OBJECT (table), "motion_notify_event",
                             (GtkSignalFunc) GTK_WIDGET_CLASS (GTK_OBJECT (hruler)->klass)->motion_notify_event,
                             GTK_OBJECT (hruler));

  /* v_display */
  view->v_display = sample_display_new();
  gtk_table_attach (GTK_TABLE(table), view->v_display,
		    1, 2, 1, 2,
		    GTK_EXPAND|GTK_FILL|GTK_SHRINK,
		    GTK_EXPAND|GTK_FILL|GTK_SHRINK,
		    0, 0);

  sample_display_set_view(SAMPLE_DISPLAY(view->v_display), view);

  gtk_signal_connect (GTK_OBJECT(view->v_display), "selection-changed",
		      GTK_SIGNAL_FUNC(sd_sel_changed_cb), view);
  gtk_signal_connect (GTK_OBJECT(view->v_display), "window-changed",
		      GTK_SIGNAL_FUNC(sd_win_changed_cb), view);

  gtk_widget_show(view->v_display);

  /* scrollbar */
  view->v_adj = gtk_adjustment_new((gfloat)start,            /* value */
				   (gfloat)0.0,              /* start */
				   (gfloat)sample->sdata->s_length, /* end */
				   1.0,                      /* step_incr */
				   (gfloat)(end-start),      /* page_incr */
				   (gfloat)(end-start)       /* page_size */
				   );

  gtk_signal_connect (GTK_OBJECT(view->v_adj), "value-changed",
		      GTK_SIGNAL_FUNC(adj_changed_cb), view);
  gtk_signal_connect (GTK_OBJECT(view->v_adj), "changed",
		      GTK_SIGNAL_FUNC(adj_changed_cb), view);

  scrollbar = gtk_hscrollbar_new(GTK_ADJUSTMENT(view->v_adj));
  gtk_table_attach (GTK_TABLE(table), scrollbar,
		    1, 2, 2, 3,
		    GTK_EXPAND|GTK_FILL|GTK_SHRINK, GTK_FILL,
		    0, 0);
  gtk_widget_show(scrollbar);

  /* volume */
  vol_adj = gtk_adjustment_new ( 0.0,  /* value */
				 0.0,  /* lower */
				 1.0,  /* upper */
				 0.1,  /* step incr */
				 0.1,  /* page incr */
				 0.1   /* page size */
				 );
  view->v_vol_adj = vol_adj;

  
  vol_vscale = gtk_vscale_new (GTK_ADJUSTMENT(vol_adj));
  gtk_table_attach (GTK_TABLE(table), vol_vscale,
		   2, 3, 0, 3,
		   GTK_FILL, GTK_EXPAND|GTK_FILL|GTK_SHRINK,
		   0, 0);
  gtk_scale_set_draw_value (GTK_SCALE(vol_vscale), FALSE);
  gtk_range_set_update_policy (GTK_RANGE(vol_vscale), GTK_UPDATE_CONTINUOUS);
  gtk_widget_show (vol_vscale);

  gtk_signal_connect (GTK_OBJECT(vol_adj), "value_changed",
		      GTK_SIGNAL_FUNC(view_vol_changed_cb), view);

  hbox = gtk_hbox_new (FALSE, 0);
#if 0
  gtk_table_attach (GTK_TABLE(table), hbox,
		   0, 3, 3, 4,
		   GTK_FILL|GTK_SHRINK, GTK_FILL,
		   0, 0);
#endif
  gtk_box_pack_start (GTK_BOX(main_vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  /* position indicator */
  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX(hbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new (NO_TIME);
  gtk_container_add (GTK_CONTAINER(frame), label);
  gtk_widget_show (label);
  view->v_pos = label;

  /* status */
  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX(hbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  label = gtk_label_new ("Sweep " VERSION);
  gtk_container_add (GTK_CONTAINER(frame), label);
  gtk_widget_show (label);
  view->v_status = label;

  /* Had to wait until view->v_display was created before
   * setting the menus up
   */

  view->v_menu = gtk_menu_new ();
  create_view_menu (view, view->v_menu);

  create_view_menu (view, view->v_menubar);

  gtk_signal_connect_object (GTK_OBJECT(menu_button),
			     "button_press_event",
			     GTK_SIGNAL_FUNC(menu_button_handler),
			     GTK_OBJECT(view->v_menu));

  /* Had to wait till view->v_display was created to set these up */

  gtk_signal_connect (GTK_OBJECT(play_button), "clicked",
		      GTK_SIGNAL_FUNC(play_view_cb), view->v_display);
  gtk_signal_connect (GTK_OBJECT(stop_button), "clicked",
		      GTK_SIGNAL_FUNC(stop_playback_cb), view->v_display);

  gtk_signal_connect (GTK_OBJECT(view->v_display),
		      "mouse-offset-changed",
		      GTK_SIGNAL_FUNC(view_set_pos_indicator_cb),
		      view->v_display);
		      
#if 0
  gtk_signal_connect_object (GTK_OBJECT(view->v_display),
			     "motion_notify_event",
			     GTK_SIGNAL_FUNC(view_set_pos_indicator_cb),
			     GTK_OBJECT(view->v_display));
#endif

  if (sample->sdata->sels)
    sample_display_start_marching_ants (SAMPLE_DISPLAY(view->v_display));

  view_refresh_title(view);

  view_default_status(view);

  return view;
}

sw_view *
view_new_all(sw_sample * sample, gfloat vol)
{
  return view_new(sample, 0, sample->sdata->s_length, vol);
}

/*
 * view_set_ends (v, start, end)
 *
 * set the endpoints shown by this view.
 */
void
view_set_ends (sw_view * view, glong start, glong end)
{
  GtkAdjustment * adj = GTK_ADJUSTMENT(view->v_adj);

  /*
  if(start < 0) start = 0;
  if(end > view->sample->sdata->s_length) end = view->sample->sdata->s_length;
  */

  adj->value = (gfloat)start;
  adj->page_increment = (gfloat)(end - start);
  adj->page_size = (gfloat)(end - start);

  gtk_adjustment_changed(adj);

  gtk_ruler_set_range (GTK_RULER(view->v_hruler),
		       start, end,
		       start, end);

  sample_display_set_window(SAMPLE_DISPLAY(view->v_display), start, end);

  view_refresh_title(view);
  view_refresh_display(view);
}

void
view_set_playmarker (sw_view * view, int offset)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(view->v_display);

  sample_display_set_playmarker(sd, offset);
}

void
view_close (sw_view * view)
{
  gtk_widget_destroy(view->v_window);
  g_free(view);
}

void
view_volume_increase (sw_view * view)
{
  GtkAdjustment * adj = GTK_ADJUSTMENT(view->v_vol_adj);

  adj->value -= 0.1;
  if (adj->value <= 0.0) adj->value = 0.0;

  gtk_adjustment_value_changed (adj);
}

void
view_volume_decrease (sw_view * view)
{
  GtkAdjustment * adj = GTK_ADJUSTMENT(view->v_vol_adj);

  adj->value += 0.1;
  if (adj->value >= 1.0) adj->value = 1.0;

  gtk_adjustment_value_changed (adj);
}

void
view_refresh_title (sw_view * view)
{
  sw_sample * s = (sw_sample *)view->sample;

#define BUF_LEN 256
  char buf[BUF_LEN];


  snprintf(buf, BUF_LEN,
	   "%s (%dHz %s) %0ld%%",
	   s->sdata->filename ? s->sdata->filename : _("Untitled"),
	   s->sdata->format->f_rate,
	   s->sdata->format->f_channels == 1 ? _("Mono") : _("Stereo"),
	   100 * (view->v_end - view->v_start) / s->sdata->s_length);

  gtk_window_set_title (GTK_WINDOW(view->v_window), buf);
#undef BUF_LEN
}

void
view_default_status (sw_view * view)
{
  sw_sample * s = (sw_sample *)view->sample;
  sw_sdata * sdata = s->sdata;

#define BYTE_BUF_LEN 16
  char byte_buf[BYTE_BUF_LEN];

#define TIME_BUF_LEN 16
  char time_buf[TIME_BUF_LEN];

#define BUF_LEN 48
  char buf [BUF_LEN];

  snprint_bytes (byte_buf, BYTE_BUF_LEN,
		 frames_to_bytes (sdata->format, sdata->s_length));
  
  snprint_time (time_buf, TIME_BUF_LEN,
		frames_to_time (sdata->format, sdata->s_length));

  snprintf (buf, BUF_LEN,
	    "%s [%s]",
	    byte_buf, time_buf);

  gtk_label_set_text (GTK_LABEL(view->v_status), buf);

#undef BUF_LEN
#undef BYTE_BUF_LEN
#undef TIME_BUF_LEN
}

void
view_refresh_hruler (sw_view * v)
{
  gtk_ruler_set_range (GTK_RULER(v->v_hruler),
		       v->v_start, v->v_end,
		       v->v_start, v->v_end);
}

void
view_refresh_display (sw_view * v)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(v->v_display);

  sample_display_refresh(sd);
}

void
view_refresh_adjustment (sw_view * v)
{
  GtkAdjustment * adj = GTK_ADJUSTMENT(v->v_adj);

  adj->upper = (gfloat)v->sample->sdata->s_length;
  if (adj->page_size > adj->upper - adj->value)
    adj->page_size = adj->upper - adj->value;

#if 0
  if (v->v_end > v->sample->sdata->s_length)
    v->v_end = v->sample->sdata->s_length;
#endif 

  gtk_adjustment_changed (adj);
}

void
view_fix_adjustment (sw_view * v)
{
  GtkAdjustment * adj = GTK_ADJUSTMENT(v->v_adj);

  adj->value = (gfloat)v->v_start;
  adj->lower = (gfloat)0.0;
  adj->upper = (gfloat)v->sample->sdata->s_length;
  adj->page_increment = (gfloat)(v->v_end - v->v_start);
  adj->page_size = (gfloat)(v->v_end - v->v_start);

  gtk_adjustment_changed (adj);
}

void
view_refresh (sw_view * v)
{
  GtkAdjustment * adj = GTK_ADJUSTMENT(v->v_adj);

  if (adj->upper != v->sample->sdata->s_length) {
    view_refresh_adjustment (v);
    view_refresh_title (v);
    view_default_status (v);
  }
  view_refresh_display (v);
}

void
view_sink_last_tmp_view (void)
{
  if (!last_tmp_view) return;

  sample_display_sink_tmp_sel(SAMPLE_DISPLAY(last_tmp_view->v_display));
}

void
view_clear_last_tmp_view (void)
{
  if (!last_tmp_view) return;

  sample_display_clear_sel(SAMPLE_DISPLAY(last_tmp_view->v_display));
}
