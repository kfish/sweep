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

/*
 * Thu Oct 22 2000 - Added 1:1 and Normal zooms from
 *                   Steve Harris <steve@totl.net>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_types.h>
#include <sweep/sweep_typeconvert.h>
#include <sweep/sweep_sample.h>
#include <sweep/sweep_undo.h>

#include "view.h"

#include "about_dialog.h"
#include "sample.h"
#include "callbacks.h"
#include "channelops.h"
#include "interface.h"
#include "print.h"
#include "param.h"
#include "record.h"
#include "sample-display.h"
#include "file_dialogs.h"
#include "question_dialogs.h"
#include "driver.h"
#include "notes.h"
#include "db_ruler.h"
#include "time_ruler.h"
#include "cursors.h"
#include "head.h"
#include "view_pixmaps.h"

/*#define DEBUG*/

#define USER_GTKRC

/*#define SCROLL_SMOOTHLY*/

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
#if 0
#define VIEW_MAX_WIDTH 517
#define VIEW_DEFAULT_HEIGHT 320
#else
#define VIEW_MAX_WIDTH 1034
#define VIEW_DEFAULT_HEIGHT_PER_CHANNEL 320
#endif

#define DEFAULT_MIN_ZOOM 8

#define NO_TIME ""

#define NOREADY(w) \
  (view->noready_widgets = g_list_append (view->noready_widgets, (w)))

#define NOMODIFY(w) \
  (view->nomodify_widgets = g_list_append (view->nomodify_widgets, (w)))

#define NOALLOC(w) \
  (view->noalloc_widgets = g_list_append (view->noalloc_widgets, (w)))


#ifdef HAVE_LIBSAMPLERATE
void samplerate_dialog_new_cb (GtkWidget * widget, gpointer data);
#endif

static GtkWidget * create_view_menu_item(GtkWidget * menu, gchar * label,
                                                  gchar * accel_path, gpointer callback, gboolean nomodify,
												  guint accel_key, GdkModifierType accel_mods, gpointer user_data);

void
view_set_vzoom (sw_view * view, sw_audio_t low, sw_audio_t high);

extern GList * plugins;
extern GdkCursor * sweep_cursors[];

extern GtkStyle * style_wb;
extern GtkStyle * style_LCD;
extern GtkStyle * style_light_grey;
extern GtkStyle * style_green_grey;
extern GtkStyle * style_red_grey;
extern GtkStyle * style_dark_grey;

/* Global */
sw_view * last_tmp_view = NULL; /* last used tmp_view */

/* proc_instance:
 * a type for applying a procedure to a sample
 */
typedef struct _sw_proc_instance sw_proc_instance;

struct _sw_proc_instance {
  sw_procedure * proc;
  sw_view * view;
};

static sw_proc_instance *
sw_proc_instance_new (sw_procedure * proc, sw_view * view)
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
  sw_procedure * proc = pi->proc;
  sw_sample * sample = pi->view->sample;
  sw_param_set pset;

  if (proc->nr_params == 0) {
    pset = NULL;
    proc->apply (sample, pset, proc->custom_data);
  } else {
    pset = sw_param_set_new (proc);
    if (proc->suggest)
      proc->suggest (sample, pset, proc->custom_data);
    create_param_set_adjuster (proc, pi->view, pset);
  }

}

static void
create_proc_menuitem (sw_procedure * proc, sw_view * view,
		      GtkWidget * submenu, GtkAccelGroup * accel_group)
{
  sw_proc_instance * pi;
  GtkWidget * menuitem;

  pi = sw_proc_instance_new (proc, view);

  menuitem = gtk_menu_item_new_with_label(_(proc->name));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(apply_procedure_cb), pi);
  gtk_widget_show(menuitem);
/* these accels are not editable */
 /*   gtk_widget_add_accelerator (menuitem, "activate", accel_group,
				proc->accel_key, proc->accel_mods,
				GTK_ACCEL_VISIBLE); */
}

static GtkWidget *
create_proc_menu (sw_view * view, GtkAccelGroup * accel_group)
{
  GtkWidget * menu, * submenu, * menuitem = NULL, * label, * hbox;
  GList * gl;
  sw_procedure * proc;
  gboolean use_submenus = FALSE;
  gint i = 0, li = 0;
  gchar first_name[32], last_name[32];
  gchar * title;

  menu = gtk_menu_new ();

  if (g_list_length (plugins) > 10) {
    use_submenus = TRUE;
  }

  submenu = menu;

#if 0
  first_name[4] = '\0';
  last_name[4] = '\0';
#endif

  if (plugins)
    /*strncpy (first_name, ((sw_procedure *)plugins->data)->name, 4);*/
    sscanf (_(((sw_procedure *)plugins->data)->name), "%s", first_name);

  /* Filter plugins */
  for (gl = plugins; gl; gl = gl->next) {
    proc = (sw_procedure *)gl->data;

    if (use_submenus && ((i % 10) == 0)) {
      if (menuitem) {
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER(menuitem), hbox);
	gtk_widget_show (hbox);

	title = g_strdup_printf ("%s  ...  %s", first_name, last_name);
	label = gtk_label_new (title);
	gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
	/*menuitem = gtk_menu_item_new_with_label (title);*/
	gtk_widget_show (label);

	/*strncpy (first_name, proc->name, 4);*/
	sscanf (_(proc->name), "%s", first_name);
	li = i;
      }

      menuitem = gtk_menu_item_new ();
      gtk_menu_append (GTK_MENU(menu), menuitem);
      gtk_widget_show (menuitem);
      submenu = gtk_menu_new ();
      gtk_menu_item_set_submenu (GTK_MENU_ITEM(menuitem), submenu);
    }

    create_proc_menuitem (proc, view, submenu, accel_group);
    /*strncpy (last_name, proc->name, 4);*/
    sscanf (_(proc->name), "%s", last_name);
    i++;
  }

  if (menuitem) {
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER(menuitem), hbox);
    gtk_widget_show (hbox);

    label = gtk_label_new (g_strdup_printf ("%s ... %s", first_name,
					    last_name));
    gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);
  }

  return menu;
}

static void view_store_cb (GtkWidget * widget, gpointer data);
static void view_retrieve_cb (GtkWidget * widget, gpointer data);

static GtkWidget *
view_refresh_channelops_menu (sw_view * view)
{
  GtkWidget * submenu, * menuitem = NULL;
  GList * gl;
  int old_channels, channels;
  GtkAccelGroup *accel_group;

  channels = view->sample->sounddata->format->channels;

  if (view->channelops_submenu != NULL) {
    old_channels = GPOINTER_TO_INT
      (g_object_get_data (G_OBJECT(view->channelops_submenu), "default"));

    /* If there's no need to change the submenu, don't */
    if ((old_channels == channels) ||
	(old_channels > 2 && channels > 2)) {
      return view->channelops_submenu;
    }
  }
  
  /* Remove references to old channelops widgets from sensitivity updates */
  for (gl = view->channelops_widgets; gl; gl = gl->next) {
    view->nomodify_widgets = g_list_remove (view->nomodify_widgets, gl->data);
#if 0
    view->noready_widgets = g_list_remove (view->noready_widgets, gl->data);
    view->noalloc_widgets = g_list_remove (view->noalloc_widgets, gl->data);
#endif
  }
  g_list_free (view->channelops_widgets);
  view->channelops_widgets = NULL;

  /* Create the new channelops submenu */
  submenu = gtk_menu_new ();
  accel_group = GTK_ACCEL_GROUP(g_object_get_data(G_OBJECT(view->window), "accel_group"));
  gtk_menu_set_accel_group (GTK_MENU (submenu), accel_group);

  
  g_object_set_data (G_OBJECT(submenu), "default", GINT_TO_POINTER(channels));
			    
  if (channels == 1) {
	menuitem = create_view_menu_item (submenu, _("Duplicate to stereo"), 
                                       "<Sweep-View>/Sample/Channels/Duplicate to stereo",
                                       dup_stereo_cb, TRUE,
                                       0, 0, view); 
      view->channelops_widgets =
      g_list_append (view->channelops_widgets, menuitem);

    menuitem = create_view_menu_item (submenu, _("Duplicate to multichannel"), 
                                       "<Sweep-View>/Channels/Duplicate to multichannel",
                                       dup_channels_dialog_new_cb, TRUE,
                                       0, 0, view);  
    view->channelops_widgets =
      g_list_append (view->channelops_widgets, menuitem);
	  
  }

  if (channels == 2) {
	  
	menuitem = create_view_menu_item (submenu, _("Swap left and right"), 
                                       "<Sweep-View>/Sample/Channels/Swap left and right",
                                       stereo_swap_cb, TRUE,
                                       0, 0, view); 
    view->channelops_widgets =
      g_list_append (view->channelops_widgets, menuitem);

	  
	menuitem = create_view_menu_item (submenu, _("Remove left channel"), 
                                       "<Sweep-View>/Sample/Channels/Remove left channel",
                                       remove_left_cb, TRUE,
                                       0, 0, view);
    view->channelops_widgets =
      g_list_append (view->channelops_widgets, menuitem);

	  
	menuitem = create_view_menu_item (submenu, _("Remove right channel"), 
                                       "<Sweep-View>/Sample/Channels/Remove right channel",
                                       remove_right_cb, TRUE,
                                       0, 0, view);	  
    view->channelops_widgets =
      g_list_append (view->channelops_widgets, menuitem);

  }

  if (channels > 1) {
	  
	menuitem = create_view_menu_item (submenu, _("Mix down to mono"), 
                                       "<Sweep-View>/Sample/Channels/Mix down to mono",
                                       mono_mixdown_cb, TRUE,
                                       0, 0, view);		  
    view->channelops_widgets =
      g_list_append (view->channelops_widgets, menuitem);
  }

  	
  menuitem = create_view_menu_item (submenu, _("Add/Remove channels"), 
                                       "<Sweep-View>/Sample/Channels/Add/Remove channels",
                                       channels_dialog_new_cb, TRUE,
                                       0, 0, view);	

  view->channelops_widgets =
    g_list_append (view->channelops_widgets, menuitem);
  

  gtk_menu_item_set_submenu (GTK_MENU_ITEM(view->channelops_menuitem),
			     submenu);
  
  view->channelops_submenu = submenu;

  return submenu;
}

/*
 * Convenience function to Create and setup individual menuitems
 */

static GtkWidget * create_view_menu_item(GtkWidget * menu, gchar * label,
                                                  gchar * accel_path, gpointer callback, gboolean nomodify,
												  guint accel_key, GdkModifierType accel_mods, gpointer user_data)
{
	GtkWidget * menuitem;
	
	menuitem = gtk_menu_item_new_with_label(label);
	/* register accel path enabling runtime changes by the user */
	gtk_menu_item_set_accel_path (GTK_MENU_ITEM(menuitem), accel_path);
	gtk_menu_append(GTK_MENU(menu), menuitem);
	 g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(callback), user_data);
	
    /* register default key binding (if one is supplied */
	if (accel_key)
		 gtk_accel_map_add_entry  (accel_path, accel_key, accel_mods);

/*	if (nomodify)
	  NOMODIFY(menuitem);
	*/
	gtk_widget_show(menuitem);
		
	return menuitem;							  
}

/*
 * Populate a GtkMenu or GtkMenubar m
 */
static GtkAccelGroup *
create_view_menu (sw_view * view, GtkWidget * m)
{
  GtkWidget * menuitem;
  GtkWidget * submenu, *subsubmenu;
  GtkAccelGroup *accel_group;
  SampleDisplay * s = SAMPLE_DISPLAY(view->display);


#define MENU_APPEND(w,c) \
  if (GTK_IS_MENU_BAR(w)) {                           \
    gtk_menu_bar_append(GTK_MENU_BAR(w), c);          \
  } else if (GTK_IS_MENU(w)) {                        \
    gtk_menu_append(GTK_MENU(w), c);                  \
  }

  /* Create a GtkAccelGroup and add it to the window. */
  accel_group = gtk_accel_group_new();
  g_object_set_data(G_OBJECT(view->window), "accel_group", accel_group);
#if 0
  if (GTK_IS_MENU(m))
    gtk_window_add_accel_group (GTK_WINDOW(view->window), accel_group);
#endif

  /* File */
  menuitem = gtk_menu_item_new_with_label(_("File"));
  MENU_APPEND(m, menuitem);
  gtk_widget_show(menuitem);
  submenu = gtk_menu_new();
  gtk_menu_set_accel_group (GTK_MENU (submenu), accel_group);

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

  create_view_menu_item (submenu, _("New ..."), "<Sweep-View>/File/New ...",
                                                  sample_new_empty_cb, FALSE,
												  GDK_n, GDK_CONTROL_MASK, view);
   
  create_view_menu_item (submenu, _("Open ..."), "<Sweep-View>/File/Open ...",
                                                  sample_load_cb, FALSE,
												  GDK_o, GDK_CONTROL_MASK, view);
  
  create_view_menu_item (submenu, _("Save"), "<Sweep-View>/File/Save",
                                                  sample_save_cb, TRUE,
												  GDK_s, GDK_CONTROL_MASK, view);
												  
  create_view_menu_item (submenu, _("Save As ..."), "<Sweep-View>/File/Save As ...",
                                                  sample_save_as_cb, TRUE,
												  0, 0, view);

  create_view_menu_item (submenu, _("Revert"), "<Sweep-View>/File/Revert",
                                                  sample_revert_cb, TRUE,
												  0, 0, view);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);

  create_view_menu_item (submenu, _("Properties ..."), "<Sweep-View>/File/Properties ...",
                                                  show_info_dialog_cb, FALSE,
												  0, 0, view);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);
  
  create_view_menu_item (submenu, _("Close"), "<Sweep-View>/File/Close",
                                                  exit_cb, FALSE,
												  GDK_q, GDK_CONTROL_MASK, s);

  create_view_menu_item (submenu, _("Quit"), "<Sweep-View>/File/Quit",
                                                  view_close_cb, FALSE,
												  GDK_w, GDK_CONTROL_MASK, s);
												  

  /* Edit */
  menuitem = gtk_menu_item_new_with_label(_("Edit"));
  MENU_APPEND(m, menuitem);
  gtk_widget_show(menuitem);
  submenu = gtk_menu_new();
  gtk_menu_set_accel_group (GTK_MENU (submenu), accel_group);
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM(menuitem), "<Sweep-View>/Edit");
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

  menuitem = create_view_menu_item (submenu, _("Cancel"), "<Sweep-View>/Edit/Cancel",
                                                  cancel_cb, TRUE,
												  GDK_Escape, GDK_BUTTON1_MASK, view);
  NOREADY(menuitem);
  
  create_view_menu_item (submenu, _("Undo"), "<Sweep-View>/Edit/Undo",
                                                  undo_cb, TRUE,
												  GDK_z, GDK_CONTROL_MASK, view);
  
  create_view_menu_item (submenu, _("Redo"), "<Sweep-View>/Edit/Redo",
                                                  redo_cb, TRUE,
												  GDK_r, GDK_CONTROL_MASK, view);
  
  create_view_menu_item (submenu, _("Show history ..."), "<Sweep-View>/Edit/Show history ...",
                                                  show_undo_dialog_cb, FALSE,
												  0, 0, view);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);
  
  create_view_menu_item (submenu, _("Delete"), "<Sweep-View>/Edit/Delete",
                                                  delete_cb, TRUE,
												  0, 0, view);
												  
  create_view_menu_item (submenu, _("Cut"), "<Sweep-View>/Edit/Cut",
                                                  cut_cb, TRUE,
												  GDK_x, GDK_CONTROL_MASK, view);												  
												  
  create_view_menu_item (submenu, _("Copy"), "<Sweep-View>/Edit/Copy",
                                                  copy_cb, TRUE,
												  GDK_c, GDK_CONTROL_MASK, view);	
 
  create_view_menu_item (submenu, _("Clear"), "<Sweep-View>/Edit/Clear",
                                                  clear_cb, TRUE,
												  0, 0, view);	

  create_view_menu_item (submenu, _("Crop"), "<Sweep-View>/Edit/Crop",
                                                  crop_cb, TRUE,
												  0, 0, view);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);

  create_view_menu_item (submenu, _("Paste: Insert"), "<Sweep-View>/Edit/Paste: Insert",
                                                  paste_cb, TRUE,
												  GDK_v, GDK_CONTROL_MASK, view);

  create_view_menu_item (submenu, _("Paste: Mix"), "<Sweep-View>/Edit/Paste: Mix",
                                                  paste_mix_cb, TRUE,
												  GDK_m, GDK_CONTROL_MASK, view);
												  
  create_view_menu_item (submenu, _("Paste: Crossfade"), "<Sweep-View>/Edit/Paste: Crossfade",
                                                  paste_xfade_cb, TRUE,
												  GDK_f, GDK_CONTROL_MASK, view);
												  
  create_view_menu_item (submenu, _("Paste as New"), "<Sweep-View>/Edit/Paste as New",
                                                  paste_as_new_cb, TRUE,
												  GDK_e, GDK_CONTROL_MASK, view);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);

  create_view_menu_item (submenu, _("Preview Cut/Cursor"), "<Sweep-View>/Edit/Preview Cut-Cursor",
                                                  preview_cut_cb, FALSE,
												  GDK_k, GDK_CONTROL_MASK, view);

  create_view_menu_item (submenu, _("Pre-roll to Cursor"), "<Sweep-View>/Edit/Pre-roll to Cursor",
                                                  preroll_cb, FALSE,
												  GDK_k, GDK_SHIFT_MASK|GDK_CONTROL_MASK, view);


  /* Select */
  menuitem = gtk_menu_item_new_with_label(_("Select"));
  MENU_APPEND(m, menuitem);
  gtk_widget_show(menuitem);
  submenu = gtk_menu_new();
  gtk_menu_set_accel_group (GTK_MENU (submenu), accel_group);

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

  create_view_menu_item (submenu, _("Invert"), "<Sweep-View>/Select/Invert",
                                                  select_invert_cb, TRUE,
												  GDK_i, GDK_CONTROL_MASK, s);

  create_view_menu_item (submenu, _("All"), "<Sweep-View>/Select/All",
                                                  select_all_cb, TRUE,
												  GDK_a, GDK_CONTROL_MASK, s);

  create_view_menu_item (submenu, _("None"), "<Sweep-View>/Select/None",
                                                  select_none_cb, TRUE,
												  GDK_a, GDK_SHIFT_MASK|GDK_CONTROL_MASK, s);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);

  create_view_menu_item (submenu, _("Halve"), "<Sweep-View>/Select/Halve",
                                                  selection_halve_cb, TRUE,
												  GDK_semicolon, GDK_BUTTON1_MASK, s); 
												  
  create_view_menu_item (submenu, _("Double"), "<Sweep-View>/Select/Double",
                                                  selection_double_cb, TRUE,
												  GDK_quoteright, GDK_BUTTON1_MASK, s); 
												  
  create_view_menu_item (submenu, _("Shift left"), "<Sweep-View>/Select/Shift left",
                                                  select_shift_left_cb, TRUE,
												  GDK_less, GDK_BUTTON1_MASK, s); 

  create_view_menu_item (submenu, _("Shift right"), "<Sweep-View>/Select/Shift right",
                                                  select_shift_right_cb, TRUE,
												  GDK_greater, GDK_BUTTON1_MASK, s); 

  /* View */
  menuitem = gtk_menu_item_new_with_label(_("View"));
  MENU_APPEND(m, menuitem);
  gtk_widget_show(menuitem);
  submenu = gtk_menu_new();
  gtk_menu_set_accel_group (GTK_MENU (submenu), accel_group);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

  menuitem = gtk_check_menu_item_new_with_label(_("Autoscroll: follow playback cursor"));
  gtk_menu_item_set_accel_path(GTK_MENU_ITEM(menuitem), "<Sweep-View>/View/Autoscroll: follow playback cursor");
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menuitem),
				  view->following);
  g_signal_connect (G_OBJECT(menuitem), "activate",
                    G_CALLBACK(follow_toggle_cb), view);
  gtk_widget_show(menuitem);
  view->follow_checkmenu = menuitem;

  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menuitem),
				  view->following);
  view->follow_checkmenu = menuitem;

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);

  create_view_menu_item (submenu, _("Center"), "<Sweep-View>/View/Center",
                                                  zoom_center_cb, FALSE,
												  GDK_slash, GDK_BUTTON1_MASK, s); 
												  
  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);
  
  create_view_menu_item (submenu, _("Zoom in"), "<Sweep-View>/View/Zoom in",
                                                  zoom_in_cb, FALSE,
												  GDK_equal, GDK_BUTTON1_MASK, view); 

  create_view_menu_item (submenu, _("Zoom out"), "<Sweep-View>/View/Zoom out",
                                                  zoom_out_cb, FALSE,
												  GDK_minus, GDK_BUTTON1_MASK, view); 

  create_view_menu_item (submenu, _("Zoom to selection"), "<Sweep-View>/View/Zoom to selection",
                                                  zoom_to_sel_cb, FALSE,
												  0, 0, s); 


#if 0
  create_view_menu_item (submenu, _("Left"), "<Sweep-View>/View/Left",
                                                  zoom_left_cb, FALSE,
												  GDK_Left, GDK_BUTTON1_MASK, s); 

  create_view_menu_item (submenu, _("Right"), "<Sweep-View>/View/Right",
                                                  zoom_right_cb, FALSE,
												  GDK_Right, GDK_BUTTON1_MASK, s); 
#endif

  create_view_menu_item (submenu, _("Zoom normal"), "<Sweep-View>/View/Zoom normal",
                                                  zoom_norm_cb, FALSE,
												  0, 0, s); 
												  
  create_view_menu_item (submenu, _("Zoom all"), "<Sweep-View>/View/Zoom all",
                                                  zoom_all_cb, FALSE,
												  GDK_1, GDK_CONTROL_MASK, view); 
												  
  create_view_menu_item (submenu, _("1:1"), "<Sweep-View>/View/1:1",
                                                  zoom_1to1_cb, FALSE,
												  0, 0, s);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);

  /* Store view */

  menuitem = gtk_menu_item_new_with_label(_("Remember as"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);
  subsubmenu = gtk_menu_new();
  gtk_menu_set_accel_group (GTK_MENU (subsubmenu), accel_group);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), subsubmenu);


#define REMEMBER_AS(title,index,accel_path) \
  menuitem = gtk_menu_item_new_with_label ((title));                         \
  g_object_set_data (G_OBJECT(menuitem), "default", GINT_TO_POINTER((index))); \
  gtk_menu_append (GTK_MENU(subsubmenu), menuitem);                          \
  gtk_widget_show (menuitem);                                                \
  g_signal_connect (G_OBJECT(menuitem), "activate",                       \
		     G_CALLBACK(view_store_cb), view);                  \
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM(menuitem), accel_path); \
  gtk_accel_map_add_entry  (accel_path,             \
			      GDK_KP_##index, GDK_CONTROL_MASK); 

  REMEMBER_AS(_("Area 1"), 1, "<Sweep-View>/View/Remember As/Area 1");
  REMEMBER_AS(_("Area 2"), 2, "<Sweep-View>/View/Remember As/Area 2");
  REMEMBER_AS(_("Area 3"), 3, "<Sweep-View>/View/Remember As/Area 3");
  REMEMBER_AS(_("Area 4"), 4, "<Sweep-View>/View/Remember As/Area 4");
  REMEMBER_AS(_("Area 5"), 5, "<Sweep-View>/View/Remember As/Area 5");
  REMEMBER_AS(_("Area 6"), 6, "<Sweep-View>/View/Remember As/Area 6");
  REMEMBER_AS(_("Area 7"), 7, "<Sweep-View>/View/Remember As/Area 7");
  REMEMBER_AS(_("Area 8"), 8, "<Sweep-View>/View/Remember As/Area 8");
  REMEMBER_AS(_("Area 9"), 9, "<Sweep-View>/View/Remember As/Area 9");
  REMEMBER_AS(_("Area 10"), 0, "<Sweep-View>/View/Remember As/Area 10");

  /* Retrieve view */

  menuitem = gtk_menu_item_new_with_label(_("Zoom to"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);
  subsubmenu = gtk_menu_new();
  gtk_menu_set_accel_group (GTK_MENU (subsubmenu), accel_group);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), subsubmenu);

#define ZOOM_TO(title,index,accel_path) \
  menuitem = gtk_menu_item_new_with_label ((title));                         \
  g_object_set_data (G_OBJECT(menuitem), "default", GINT_TO_POINTER((index))); \
  gtk_menu_append (GTK_MENU(subsubmenu), menuitem);                          \
  gtk_widget_show (menuitem);                                                \
  g_signal_connect (G_OBJECT(menuitem), "activate",                       \
		     G_CALLBACK(view_retrieve_cb), view);               \
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM(menuitem), accel_path); \
  gtk_accel_map_add_entry  (accel_path,             \
			      GDK_KP_##index, GDK_BUTTON1_MASK);

  ZOOM_TO(_("Area 1"), 1, "<Sweep-View>/View/Zoom To/Area 1");
  ZOOM_TO(_("Area 2"), 2, "<Sweep-View>/View/Zoom To/Area 2");
  ZOOM_TO(_("Area 3"), 3, "<Sweep-View>/View/Zoom To/Area 3");
  ZOOM_TO(_("Area 4"), 4, "<Sweep-View>/View/Zoom To/Area 4");
  ZOOM_TO(_("Area 5"), 5, "<Sweep-View>/View/Zoom To/Area 5");
  ZOOM_TO(_("Area 6"), 6, "<Sweep-View>/View/Zoom To/Area 6");
  ZOOM_TO(_("Area 7"), 7, "<Sweep-View>/View/Zoom To/Area 7");
  ZOOM_TO(_("Area 8"), 8, "<Sweep-View>/View/Zoom To/Area 8");
  ZOOM_TO(_("Area 9"), 9, "<Sweep-View>/View/Zoom To/Area 9");
  ZOOM_TO(_("Area 10"), 0, "<Sweep-View>/View/Zoom To/Area 10");

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);
											  
  menuitem = gtk_menu_item_new_with_label(_("Color scheme"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);
  subsubmenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), subsubmenu);

  menuitem = gtk_menu_item_new_with_label (_("Decoder Red"));
  g_object_set_data (G_OBJECT(menuitem), "default", 
			    GINT_TO_POINTER(VIEW_COLOR_RED));
  gtk_menu_append (GTK_MENU(subsubmenu), menuitem);
  gtk_widget_show (menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(sample_set_color_cb), view);

  menuitem = gtk_menu_item_new_with_label (_("Orangeboom"));
  g_object_set_data (G_OBJECT(menuitem), "default", 
			    GINT_TO_POINTER(VIEW_COLOR_ORANGE));
  gtk_menu_append (GTK_MENU(subsubmenu), menuitem);
  gtk_widget_show (menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(sample_set_color_cb), view);

  menuitem = gtk_menu_item_new_with_label (_("Lame Yellow"));
  g_object_set_data (G_OBJECT(menuitem), "default", 
			    GINT_TO_POINTER(VIEW_COLOR_YELLOW));
  gtk_menu_append (GTK_MENU(subsubmenu), menuitem);
  gtk_widget_show (menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(sample_set_color_cb), view);

  menuitem = gtk_menu_item_new_with_label (_("Coogee Bay Blue"));
  g_object_set_data (G_OBJECT(menuitem), "default", 
			    GINT_TO_POINTER(VIEW_COLOR_BLUE));
  gtk_menu_append (GTK_MENU(subsubmenu), menuitem);
  gtk_widget_show (menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(sample_set_color_cb), view);

  menuitem = gtk_menu_item_new_with_label (_("Blackwattle"));
  g_object_set_data (G_OBJECT(menuitem), "default", 
			    GINT_TO_POINTER(VIEW_COLOR_BLACK));
  gtk_menu_append (GTK_MENU(subsubmenu), menuitem);
  gtk_widget_show (menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(sample_set_color_cb), view);

  menuitem = gtk_menu_item_new_with_label (_("Frigid"));
  g_object_set_data (G_OBJECT(menuitem), "default", 
			    GINT_TO_POINTER(VIEW_COLOR_WHITE));
  gtk_menu_append (GTK_MENU(subsubmenu), menuitem);
  gtk_widget_show (menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(sample_set_color_cb), view);

  menuitem = gtk_menu_item_new_with_label (_("Radar"));
  g_object_set_data (G_OBJECT(menuitem), "default", 
			    GINT_TO_POINTER(VIEW_COLOR_RADAR));
  gtk_menu_append (GTK_MENU(subsubmenu), menuitem);
  gtk_widget_show (menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(sample_set_color_cb), view);

  menuitem = gtk_menu_item_new_with_label (_("Bluescreen"));
  g_object_set_data (G_OBJECT(menuitem), "default", 
			    GINT_TO_POINTER(VIEW_COLOR_BLUESCREEN));
  gtk_menu_append (GTK_MENU(subsubmenu), menuitem);
  gtk_widget_show (menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(sample_set_color_cb), view);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);
  
  create_view_menu_item (submenu, _("New View"), "<Sweep-View>/View/New View",
                                                  view_new_cb, FALSE,
												  0, 0, s);

  /* Sample */
  menuitem = gtk_menu_item_new_with_label(_("Sample"));
  MENU_APPEND(m, menuitem);
  gtk_widget_show(menuitem);
  submenu = gtk_menu_new();
  gtk_menu_set_accel_group (GTK_MENU (submenu), accel_group);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

  menuitem = gtk_menu_item_new_with_label (_("Channels"));
  gtk_menu_append (GTK_MENU(submenu), menuitem);
  gtk_widget_show (menuitem);

  view->channelops_menuitem = menuitem;
  view->channelops_submenu = NULL;
  view_refresh_channelops_menu (view);

#ifdef HAVE_LIBSAMPLERATE
  
  create_view_menu_item (submenu, _("Resample ..."), "<Sweep-View>/Sample/Resample ...",
                                                  samplerate_dialog_new_cb, TRUE,
												  0, 0, view);
#endif

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);
  
  create_view_menu_item (submenu, _("Duplicate"), "<Sweep-View>/Sample/Duplicate",
                                                  sample_new_copy_cb, TRUE,
												  GDK_d, GDK_CONTROL_MASK, s);

  /* Filters */
  menuitem = gtk_menu_item_new_with_label(_("Process"));
  MENU_APPEND(m, menuitem);
  gtk_widget_show(menuitem);
  submenu = create_proc_menu (view, accel_group);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

  NOMODIFY(menuitem);

  /* Playback */
  menuitem = gtk_menu_item_new_with_label(_("Playback"));
  MENU_APPEND(m, menuitem);
  gtk_widget_show(menuitem);
  submenu = gtk_menu_new();
  gtk_menu_set_accel_group (GTK_MENU (submenu), accel_group);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

  create_view_menu_item (submenu, _("Configure audio device ..."), "<Sweep-View>/Playback/Configure audio device ...",
                                                  device_config_cb, FALSE,
												  0, 0, view);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Transport"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);
  subsubmenu = gtk_menu_new();
  gtk_menu_set_accel_group (GTK_MENU (subsubmenu), accel_group);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), subsubmenu);

  NOALLOC(menuitem);

  create_view_menu_item (subsubmenu, _("Go to start of file"), "<Sweep-View>/Playback/Transport/Go to start of file",
                                                  goto_start_cb, FALSE,
												  GDK_Home, GDK_CONTROL_MASK, view);
												  									  
  create_view_menu_item (subsubmenu, _("Go to start of window"), "<Sweep-View>/Playback/Transport/Go to start of window",
                                                  goto_start_of_view_cb, FALSE,
												  GDK_Home, GDK_BUTTON1_MASK, view);

  create_view_menu_item (subsubmenu, _("Skip back"), "<Sweep-View>/Playback/Transport/Skip back",
                                                  page_back_cb, FALSE,
												  GDK_Page_Up, GDK_BUTTON1_MASK, view);

  create_view_menu_item (subsubmenu, _("Skip forward"), "<Sweep-View>/Playback/Transport/Skip forward",
                                                  page_fwd_cb, FALSE,
												  GDK_Page_Down, GDK_BUTTON1_MASK, view);

  create_view_menu_item (subsubmenu, _("Go to end of window"), "<Sweep-View>/Playback/Transport/Go to end of window",
                                                  goto_end_of_view_cb, FALSE,
												  GDK_End, GDK_BUTTON1_MASK, view);

  create_view_menu_item (subsubmenu, _("Go to end of file"), "<Sweep-View>/Playback/Transport/Go to end of file",
                                                  goto_end_cb, FALSE,
												  GDK_End, GDK_CONTROL_MASK, view);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);

  menuitem = create_view_menu_item (submenu, _("Play selection"), "<Sweep-View>/Playback/Transport/Play selection",
                                                  play_view_sel_cb, FALSE,
												  GDK_space, GDK_BUTTON1_MASK, view);
  NOALLOC(menuitem);
  
  menuitem = create_view_menu_item (submenu, _("Play sample"), "<Sweep-View>/Playback/Transport/Play sample",
                                                  play_view_cb, FALSE,
												  GDK_space, GDK_CONTROL_MASK, view);
  NOALLOC(menuitem);
										  
  menuitem = gtk_menu_item_new_with_label(_("Play note"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);
  subsubmenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), subsubmenu);

  NOALLOC(menuitem);
  
  /* 
  ** This sets up menu items and callbacks for all the note play 
  ** and is quite a bit neater than the old stuff.
  */
  noteplay_setup (subsubmenu, view, accel_group);


  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);

  menuitem = gtk_check_menu_item_new_with_label(_("Toggle monitoring"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menuitem),
				  view->sample->play_head->monitor);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(monitor_toggle_cb), view);
			 
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM(menuitem), 
                               "<Sweep-View>/Playback/Transport/Toggle monitoring");
  	 
  gtk_widget_show(menuitem);
  view->monitor_checkmenu = menuitem;

  menuitem = gtk_check_menu_item_new_with_label(_("Toggle looping"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menuitem),
				  view->sample->play_head->looping);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(loop_toggle_cb), view);

  gtk_menu_item_set_accel_path (GTK_MENU_ITEM(menuitem), 
                               "<Sweep-View>/Playback/Transport/Toggle looping");		 
  gtk_widget_show(menuitem);
  view->loop_checkmenu = menuitem;

  menuitem = gtk_check_menu_item_new_with_label(_("Toggle muting"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menuitem),
				  view->sample->play_head->mute);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(mute_toggle_cb), view);
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM(menuitem), 
                               "<Sweep-View>/Playback/Transport/Toggle muting");
  gtk_widget_show(menuitem);
  view->mute_checkmenu = menuitem;

  menuitem = gtk_check_menu_item_new_with_label(_("Toggle reverse playback"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menuitem),
				  view->sample->play_head->reverse);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(playrev_toggle_cb), view);
  gtk_widget_show(menuitem);
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM(menuitem), 
                               "<Sweep-View>/Playback/Transport/Toggle reverse playback");
  gtk_accel_map_add_entry  ("<Sweep-View>/Playback/Transport/Toggle reverse playback",
                             GDK_quoteleft, GDK_BUTTON1_MASK);	

  view->playrev_checkmenu = menuitem;

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);

  menuitem = create_view_menu_item (submenu, _("Pause"), "<Sweep-View>/Playback/Pause",
                                                  pause_playback_cb, FALSE,
												  0, 0, view);
  NOALLOC(menuitem);

  menuitem = create_view_menu_item (submenu, _("Stop"), "<Sweep-View>/Playback/Stop",
                                                  stop_playback_cb, FALSE,
												  GDK_Return, GDK_BUTTON1_MASK, view);
  NOALLOC(menuitem);
												  
  menuitem = gtk_menu_item_new_with_label (_("Help"));
  MENU_APPEND(m, menuitem);
  gtk_widget_show(menuitem);
  submenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM(menuitem), submenu);

  menuitem = gtk_menu_item_new_with_label (_("About MP3 export..."));
  gtk_menu_append (GTK_MENU(submenu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		      G_CALLBACK(mp3_unsupported_dialog), NULL);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label (_("About Sweep ..."));
  gtk_menu_append (GTK_MENU(submenu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		      G_CALLBACK(about_dialog_create), NULL);
  gtk_widget_show(menuitem);

  return accel_group;
}

/*
 * create_context_menu_sel (view)
 *
 * Creates a context menu for operations on a selection
 */
static GtkWidget *
create_context_menu_sel (sw_view * view)
{
  GtkWidget * menu;
  GtkWidget * menuitem;
  GtkWidget * submenu;
  SampleDisplay * s = SAMPLE_DISPLAY(view->display);

  menu = gtk_menu_new ();

  /* Zoom */

  menuitem = gtk_menu_item_new_with_label(_("Zoom to selection"));
  gtk_menu_append(GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(zoom_to_sel_cb), s);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Zoom normal"));
  gtk_menu_append(GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
                    G_CALLBACK(zoom_norm_cb), s);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Zoom all"));
  gtk_menu_append(GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(zoom_all_cb), view);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);


  /* Edit */

  menuitem = gtk_menu_item_new_with_label(_("Edit"));
  gtk_menu_append (GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);
  submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

  NOMODIFY(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Cut"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(cut_cb), view);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Copy"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(copy_cb), view);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Clear"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(clear_cb), view);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Crop"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(crop_cb), view);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Paste: Insert"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(paste_cb), view);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Paste: Mix"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(paste_mix_cb), view);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Paste: Crossfade"));
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(paste_xfade_cb), view);
  gtk_widget_show(menuitem);

  /* Filters */

  menuitem = gtk_menu_item_new_with_label(_("Process"));
  gtk_menu_append (GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);
  submenu = create_proc_menu (view, NULL);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

  NOMODIFY(menuitem);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);

  /* Select */

  menuitem = gtk_menu_item_new_with_label(_("Invert selection"));
  gtk_menu_append(GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(select_invert_cb), s);
  gtk_widget_show(menuitem);

  NOMODIFY(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Select all"));
  gtk_menu_append(GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(select_all_cb), s);
  gtk_widget_show(menuitem);

  NOMODIFY(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Select none"));
  gtk_menu_append(GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(select_none_cb), s);
  gtk_widget_show(menuitem);

  NOMODIFY(menuitem);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);

  /* View */

  menuitem = gtk_menu_item_new_with_label(_("New View"));
  gtk_menu_append(GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(view_new_cb), s);
  gtk_widget_show(menuitem);

#if 0
  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);

  /* Properties */

  menuitem = gtk_menu_item_new_with_label(_("File properties ..."));
  gtk_menu_append(GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(show_info_dialog_cb), view);
  gtk_widget_show(menuitem);
#endif

  return menu;
}

/*
 * create_context_menu_point (view)
 *
 * Creates a context menu for point (cursor) operations, ie. when
 * no selection is active
 */
static GtkWidget *
create_context_menu_point (sw_view * view)
{
  GtkWidget * menu;
  GtkWidget * menuitem;
  SampleDisplay * s = SAMPLE_DISPLAY(view->display);

  menu = gtk_menu_new ();

  /* Zoom */

  menuitem = gtk_menu_item_new_with_label(_("Zoom normal"));
  gtk_menu_append(GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
                    G_CALLBACK(zoom_norm_cb), s);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new_with_label(_("Zoom all"));
  gtk_menu_append(GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(zoom_all_cb), view);
  gtk_widget_show(menuitem);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);

  /* Edit */

  menuitem = gtk_menu_item_new_with_label(_("Paste"));
  gtk_menu_append(GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(paste_cb), view);
  gtk_widget_show(menuitem);

  NOMODIFY(menuitem);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);

  /* Select */

  menuitem = gtk_menu_item_new_with_label(_("Select all"));
  gtk_menu_append(GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(select_all_cb), s);
  gtk_widget_show(menuitem);

  NOMODIFY(menuitem);

  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);

  /* View */

  menuitem = gtk_menu_item_new_with_label(_("New View"));
  gtk_menu_append(GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(view_new_cb), s);
  gtk_widget_show(menuitem);

#if 0
  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);

  /* Properties */

  menuitem = gtk_menu_item_new_with_label(_("File properties ..."));
  gtk_menu_append(GTK_MENU(menu), menuitem);
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(show_info_dialog_cb), view);
  gtk_widget_show(menuitem);
#endif

  return menu;
}

static gint
view_destroy_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;

  sample_display_stop_marching_ants (SAMPLE_DISPLAY(view->display));

  if (view->sample->op_progress_tag != -1)
    gtk_timeout_remove(view->sample->op_progress_tag);
  cancel_active_op (view->sample);
  sample_remove_view(view->sample, view);
  gtk_widget_destroy (GTK_WIDGET (view->display));

  return (FALSE);
}

static void
db_ruler_changed_cb (GtkWidget * widget, gpointer data)
{
  GtkRuler * ruler = GTK_RULER(widget);
  sw_view * view = (sw_view *)data;

  view_set_vzoom (view, ruler->lower, ruler->upper);
}

static void
view_refresh_db_rulers (sw_view * view)
{
  int i, old_channels, new_channels;
  GtkWidget * vbox = view->db_rulers_vbox;
  GList * gl;
  GtkWidget * db_ruler;
  
  old_channels = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(vbox), "default"));
  new_channels = view->sample->sounddata->format->channels;

  if (old_channels == 0 || old_channels != new_channels) {

    for (gl = view->db_rulers; gl; gl = gl->next) {
	  g_signal_handlers_disconnect_matched(GTK_OBJECT(view->window),
								G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, gl->data);
      gtk_widget_destroy (GTK_WIDGET(gl->data));
    }
    g_list_free (view->db_rulers);
    view->db_rulers = NULL;

    g_object_set_data (G_OBJECT(vbox), "default", GINT_TO_POINTER(new_channels));

    for (i = 0; i < new_channels; i++) {
      db_ruler = db_ruler_new ();
      gtk_box_pack_start (GTK_BOX(vbox), db_ruler, TRUE, TRUE, 0);
      gtk_ruler_set_range (GTK_RULER(db_ruler), -1.0, 1.0, 0, 2.0);
      gtk_widget_show (db_ruler);

     g_signal_connect_swapped(GTK_OBJECT(view->window),
				 "motion_notify_event",
		
				 G_CALLBACK(GTK_WIDGET_GET_CLASS(db_ruler)->motion_notify_event),
		
				 GTK_OBJECT (db_ruler));

		
      g_signal_connect (G_OBJECT(db_ruler), "changed",
			  G_CALLBACK(db_ruler_changed_cb), view);
    
      view->db_rulers = g_list_append (view->db_rulers, db_ruler);
    }
  }

  for (gl = view->db_rulers; gl; gl = gl->next) {
    db_ruler = GTK_WIDGET(gl->data);
    gtk_ruler_set_range (GTK_RULER(db_ruler), view->vlow, view->vhigh,
			 0, 2.0);
  }
}


static void
view_rate_changed_cb (GtkWidget * widget, gpointer data)
{
  sw_view * v = (sw_view *)data;
  sw_sample * s = v->sample;

  s->rate = 1.0 - GTK_ADJUSTMENT(v->rate_adj)->value/1000.0;
}

static void
view_rate_zeroed_cb (GtkWidget * Widget, gpointer data)
{
  sw_view * v = (sw_view *)data;
  sw_sample * s = v->sample;

  s->rate = 1.0;

  gtk_adjustment_set_value (GTK_ADJUSTMENT(v->rate_adj), 0.0);
}

static void
view_gain_changed_cb (GtkWidget * widget, gpointer data)
{
  sw_view * v = (sw_view *)data;

  head_set_gain (v->sample->play_head,
		 GTK_ADJUSTMENT (v->gain_adj)->value / 10.0);
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
		  frames_to_time (view->sample->sounddata->format,
				  sd->mouse_offset));
    gtk_label_set_text (GTK_LABEL(view->pos), buf);
  } else {
    gtk_label_set_text (GTK_LABEL(view->pos), NO_TIME);
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

#define VIEW_TOOLBAR_BUTTON SW_TOOLBAR_BUTTON
#define VIEW_TOOLBAR_TOGGLE_BUTTON SW_TOOLBAR_TOGGLE_BUTTON
#define VIEW_TOOLBAR_RADIO_BUTTON SW_TOOLBAR_RADIO_BUTTON

#if 0
typedef enum {
  VIEW_TOOLBAR_BUTTON,
  VIEW_TOOLBAR_TOGGLE_BUTTON,
  VIEW_TOOLBAR_RADIO_BUTTON,
} view_toolbar_button_type;

static GtkWidget *
create_pixmap_button (GtkWidget * widget, gchar ** xpm_data,
		      const gchar * tip_text, const gchar * custom_name,
		      view_toolbar_button_type button_type,
		      GCallback clicked,
		      GCallback pressed, GCallback released,
		      gpointer data)
{
  GtkWidget * pixmap;
  GtkWidget * button;
  GtkTooltips * tooltips;

  switch (button_type) {
  case VIEW_TOOLBAR_TOGGLE_BUTTON:
    button = gtk_toggle_button_new ();
    break;
  case VIEW_TOOLBAR_RADIO_BUTTON:
    button = gtk_radio_button_new (NULL);
    break;
  case VIEW_TOOLBAR_BUTTON:
  default:
    button = gtk_button_new ();
    break;
  }

  if (xpm_data != NULL) {
    pixmap = create_widget_from_xpm (widget, xpm_data);
    gtk_widget_show (pixmap);
    gtk_container_add (GTK_CONTAINER (button), pixmap);
  }

  if (tip_text != NULL) {
    tooltips = gtk_tooltips_new ();
    gtk_tooltips_set_tip (tooltips, button, tip_text, NULL);
  }

  if (style != NULL) {
    gtk_widget_set_style (button, style);
  }

  if (clicked != NULL) {
    g_signal_connect (G_OBJECT (button), "clicked",
			G_CALLBACK(clicked), data);
  }

  if (pressed != NULL) {
    g_signal_connect (G_OBJECT(button), "pressed",
			G_CALLBACK(pressed), data);
  }

  if (released != NULL) {
    g_signal_connect (G_OBJECT(button), "released",
			G_CALLBACK(released), data);
  }

  return button;
}
#endif

static void
scrub_clicked_cb (GtkWidget * widget, GdkEventButton * event, gpointer data)
{
  sw_view * view = (sw_view *)data;
  int width;
  sw_framecount_t offset;

  width = widget->allocation.width;
  offset = view->start + (view->end - view->start) * event->x / width;
  sample_set_playmarker (view->sample, offset, TRUE);
  sample_set_scrubbing (view->sample, TRUE);
}

static void
scrub_motion_cb (GtkWidget * widget, GdkEventMotion * event, gpointer data)
{
  sw_view * view = (sw_view *)data;
  int width;
  sw_framecount_t offset;

  gdk_window_set_cursor (widget->window,
			 sweep_cursors[SWEEP_CURSOR_NEEDLE]);

  if (event->state & (GDK_BUTTON1_MASK|GDK_BUTTON2_MASK|GDK_BUTTON3_MASK)) {
    width = widget->allocation.width;
    offset = view->start + (view->end - view->start) * event->x / width;
    sample_set_playmarker (view->sample, offset, TRUE);  
  }
}

static void
scrub_released_cb (GtkWidget * widget, GdkEventButton * event, gpointer data)
{
  sw_view * view = (sw_view *)data;

  sample_set_scrubbing (view->sample, FALSE);
}

static void
vzoom_clicked_cb (GtkWidget * widget, GdkEventButton * event, gpointer data)
{
  /*  sw_view * view = (sw_view *)data;*/
}

static void
vzoom_motion_cb (GtkWidget * widget, GdkEventMotion * event, gpointer data)
{
  /*  sw_view * view = (sw_view *)data;*/
}

static void
vzoom_released_cb (GtkWidget * widget, GdkEventButton * event, gpointer data)
{
  /*  sw_view * view = (sw_view *)data;*/
}

sw_view *
view_new(sw_sample * sample, sw_framecount_t start, sw_framecount_t end,
	 gfloat gain)
{
  sw_view * view;

  gint screen_width, screen_height;
  gint win_width, win_height;

  GtkWidget * window;
  GtkWidget * main_vbox;
  GtkWidget * table;
  GtkWidget * hbox;
  GtkWidget * vbox;
  GtkWidget * handlebox;
  GtkWidget * separator;
  GtkWidget * ebox;
  GtkWidget * time_ruler;
  GtkWidget * scrollbar;
  GtkWidget * rate_vbox;
  GtkObject * rate_adj;
  GtkWidget * rate_vscale;
  GtkWidget * lcdbox;
  GtkWidget * imagebox;
  GtkObject * gain_adj;
  GtkWidget * tool_hbox;
  GtkWidget * gain_hscale;
  GtkWidget * menu_button;
  GtkWidget * button;
  GtkWidget * arrow;
  GtkWidget * pixmap;
  GtkWidget * progress;
  GtkWidget * frame;
  GtkWidget * label;
#if 0
  GtkWidget * entry;
#endif

#if 0
  GtkWidget * toolbar;
#endif

#ifdef DEVEL_CODE
  GtkWidget * notebook;
#endif

  GtkAccelGroup * accel_group;

  GtkTooltips * tooltips;

  GList * zoom_combo_items;
  GtkWidget * zoom_combo;

  gfloat step = 1.0;

  view = g_malloc0 (sizeof(sw_view));

  view->sample = sample;
  view->start = start;
  view->end = end;
  view->vlow = SW_AUDIO_T_MIN;
  view->vhigh = SW_AUDIO_T_MAX;

  /*  view->gain = gain;*/

  view->current_tool = TOOL_SELECT;

  view->repeater_tag = 0;

  view->following = TRUE;

  view->noready_widgets = NULL;
  view->nomodify_widgets = NULL;
  view->noalloc_widgets = NULL;

  view->channelops_widgets = NULL;

#if 0
  win_width = CLAMP (sample->sounddata->nr_frames / 150,
		     VIEW_MIN_WIDTH, VIEW_MAX_WIDTH);
  win_height =
    VIEW_DEFAULT_HEIGHT_PER_CHANNEL *
    MIN (2, sample->sounddata->format->channels);
#else
  screen_width = gdk_screen_width ();
  screen_height = gdk_screen_height ();

  if (sample->views == NULL) {
    win_height = screen_height / 4;
  } else {
    win_height = screen_height / 8;
  }
  win_width = (win_height * 2 * 1618) / 1000;

  win_height *= MIN (2, sample->sounddata->format->channels);
#endif

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  sweep_set_window_icon (GTK_WINDOW(window));

  gtk_window_set_default_size (GTK_WINDOW(window), win_width, win_height);
  view->window = window;

  g_signal_connect (G_OBJECT(window), "destroy",
		      G_CALLBACK(view_destroy_cb), view);

  main_vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(window), main_vbox);
  gtk_widget_show (main_vbox);

  handlebox = gtk_handle_box_new ();
  gtk_box_pack_start (GTK_BOX (main_vbox), handlebox, FALSE, TRUE, 0);
  gtk_widget_show (handlebox);

  view->menubar = gtk_menu_bar_new ();
  gtk_container_add (GTK_CONTAINER (handlebox), view->menubar);
  gtk_widget_show (view->menubar);

  /* file toolbar */

  handlebox = gtk_handle_box_new ();
  gtk_handle_box_set_shadow_type (GTK_HANDLE_BOX(handlebox), GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (main_vbox), handlebox, FALSE, TRUE, 0);
  gtk_widget_show (handlebox);

  /*  gtk_widget_set_style (handlebox, style_dark_grey);*/

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_container_add (GTK_CONTAINER (handlebox), hbox);
  gtk_widget_show (hbox);

  gtk_widget_set_size_request (hbox, -1, 26);

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (hbox), separator, FALSE, TRUE, 2);
  gtk_widget_show (separator);


  /* File op buttons */

  tool_hbox = gtk_hbox_new (TRUE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), tool_hbox, FALSE, TRUE, 0);
  gtk_widget_show (tool_hbox);

  button = create_pixmap_button (window, new_xpm, _("New ..."),
				 NULL, VIEW_TOOLBAR_BUTTON,
				 G_CALLBACK (sample_new_empty_cb), NULL, NULL, view);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  button = create_pixmap_button (window, open_xpm, _("Open ..."),
				 NULL, VIEW_TOOLBAR_BUTTON,
				G_CALLBACK (sample_load_cb), NULL, NULL, NULL);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  button = create_pixmap_button (window, save_xpm, _("Save"),
				 NULL, VIEW_TOOLBAR_BUTTON,
				 G_CALLBACK (sample_save_cb), NULL, NULL, view);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  NOMODIFY(button);

  button = create_pixmap_button (window, saveas_xpm, _("Save as ..."),
				 NULL, VIEW_TOOLBAR_BUTTON,
				 G_CALLBACK (sample_save_as_cb), NULL, NULL, view);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  NOMODIFY(button);

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (hbox), separator, FALSE, TRUE, 2);
  gtk_widget_show (separator);

  /* Edit buttons */

  tool_hbox = gtk_hbox_new (TRUE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), tool_hbox, FALSE, TRUE, 0);
  gtk_widget_show (tool_hbox);

  button = create_pixmap_button (window, cut_xpm,
				 _("Cut selection to clipboard"),
				 NULL, VIEW_TOOLBAR_BUTTON,
				 G_CALLBACK (cut_cb),
				 NULL, NULL, view);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);
  NOMODIFY(button);

  button = create_pixmap_button (window, copy_xpm,
				 _("Copy selection to clipboard"),
				 NULL, VIEW_TOOLBAR_BUTTON,
				 G_CALLBACK (copy_cb),
				 NULL, NULL, view);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);
  NOMODIFY(button);

  button =
    create_pixmap_button (window, paste_xpm,
			  _("Paste: insert clipboard at cursor position"),
			  NULL, VIEW_TOOLBAR_BUTTON,
			  G_CALLBACK (paste_cb),
			  NULL, NULL, view);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);
  NOMODIFY(button);

  button =
    create_pixmap_button (window, pastemix_xpm,
			  _("Paste: mix clipboard in from cursor position"),
			  NULL, VIEW_TOOLBAR_BUTTON,
			  G_CALLBACK (paste_mix_cb),
			  NULL, NULL, view);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);
  NOMODIFY(button);

  button =
    create_pixmap_button (window, pastexfade_xpm,
			  _("Paste: fade clipboard in from cursor position"),
			  NULL, VIEW_TOOLBAR_BUTTON,
			  G_CALLBACK (paste_xfade_cb),
			  NULL, NULL, view);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);
  NOMODIFY(button);

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (hbox), separator, FALSE, TRUE, 2);
  gtk_widget_show (separator);

  button = create_pixmap_button (window, crop_xpm,
				 _("Crop"),
				 NULL, VIEW_TOOLBAR_BUTTON,
				 G_CALLBACK (crop_cb),
				 NULL, NULL, view);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);
  NOMODIFY(button);

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (hbox), separator, FALSE, TRUE, 2);
  gtk_widget_show (separator);

  /* Undo/Redo */

  tool_hbox = gtk_hbox_new (TRUE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), tool_hbox, FALSE, TRUE, 0);
  gtk_widget_show (tool_hbox);

  button = create_pixmap_button (window, undo_xpm, _("Undo"),
				 NULL, VIEW_TOOLBAR_BUTTON,
				 G_CALLBACK (undo_cb),
				 NULL, NULL, view);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);
  NOMODIFY(button);

  button = create_pixmap_button (window, redo_xpm, _("Redo"),
				 NULL, VIEW_TOOLBAR_BUTTON,
				 G_CALLBACK (redo_cb),
				 NULL, NULL, view);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);
  NOMODIFY(button);

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (hbox), separator, FALSE, TRUE, 4);
  gtk_widget_show (separator);


  /* TOOLS */

  ebox = gtk_event_box_new ();
  gtk_box_pack_start (GTK_BOX (hbox), ebox, FALSE, FALSE, 0);
  gtk_widget_show (ebox);

  gtk_widget_set_style (ebox, style_light_grey);

  tool_hbox = gtk_hbox_new (TRUE, 4);
  gtk_container_add (GTK_CONTAINER(ebox), tool_hbox);
  gtk_widget_show (tool_hbox);

  view->tool_buttons = NULL;

  button = create_pixmap_button (window, select_xpm, _("Selector tool"),
				 style_light_grey, VIEW_TOOLBAR_TOGGLE_BUTTON,
				 G_CALLBACK (view_set_tool_cb), NULL, NULL, view);
  g_object_set_data (G_OBJECT(button), "default", GINT_TO_POINTER(TOOL_SELECT));

  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);
  view->tool_buttons = g_list_append (view->tool_buttons, button);

  NOMODIFY(button);

  button = create_pixmap_button (window, scrub_xpm,
				 _("\"Scrubby\" the scrub tool"),
				 style_light_grey, VIEW_TOOLBAR_TOGGLE_BUTTON,
				 G_CALLBACK (view_set_tool_cb), NULL, NULL, view);
  g_object_set_data (G_OBJECT(button), "default", GINT_TO_POINTER(TOOL_SCRUB));
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);
  view->tool_buttons = g_list_append (view->tool_buttons, button);

#ifdef DEVEL_CODE /* Pencil & Noise -- these need undos */

  button = create_pixmap_button (window, pencil_xpm, _("Pencil tool"),
				 style_light_grey, VIEW_TOOLBAR_TOGGLE_BUTTON,
				 G_CALLBACK (view_set_tool_cb), NULL, NULL, view);
  g_object_set_data (G_OBJECT(button), "default", GINT_TO_POINTER(TOOL_PENCIL));
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);
  view->tool_buttons = g_list_append (view->tool_buttons, button);

  button = create_pixmap_button (window, spraycan_xpm, _("Noise tool"),
				 style_light_grey, VIEW_TOOLBAR_TOGGLE_BUTTON,
				 G_CALLBACK (view_set_tool_cb), NULL, NULL, view);
  g_object_set_data (G_OBJECT(button), "default", GINT_TO_POINTER(TOOL_NOISE));
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);
  view->tool_buttons = g_list_append (view->tool_buttons, button);

#endif

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (hbox), separator, FALSE, TRUE, 4);
  gtk_widget_show (separator);


  /* ZOOM */

  tool_hbox = gtk_hbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), tool_hbox, FALSE, TRUE, 0);
  gtk_widget_show (tool_hbox);

  gtk_widget_set_style (tool_hbox, style_dark_grey);

#if 0
  /* Zoom all */
  button = create_pixmap_button (window, zoom_all_xpm, _("Zoom all"),
				 NULL, VIEW_TOOLBAR_BUTTON, G_CALLBACK (zoom_all_cb),
				 NULL, NULL, view);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);
#endif

  /* Zoom in */
  button = create_pixmap_button (window, zoom_in_xpm, _("Zoom in"),
				 NULL, VIEW_TOOLBAR_BUTTON, NULL,
				 G_CALLBACK (zoom_in_pressed_cb), G_CALLBACK (repeater_released_cb),
				 view);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  /* Zoom out */
  button = create_pixmap_button (window, zoom_out_xpm, _("Zoom out"),
				 NULL, VIEW_TOOLBAR_BUTTON, NULL,
				 G_CALLBACK (zoom_out_pressed_cb), G_CALLBACK (repeater_released_cb),
				 view);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  /* Zoom combo */

  zoom_combo_items = NULL;
  zoom_combo_items = g_list_append (zoom_combo_items, "All");
  zoom_combo_items = g_list_append (zoom_combo_items, "01:00:00.000");
  zoom_combo_items = g_list_append (zoom_combo_items, "00:30:00.000");
  zoom_combo_items = g_list_append (zoom_combo_items, "00:05:00.000");
  zoom_combo_items = g_list_append (zoom_combo_items, "00:01:00.000");
  zoom_combo_items = g_list_append (zoom_combo_items, "00:00:30.000");
  zoom_combo_items = g_list_append (zoom_combo_items, "00:00:15.000");
  zoom_combo_items = g_list_append (zoom_combo_items, "00:00:05.000");
  zoom_combo_items = g_list_append (zoom_combo_items, "00:00:01.000");
  zoom_combo_items = g_list_append (zoom_combo_items, "00:00:00.500");
  zoom_combo_items = g_list_append (zoom_combo_items, "00:00:00.100");
  zoom_combo_items = g_list_append (zoom_combo_items, "00:00:00.010");
  zoom_combo_items = g_list_append (zoom_combo_items, "00:00:00.001");

  zoom_combo = gtk_combo_new ();
  
 /* connect hack_max_combo_width_cb to the theme change signal so zoom_combo is 
  * kept at an appropriate size regardless of font or theme changes.
  *
  * replace with gtk_entry_set_width_chars() when GTK+-2.6 is used by mainstream distro's */
  g_signal_connect (G_OBJECT(GTK_COMBO(zoom_combo)->entry), "style_set", G_CALLBACK(hack_max_combo_width_cb), NULL);
  
  gtk_combo_set_popdown_strings (GTK_COMBO(zoom_combo), zoom_combo_items);
  gtk_combo_set_value_in_list (GTK_COMBO(zoom_combo), FALSE, TRUE);

  /* unfortunately we can't just edit the zoom value, because the entry
   * reports every keystroke as a change, and not the 'enter' key. */
  gtk_editable_set_editable (GTK_EDITABLE (GTK_COMBO(zoom_combo)->entry), TRUE);

  /*gtk_widget_set_style (GTK_COMBO(zoom_combo)->button, style_dark_grey);*/

  g_signal_connect (G_OBJECT(GTK_COMBO(zoom_combo)->entry), "changed",
		      G_CALLBACK(zoom_combo_changed_cb), view);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, GTK_COMBO(zoom_combo)->button,
			_("Visible length"), NULL);

  view->zoom_combo = zoom_combo;

  gtk_box_pack_start (GTK_BOX (tool_hbox), view->zoom_combo, FALSE, FALSE, 0);

  gtk_widget_show (zoom_combo);

  button = create_pixmap_button (window, scroll_xpm,
				 _("Autoscroll: follow playback cursor"),
				 NULL, VIEW_TOOLBAR_TOGGLE_BUTTON,
				 G_CALLBACK (follow_toggled_cb),
				 NULL, NULL, view);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_widget_set_style (button, style_light_grey);

  gtk_signal_handler_block_by_data (GTK_OBJECT(button), view);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button),
				view->following);
  g_signal_handlers_unblock_matched (GTK_OBJECT(button), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);

  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  view->follow_toggle = button;

#ifdef DEVEL_CODE
  button = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX(main_vbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  /* notebook */

  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX(main_vbox), notebook, TRUE, TRUE, 0);
  gtk_widget_show (notebook);

  gtk_widget_set_style (notebook, style_light_grey);

  gtk_container_set_border_width (GTK_CONTAINER(notebook), 0);
  if (TRUE) {
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK(notebook), TRUE);
  }
#endif

  /* main table */

  table = gtk_table_new (3, 3, FALSE);
  gtk_table_set_col_spacing (GTK_TABLE(table), 0, 1);
  gtk_table_set_col_spacing (GTK_TABLE(table), 1, 2);
  gtk_table_set_row_spacing (GTK_TABLE(table), 0, 1);
  gtk_table_set_row_spacing (GTK_TABLE(table), 1, 2);
  gtk_container_set_border_width (GTK_CONTAINER(table), 2);
#ifdef DEVEL_CODE
  label = gtk_label_new (g_basename(sample->pathname));
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), table, label);
#else
  gtk_box_pack_start (GTK_BOX(main_vbox), table, TRUE, TRUE, 0);
#endif
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

  /* time_ruler */

  ebox = gtk_event_box_new ();
  gtk_table_attach (GTK_TABLE(table), ebox,
		    1, 2, 0, 1,
		    GTK_EXPAND|GTK_FILL|GTK_SHRINK, GTK_FILL,
		    0, 0);
  g_signal_connect (G_OBJECT(ebox), "button-press-event",
		      G_CALLBACK(scrub_clicked_cb), view);
  g_signal_connect (G_OBJECT(ebox), "motion-notify-event",
		      G_CALLBACK(scrub_motion_cb), view);
  g_signal_connect (G_OBJECT(ebox), "button-release-event",
		      G_CALLBACK(scrub_released_cb), view);
  gtk_widget_show (ebox);

  time_ruler = time_ruler_new ();
  gtk_container_add (GTK_CONTAINER(ebox), time_ruler);
  gtk_ruler_set_range (GTK_RULER(time_ruler),
		       start, end,
		       start, end);
  time_ruler_set_format (TIME_RULER(time_ruler), sample->sounddata->format);
  gtk_widget_show (time_ruler);
  view->time_ruler = time_ruler;
/* tmp GTK_<OBJECT>_GET_CLASS (object)  */
  g_signal_connect_swapped (GTK_OBJECT (table), "motion_notify_event",
                             G_CALLBACK(GTK_WIDGET_GET_CLASS(time_ruler)->motion_notify_event),
                             GTK_OBJECT (time_ruler));

  /* db_ruler */

  ebox = gtk_event_box_new ();
  gtk_table_attach (GTK_TABLE(table), ebox,
		    0, 1, 1, 2,
		    GTK_FILL, GTK_EXPAND|GTK_SHRINK|GTK_FILL,
		    0, 0);
  g_signal_connect (G_OBJECT(ebox), "button-press-event",
		      G_CALLBACK(vzoom_clicked_cb), view);
  g_signal_connect (G_OBJECT(ebox), "motion-notify-event",
		      G_CALLBACK(vzoom_motion_cb), view);
  g_signal_connect (G_OBJECT(ebox), "button-release-event",
		     G_CALLBACK(vzoom_released_cb), view);
  gtk_widget_show (ebox);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, ebox,
			_("Vertical zoom [Shift + Arrow Up/Down]"), NULL);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(ebox), vbox);
  gtk_widget_show (vbox);
  view->db_rulers_vbox = vbox;
  view->db_rulers = NULL;

  view_refresh_db_rulers (view);

  /* display */
  view->display = sample_display_new();
  gtk_table_attach (GTK_TABLE(table), view->display,
		    1, 2, 1, 2,
		    GTK_EXPAND|GTK_FILL|GTK_SHRINK,
		    GTK_EXPAND|GTK_FILL|GTK_SHRINK,
		    0, 0);

  sample_display_set_view(SAMPLE_DISPLAY(view->display), view);

  g_signal_connect (G_OBJECT(view->display), "selection-changed",
		      G_CALLBACK(sd_sel_changed_cb), view);
  g_signal_connect (G_OBJECT(view->display), "window-changed",
		      G_CALLBACK(sd_win_changed_cb), view);

  gtk_widget_show(view->display);

  /* rate adjuster */

  rate_vbox = gtk_vbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE(table), rate_vbox,
		    2, 3, 1, 2,
		    GTK_FILL, GTK_EXPAND|GTK_SHRINK|GTK_FILL,
		    0, 0);
  gtk_widget_show (rate_vbox);

  label = gtk_label_new ("+10%");
  gtk_box_pack_start (GTK_BOX(rate_vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

#if 0
  rate_adj = gtk_adjustment_new (50.0,   /* value */
				 0.0,    /* lower */
				 100.0,  /* upper */
				 0.001,  /* step incr */
				 0.001,  /* page incr */
				 0.001   /* page size */
				 );
#else
  rate_adj = gtk_adjustment_new (0.0,    /* value */
				 -100.0, /* lower */
				 100.0,  /* upper */
				 2.5,    /* step incr */
				 20.0,   /* page incr */
				 0.0     /* page size */
				 );
#endif

  /*  view->rate = 1.0;*/
  view->rate_adj = rate_adj;

  rate_vscale = gtk_vscale_new (GTK_ADJUSTMENT(rate_adj));
  gtk_box_pack_start (GTK_BOX(rate_vbox), rate_vscale, TRUE, TRUE, 0);
  gtk_scale_set_draw_value (GTK_SCALE(rate_vscale), FALSE);
  gtk_range_set_update_policy (GTK_RANGE(rate_vscale), GTK_UPDATE_CONTINUOUS);
  gtk_widget_show (rate_vscale); 

  g_signal_connect (G_OBJECT(rate_adj), "value_changed",
		      G_CALLBACK(view_rate_changed_cb), view);

  label = gtk_label_new ("-10%");
  gtk_box_pack_start (GTK_BOX(rate_vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  button = gtk_button_new_with_label ("0%");
  gtk_box_pack_start (GTK_BOX(rate_vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK(view_rate_zeroed_cb), view);

  /* scrollbar */
  step = ((gfloat)(end - start)) / 10.0;
  if (step < 1.0) step = 1.0;

  view->adj =
    gtk_adjustment_new((gfloat)start,            /* value */
		       (gfloat)0.0,              /* start */
		       (gfloat)sample->sounddata->nr_frames, /* end */
		       step,                      /* step_incr */
		       (gfloat)(end-start),      /* page_incr */
		       (gfloat)(end-start)       /* page_size */
		       );

  g_signal_connect (G_OBJECT(view->adj), "value-changed",
		      G_CALLBACK(adj_value_changed_cb), view);
  g_signal_connect (G_OBJECT(view->adj), "changed",
		      G_CALLBACK(adj_changed_cb), view);

  scrollbar = gtk_hscrollbar_new(GTK_ADJUSTMENT(view->adj));
  gtk_table_attach (GTK_TABLE(table), scrollbar,
		    1, 2, 2, 3,
		    GTK_EXPAND|GTK_FILL|GTK_SHRINK, GTK_FILL,
		    0, 0);
  gtk_widget_show(scrollbar);


  /* playback toolbar */

  handlebox = gtk_handle_box_new ();
  gtk_box_pack_start (GTK_BOX (main_vbox), handlebox, FALSE, TRUE, 0);
  gtk_widget_show (handlebox);

  gtk_widget_set_style (handlebox, style_light_grey);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_container_add (GTK_CONTAINER (handlebox), hbox);
  gtk_widget_show (hbox);

  gtk_widget_set_usize (hbox, -1, 24);


  /* Record */

  tool_hbox = gtk_hbox_new (TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), tool_hbox, FALSE, TRUE, 0);
  gtk_widget_show (tool_hbox);

  button
    = create_pixmap_button (window, record_dialog_xpm,
			    _("Record ..."),
			    style_red_grey, VIEW_TOOLBAR_BUTTON,
			    G_CALLBACK (show_rec_dialog_cb),
			    NULL, NULL, view);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  NOALLOC(button);

#define PLAYPOS_LABEL
#ifdef PLAYPOS_LABEL
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
  view->play_pos = label;

  imagebox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(tool_hbox), imagebox, FALSE, FALSE, 0);
  gtk_widget_show (imagebox);

  pixmap = create_widget_from_xpm (window, upright_xpm);
  gtk_widget_show (pixmap);
  gtk_box_pack_start (GTK_BOX(imagebox), pixmap, FALSE, FALSE, 0);

  pixmap = create_widget_from_xpm (window, lowright_xpm);
  gtk_widget_show (pixmap);
  gtk_box_pack_end (GTK_BOX(imagebox), pixmap, FALSE, FALSE, 0);
  
#else
  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY(entry), "00:00:00.000");
  gtk_entry_set_editable (GTK_ENTRY(entry), FALSE);
  gtk_widget_set_style (entry, style_LCD);
  gtk_box_pack_start (GTK_BOX(hbox), entry, TRUE, TRUE, 0);
  gtk_widget_show (entry);
  view->play_pos = entry;
#endif


  tool_hbox = gtk_hbox_new (TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), tool_hbox, FALSE, TRUE, 0);
  gtk_widget_show (tool_hbox);

  /* Play reverse */

  button = create_pixmap_button (window, playrev_xpm,
				 _("Reverse mode playback (toggle)"),
				 style_green_grey, VIEW_TOOLBAR_TOGGLE_BUTTON,
				 G_CALLBACK (playrev_toggled_cb), NULL, NULL, view);

  g_signal_handlers_block_matched (GTK_OBJECT(button), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button),
				view->sample->play_head->reverse);
  g_signal_handlers_unblock_matched (GTK_OBJECT(button), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);

  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  view->playrev_toggle = button;


  /* Loop */

  button = create_pixmap_button (window, loop_xpm,
				 _("Loop mode playback (toggle)"),
				 style_green_grey, VIEW_TOOLBAR_TOGGLE_BUTTON,
				 G_CALLBACK (loop_toggled_cb), NULL, NULL, view);

  g_signal_handlers_block_matched (GTK_OBJECT(button), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view); 
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button),
				view->sample->play_head->looping);
  g_signal_handlers_unblock_matched (GTK_OBJECT(button), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);

  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  view->loop_toggle = button;


  /* Play */

  tool_hbox = gtk_hbox_new (TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), tool_hbox, TRUE, TRUE, 0);
  gtk_widget_show (tool_hbox);

  button
    = create_pixmap_button (window, playpaus_xpm,
			    /* _("Play all / Pause    [Ctrl+Space / Enter]"),*/
			    _("Play all / Pause"),
			    style_green_grey, VIEW_TOOLBAR_TOGGLE_BUTTON,
			    G_CALLBACK (play_view_button_cb),
			    NULL, NULL, view);
				
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  NOALLOC(button);

  view->play_toggle = button;

  /* Play selection */
  
  button
    = create_pixmap_button (window, playpsel_xpm,
			    /*_("Play selection / Pause    [Space / Enter]"),*/
			    _("Play selection / Pause"),
			    style_green_grey, VIEW_TOOLBAR_TOGGLE_BUTTON,
			    G_CALLBACK (play_view_sel_button_cb),
			    NULL, NULL, view);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  NOALLOC(button);

  view->play_sel_toggle = button;

  /* Stop */

  button
    = create_pixmap_button (window, stop_xpm,
			    /*_("Stop playback    [Space]"),*/
			    _("Stop playback"),
			    style_green_grey, VIEW_TOOLBAR_BUTTON,
			    G_CALLBACK (stop_playback_cb),
			    NULL, NULL, view);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  NOALLOC(button);

  /* Beginning */

  tool_hbox = gtk_hbox_new (TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), tool_hbox, FALSE, TRUE, 0);
  gtk_widget_show (tool_hbox);

  button
    = create_pixmap_button (window, prevtrk_xpm,
			    _("Go to beginning"),
			    style_green_grey, VIEW_TOOLBAR_BUTTON,
			    G_CALLBACK (goto_start_cb), NULL, NULL, view);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  NOALLOC(button);

  /* Rewind */

  button
    = create_pixmap_button (window, rew_xpm, _("Rewind"),
			    style_green_grey, VIEW_TOOLBAR_BUTTON,
			    NULL,
			    G_CALLBACK (rewind_pressed_cb), G_CALLBACK (repeater_released_cb), view);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  NOALLOC(button);

  /* Fast forward */

  button
    = create_pixmap_button (window, ff_xpm, _("Fast forward"),
			    style_green_grey, VIEW_TOOLBAR_BUTTON,
			    NULL,
			    G_CALLBACK (ffwd_pressed_cb), G_CALLBACK (repeater_released_cb), view);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  NOALLOC(button);

  /* End */

  button
    = create_pixmap_button (window, nexttrk_xpm,
			    _("Go to the end"),
			    style_green_grey, VIEW_TOOLBAR_BUTTON,
			    G_CALLBACK (goto_end_cb), NULL, NULL, view);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  NOALLOC(button);

  /* gain */

  tool_hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (tool_hbox);

  button = create_pixmap_button (window, mute_xpm,
				 _("Muted playback (toggle)"),
				 style_green_grey, VIEW_TOOLBAR_TOGGLE_BUTTON,
				 G_CALLBACK (mute_toggled_cb), NULL, NULL, view);

  g_signal_handlers_block_matched (GTK_OBJECT(button), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button),
				view->sample->play_head->mute);
  g_signal_handlers_unblock_matched (GTK_OBJECT(button), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);

  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  gtk_widget_show (button);

  view->mute_toggle = button;

  pixmap = create_widget_from_xpm (window, vol_xpm);
  gtk_box_pack_start (GTK_BOX (tool_hbox), pixmap, FALSE, FALSE, 0);
  gtk_widget_show (pixmap);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, pixmap,
			_("Playback gain slider (volume)"), NULL);

  gain_adj = gtk_adjustment_new (view->sample->play_head->gain*10.0,/* value */
				  0.0,   /* lower */
				  10.0,  /* upper */
				  0.17,  /* step incr */
				  1.6,   /* page incr */
				  0.0    /* page size */
				  );
  view->gain_adj = gain_adj;

  gain_hscale = gtk_hscale_new (GTK_ADJUSTMENT(gain_adj));
  gtk_scale_set_draw_value (GTK_SCALE(gain_hscale), FALSE);
  gtk_range_set_update_policy (GTK_RANGE(gain_hscale), GTK_UPDATE_CONTINUOUS);
  gtk_widget_show (gain_hscale);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, gain_hscale,
			_("Playback gain slider (volume)"), NULL);

  gtk_widget_set_style (gain_hscale, style_green_grey);

  g_signal_connect (G_OBJECT(gain_adj), "value_changed",
		      G_CALLBACK(view_gain_changed_cb), view);

  gtk_box_pack_start (GTK_BOX(tool_hbox), gain_hscale, TRUE, TRUE, 0);

  /* Monitor */

  button = create_pixmap_button (window, headphones_xpm,
				 _("Monitor (toggle)"),
				 style_green_grey, VIEW_TOOLBAR_TOGGLE_BUTTON,
				G_CALLBACK (monitor_toggled_cb), NULL, NULL, view);

  g_signal_handlers_block_matched (GTK_OBJECT(button), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view); 
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button),
				view->sample->play_head->monitor);
  g_signal_handlers_unblock_matched (GTK_OBJECT(button), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view); 

  gtk_box_pack_start (GTK_BOX (tool_hbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);

  view->monitor_toggle = button;

  NOALLOC(button);

  /* SYNC */

  button = gtk_button_new_with_label (_("SYNC"));
  gtk_widget_set_style (button, style_green_grey);
  gtk_box_pack_start (GTK_BOX (tool_hbox), button, FALSE, TRUE, 0);
  /*gtk_widget_show (button);*/

  NOALLOC(button);

  gtk_box_pack_start (GTK_BOX (hbox), tool_hbox, TRUE, TRUE, 0);

  /* Status line */

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

  tool_hbox = gtk_hbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER(frame), tool_hbox);
  gtk_widget_show (tool_hbox);

  pixmap = create_widget_from_xpm (window, mouse_xpm);
  gtk_box_pack_start (GTK_BOX(tool_hbox), pixmap, FALSE, FALSE, 0);
  gtk_widget_show (pixmap);

  label = gtk_label_new (NO_TIME);
  gtk_box_pack_start (GTK_BOX(tool_hbox), label, FALSE, FALSE, 0);
  
 /* connect hack_max_label_width_cb to the theme change signal so label is 
  * kept at an appropriate size regardless of font or theme changes. 
  *
  * replace with gtk_label_set_width_chars() when GTK+-2.6 is used by mainstream distro's */
  g_signal_connect (G_OBJECT(label), "style_set", G_CALLBACK(hack_max_label_width_cb), NULL);
   
  gtk_widget_show (label); 
  view->pos = label;
  

  /* progress bar */
  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX(hbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  tool_hbox = gtk_hbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER(frame), tool_hbox);
  gtk_widget_show (tool_hbox);

  progress = gtk_progress_bar_new ();
  gtk_box_pack_start (GTK_BOX(tool_hbox), progress, TRUE, TRUE, 0);
  gtk_progress_set_show_text (GTK_PROGRESS(progress), TRUE);
  gtk_widget_show (progress);

  view->progress = progress;

  button = gtk_button_new_with_label (_("Cancel"));
  gtk_box_pack_start (GTK_BOX(tool_hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK(cancel_cb), view);

  NOREADY(button);

  /* status */
  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX(hbox), frame, FALSE, TRUE, 0);
  gtk_widget_show (frame);

  tool_hbox = gtk_hbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER(frame), tool_hbox);
  gtk_widget_show (tool_hbox);

  label = gtk_label_new ("Sweep " VERSION);
  gtk_box_pack_start (GTK_BOX(tool_hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);
  view->status = label;

  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX(tool_hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK(show_info_dialog_cb), view);

  pixmap = create_widget_from_xpm (window, info_xpm);
  gtk_container_add (GTK_CONTAINER(button), pixmap);
  gtk_widget_show (pixmap);

  /* Had to wait until view->display was created before
   * setting the menus up
   */

  view->menu = gtk_menu_new ();
  accel_group = create_view_menu (view, view->menu);
  gtk_window_add_accel_group (GTK_WINDOW(view->window), accel_group);

  create_view_menu (view, view->menubar);

  view->menu_sel = create_context_menu_sel (view);
  view->menu_point = create_context_menu_point (view);

  g_signal_connect_swapped (G_OBJECT(menu_button),
			     "button_press_event",
			     G_CALLBACK(menu_button_handler),
			     G_OBJECT(view->menu));

  /* Had to wait till view->display was created to set these up */

  g_signal_connect (G_OBJECT(view->display),
		      "mouse-offset-changed",
		      G_CALLBACK(view_set_pos_indicator_cb),
		      view->display);
		      
#if 0
  g_signal_connect_swapped (G_OBJECT(view->display),
			     "motion_notify_event",
			     G_CALLBACK(view_set_pos_indicator_cb),
			     G_OBJECT(view->display));
#endif

  if (sample->sounddata->sels)
    sample_display_start_marching_ants (SAMPLE_DISPLAY(view->display));

  view_refresh_title(view);

  view_default_status(view);

  view_refresh_tool_buttons (view);

  view_refresh_edit_mode (view);

  gtk_widget_show(window);

  view_zoom_normal (view);

  return view;
}

sw_view *
view_new_all (sw_sample * sample, gfloat gain)
{
  return view_new(sample, 0, sample->sounddata->nr_frames, gain);
}


void
view_popup_context_menu (sw_view * view, guint button, guint32 activate_time)
{
  GtkWidget * menu;

  if (view->sample->sounddata->sels == NULL) {
    menu = view->menu_point;
  } else {
    menu = view->menu_sel;
  }

  gtk_menu_popup (GTK_MENU(menu), NULL, NULL, NULL, NULL,
		  button, activate_time);
}

/*
 * view_set_ends (v, start, end)
 *
 * set the endpoints shown by this view.
 */
void
view_set_ends (sw_view * view, sw_framecount_t start, sw_framecount_t end)
{
  GtkAdjustment * adj = GTK_ADJUSTMENT(view->adj);
  GtkWidget * entry;
  sw_framecount_t orig_length;
  sw_time_t length;
#define BUF_LEN 16
  gchar buf[BUF_LEN];
  gfloat step;

  /* Clamp view to within bounds of sample */
  orig_length = end - start;

  if(end > view->sample->sounddata->nr_frames) {
    end = view->sample->sounddata->nr_frames;
    start = end - orig_length;
  }
  if(start < 0)
    start = 0;

  /* Update duration displayed in zoom combo */
  length = frames_to_time (view->sample->sounddata->format, end-start);
  snprint_time (buf, BUF_LEN, length);

  entry = GTK_COMBO(view->zoom_combo)->entry;
 
  g_signal_handlers_block_matched (G_OBJECT(entry), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, view);
  gtk_entry_set_text (GTK_ENTRY(entry), buf);
  g_signal_handlers_unblock_matched (G_OBJECT(entry), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, view);

  /* This check used to be at at the start of this function, but by
   * putting it down here we ensure the zoom combo displays the correct
   * length, and not a string like "All". */
  if (start == view->start && end == view->end) return;

  /* Update main scrollbar */
  step = ((gfloat)(end - start)) / 10.0;
  if (step < 1.0) step = 1.0;

  adj->value = (gfloat)start;
  adj->step_increment = step;
  adj->page_increment = (gfloat)(end - start);
  adj->page_size = (gfloat)(end - start);

  gtk_adjustment_changed(adj);

  /* Update ruler */
  gtk_ruler_set_range (GTK_RULER(view->time_ruler),
		       start, end,
		       start, end);

  /* Update title etc. */
  view_refresh_title(view);
  view_refresh_display(view);

#undef BUF_LEN
}

void
view_set_vzoom (sw_view * view, sw_audio_t low, sw_audio_t high)
{
  sw_audio_t length;

  length = high - low;

  if (length > 2.0) {
    low = -1.0;
    high = 1.0;
  } else if (low < -1.0) {
    low = -1.0;
    high = low + length;
  } else if (high > 1.0) {
    high = 1.0;
    low = high - length;
  }

  view->vlow = low;
  view->vhigh = high;

  view_refresh_db_rulers (view);
  view_refresh_display (view);
}

void
view_vzoom_in (sw_view * view, double ratio)
{
  sw_audio_t oz, z;
  sw_audio_t nhigh, nlow;

  oz = view->vhigh - view->vlow;
  z = (sw_audio_t)((gdouble)oz / ratio);

  nlow = view->vlow + (oz - z)/2;
  nhigh = nlow + z;

  view_set_vzoom (view, nlow, nhigh);
}

void
view_vzoom_out (sw_view * view, double ratio)
{
  sw_audio_t oz, z;
  sw_audio_t nhigh, nlow;

  oz = view->vhigh - view->vlow;
  z = (sw_audio_t)((gdouble)oz * ratio);

  nlow = view->vlow + (oz - z)/2;
  nhigh = nlow + z;

  view_set_vzoom (view, nlow, nhigh);
}

static void
view_clamp_to_offset (sw_view * view, sw_framecount_t * start,
		      sw_framecount_t * end, sw_framecount_t offset)
{
  sw_framecount_t length, width, nr_frames;

  length = *end - *start;
  width = SAMPLE_DISPLAY(view->display)->width;
  nr_frames = view->sample->sounddata->nr_frames;

#ifdef SCROLL_SMOOTHLY
  if (!view->sample->play_head->scrubbing) {
#else
  if (offset < *start || offset > *end) {
#endif
      if (offset > nr_frames - length/2) {
	*start = nr_frames - length;
	*end = nr_frames;
      } else if (offset < length/2) {
	*start = 0;
	*end = length;
      } else if (length <= width * 8) {
	*start = offset - length/2;
	*end = *start + length;
      } else {
#ifdef SCROLL_SMOOTHLY
	*start = offset - length/2;
	*end = *start + length;
#else
	if (view->sample->play_head->reverse) {
	  *end = offset;
	  *start = *end - length;
	} else {
	  *start = offset;
	  *end = *start + length;
	}
#endif
      }
    }
}

void
view_zoom_to_playmarker (sw_view * view)
{
  sw_framecount_t start, end;

  g_assert (view->following);

  start = view->start;
  end = view->end;

  view_clamp_to_offset (view, &start, &end, view->sample->user_offset);
  view_set_ends (view, start, end);
}

void
view_zoom_to_offset (sw_view * view, sw_framecount_t offset)
{
  sw_framecount_t start, end;

  start = view->start;
  end = view->end;

  view_clamp_to_offset (view, &start, &end, offset);

  view_set_ends (view, start, end);
}

void
view_center_on (sw_view * view, sw_framecount_t offset)
{
  sw_framecount_t vlen2;

  vlen2 = (view->end - view->start) / 2;

  view_set_ends (view, offset - vlen2, offset + vlen2);
}

void
view_zoom_normal (sw_view * view)
{
  sw_framecount_t length;

  length = MIN (view->sample->sounddata->nr_frames,
		SAMPLE_DISPLAY(view->display)->width * 1024);
  view_zoom_length (view, length);
  view_center_on (view, view->sample->user_offset);
}

void
view_zoom_length (sw_view * view, sw_framecount_t length)
{
  sw_framecount_t center;

  center = (view->end + view->start) / 2;

  view_set_ends (view, center - length/2, center + length/2);
}

void
view_zoom_in (sw_view * view, double ratio)
{
  sw_framecount_t nstart, nend, nlength, olength, offset, nr_frames;
  gboolean do_following;
  SampleDisplay * sd = SAMPLE_DISPLAY(view->display);

  olength = view->end - view->start;
  nlength = (sw_framecount_t)((double)olength / ratio);

  if(nlength <= DEFAULT_MIN_ZOOM) return;

  offset = view->sample->user_offset;

  /* zoom centred on the play marker if its visible, otherwise on
   * the middle of the view */
  do_following =
    (view->following && offset >= view->start && offset <= view->end);

  if (do_following) {
    nr_frames = view->sample->sounddata->nr_frames;

    if (offset >= view->start && offset < view->end) {
      nstart = offset - (offset - view->start) / ratio;
    } else {
      nstart = offset - nlength/2;
    }

    if (nstart > nr_frames - nlength) {
      nstart = nr_frames - nlength;
    } else if (nstart < 0) {
      nstart = 0;
    }
  } else {
    nstart = view->start + (olength - nlength)/2;
  }

  sample_display_set_cursor(sd, sweep_cursors[SWEEP_CURSOR_ZOOM_IN]);

  nend = nstart+nlength;

  /*
  if (view->following) {
    view_clamp_to_playmarker (view, &nstart, &nend);
  }
  */

  view_set_ends(view, nstart, nend);
}

void
view_zoom_out (sw_view * view, double ratio)
{
  sw_framecount_t nstart, nend, nlength, olength, offset, nr_frames;
  gboolean do_following;
  SampleDisplay * sd = SAMPLE_DISPLAY(view->display);

  olength = view->end - view->start;
  nlength = (sw_framecount_t)((double)olength * ratio);

  if (nlength < 0) return; /* sw_framecount_t multiplication overflow */

  offset = view->sample->user_offset;

  /* zoom centred on the play marker if it's visible, otherwise on
   * the middle of the view */
  do_following =
    (view->following && offset >= view->start && offset <= view->end);

  if (do_following) {
    nr_frames = view->sample->sounddata->nr_frames;

    if (offset >= view->start && offset < view->end) {
      nstart = offset - (offset - view->start) * ratio;
    } else {
      nstart = offset - nlength/2;
    }

    if (nstart > nr_frames - nlength) {
      nstart = nr_frames - nlength;
    } else if (nstart < 0) {
      nstart = 0;
    }
  } else {
    if (nlength > olength) {
      nstart = view->start - (nlength - olength)/2;
    } else {
      nstart = view->start + (olength - nlength)/2;
    }
  }

  if (nstart == view->start && (nstart+nlength) == view->end)
    return;

  sample_display_set_cursor(sd, sweep_cursors[SWEEP_CURSOR_ZOOM_OUT]);

  nend = nstart + nlength;
  view_set_ends(view, nstart, nend);
}

void
view_zoom_to_sel (sw_view * view)
{
  GList * gl;
  sw_sel * sel;
  gint sel_min, sel_max;

  if(!view->sample->sounddata->sels) return;

  gl = view->sample->sounddata->sels;

  sel = (sw_sel *)gl->data;
  sel_min = sel->sel_start;

  if (gl->next)
    for (gl = gl->next; gl->next; gl = gl->next);

  sel = (sw_sel *)gl->data;
  sel_max = sel->sel_end;

  view_set_ends(view, sel_min, sel_max);
}

void
view_zoom_left (sw_view * view)
{
  GtkAdjustment * adj = GTK_ADJUSTMENT(view->adj);

  adj->value -= adj->page_size;
  if(adj->value < adj->lower) {
    adj->value = adj->lower;
  }

  gtk_adjustment_value_changed (GTK_ADJUSTMENT(adj));
}

void
view_zoom_right (sw_view * view)
{
  GtkAdjustment * adj = GTK_ADJUSTMENT(view->adj);

  adj->value += adj->page_size;
  if(adj->value > adj->upper) {
    adj->value = adj->upper;
  }

  gtk_adjustment_value_changed (GTK_ADJUSTMENT(adj));
}

void
view_zoom_all (sw_view * view)
{
  sw_sample * s;

  s = view->sample;

  view_set_ends(view, 0, s->sounddata->nr_frames);
}

static void
view_store_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;
  gint slot;

  slot = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(widget), "default"));
  if (slot < 0 || slot > 9) return;

  sample->stored_views[slot].start = view->start;
  sample->stored_views[slot].end = view->end;

  sample_set_tmp_message (sample, _("Remembered as area %d"), slot);
}

static void
view_retrieve_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;
  sw_sample * sample = view->sample;
  gint slot;
  sw_framecount_t start, end;

  slot = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(widget), "default"));
  if (slot < 0 || slot > 9) return;

  start = sample->stored_views[slot].start;
  end =	sample->stored_views[slot].end;

  if (start == end) {
    sample_set_tmp_message (sample, _("No area remembered as %d"), slot);
  } else {
    view_set_ends (view, start, end);
    sample_set_tmp_message (sample, _("Zoomed to area %d"), slot);
  }
}

void
view_refresh_edit_mode (sw_view * view)
{
  GList * gl;
  GtkWidget * w;

  if (view->sample == NULL) return;

  switch (view->sample->edit_mode) {
  case SWEEP_EDIT_MODE_READY:
    for (gl = view->noready_widgets; gl; gl = gl->next) {
      w = (GtkWidget *)gl->data;
      gtk_widget_set_sensitive (w, FALSE);
    }
    for (gl = view->nomodify_widgets; gl; gl = gl->next) {
      w = (GtkWidget *)gl->data;
      gtk_widget_set_sensitive (w, TRUE);
    }
    for (gl = view->noalloc_widgets; gl; gl = gl->next) {
      w = (GtkWidget *)gl->data;
      gtk_widget_set_sensitive (w, TRUE);
    }

    if (view->sample->tmp_message_active) {
      view_set_tmp_message (view, view->sample->last_tmp_message);
    } else {
      view_set_progress_ready (view);
    }

    break;
  case SWEEP_EDIT_MODE_META:
  case SWEEP_EDIT_MODE_FILTER:
    for (gl = view->noready_widgets; gl; gl = gl->next) {
      w = (GtkWidget *)gl->data;
      gtk_widget_set_sensitive (w, TRUE);
    }
    for (gl = view->nomodify_widgets; gl; gl = gl->next) {
      w = (GtkWidget *)gl->data;
      gtk_widget_set_sensitive (w, FALSE);
    }
    for (gl = view->noalloc_widgets; gl; gl = gl->next) {
      w = (GtkWidget *)gl->data;
      gtk_widget_set_sensitive (w, TRUE);
    }
    break;
  case SWEEP_EDIT_MODE_ALLOC:
    for (gl = view->noready_widgets; gl; gl = gl->next) {
      w = (GtkWidget *)gl->data;
      gtk_widget_set_sensitive (w, TRUE);
    }
    for (gl = view->nomodify_widgets; gl; gl = gl->next) {
      w = (GtkWidget *)gl->data;
      gtk_widget_set_sensitive (w, FALSE);
    }
    for (gl = view->noalloc_widgets; gl; gl = gl->next) {
      w = (GtkWidget *)gl->data;
      gtk_widget_set_sensitive (w, FALSE);
    }
    break;
  default:
    g_assert_not_reached ();
    break;
  }
}

void
view_refresh_playmode (sw_view * view)
{
  sw_head * head = view->sample->play_head;
  gboolean playing, playing_sel;

  g_mutex_lock (head->head_mutex);
  playing = head->going && !head->restricted;
  playing_sel = head->going && head->restricted;
  g_mutex_unlock (head->head_mutex);

  g_signal_handlers_block_matched (GTK_OBJECT(view->play_toggle), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
  gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON(view->play_toggle),
			       playing);
  g_signal_handlers_unblock_matched (GTK_OBJECT(view->play_toggle), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);

  g_signal_handlers_block_matched (GTK_OBJECT(view->play_sel_toggle), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
  gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON(view->play_sel_toggle),
			       playing_sel);
  g_signal_handlers_unblock_matched (GTK_OBJECT(view->play_sel_toggle), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);

  /* If we're stopped, don't show the play line any more */
  view_refresh_display (view);

  view_refresh_offset_indicators (view);
}

void
view_refresh_offset_indicators (sw_view * view)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(view->display);
  sw_sample * sample = view->sample;
  sw_framecount_t offset;
  static int rate_limit = 0;


#define BUF_LEN 16
  char buf[BUF_LEN];

  offset = (sample->play_head->going ?
	    (sw_framecount_t)sample->play_head->offset :
	    sample->user_offset);

  snprint_time (buf, BUF_LEN,
		frames_to_time (sample->sounddata->format, offset));
/* cheesy rate limiter. ugly in operation. limits pango damage */
if (rate_limit >= 3) {
	rate_limit=0;
#ifdef PLAYPOS_LABEL
  gtk_label_set_text (GTK_LABEL(view->play_pos), buf);
#else
  gtk_entry_set_text (GTK_ENTRY(view->play_pos), buf);
#endif

}
++rate_limit;
#undef BUF_LEN

  sample_display_refresh_play_marker (sd);
  sample_display_refresh_user_marker (sd);

  if (view->following) {
    view_zoom_to_offset (view, offset);
  }
}

void
view_refresh_rec_offset_indicators (sw_view * view)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(view->display);

  sample_display_refresh_rec_marker (sd);
}


/* format string can include %p (current percentage) or %v (current value) */
void
view_set_progress_text (sw_view * view, gchar * text)
{
  if (view == NULL) return;

  gtk_progress_set_format_string (GTK_PROGRESS(view->progress), text);
}

void
view_set_progress_percent (sw_view * view, gint percent)
{
  if (view == NULL) return;

  gtk_progress_set_percentage (GTK_PROGRESS(view->progress),
			       (gfloat)percent/100.0);
}

void
view_set_tmp_message (sw_view * view, gchar * message)
{
  if (view == NULL) return;

  gtk_progress_set_format_string (GTK_PROGRESS(view->progress), message);
  gtk_progress_set_percentage (GTK_PROGRESS(view->progress), 0.0);
}

void
view_set_progress_ready (sw_view * view)
{
#define BUF_LEN 64
  static gchar buf[BUF_LEN];

  if (view == NULL) return;

  snprintf (buf, BUF_LEN, "%s%s - %s",
	    view->sample->modified ? "*" : "",
	    g_basename (view->sample->pathname),
	    view->sample->play_head->scrubbing ? _("Scrub!") : _("Ready"));

#if 0
  if (view->sample->play_head->scrubbing) {
    snprintf (buf, BUF_LEN, "Sweep %s - %s", VERSION, _("Scrub!"));
  } else {
    snprintf (buf, BUF_LEN, "Sweep %s - %s", VERSION, _("Ready"));
  }
#endif

  gtk_progress_set_format_string (GTK_PROGRESS(view->progress), buf);
  gtk_progress_set_percentage (GTK_PROGRESS(view->progress), 0.0);
#undef BUF_LEN
}

void
view_set_following (sw_view * view, gboolean following)
{
  view->following = following;

  g_signal_handlers_block_matched (GTK_OBJECT(view->follow_toggle), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(view->follow_toggle),
				view->following);
  g_signal_handlers_unblock_matched (GTK_OBJECT(view->follow_toggle), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);

  g_signal_handlers_block_matched (GTK_OBJECT(view->follow_checkmenu), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(view->follow_checkmenu),
				  view->following);
  g_signal_handlers_unblock_matched (GTK_OBJECT(view->follow_checkmenu), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);

  if (view->following) {
    view_zoom_to_playmarker (view);
  }
}

static void
view_close_ok_cb (GtkWidget * widget, gpointer data)
{
  sw_view * view = (sw_view *)data;

  gtk_widget_destroy(view->window);
  g_free(view);
}

void
view_close (sw_view * view)
{
  sw_sample * sample = view->sample;
#define BUF_LEN 256
  char buf[BUF_LEN];

  if (sample->modified && g_list_length (sample->views) == 1) {
    snprintf (buf, BUF_LEN, _("%s has been modified. Close anyway?"),
	      g_basename (sample->pathname));
    question_dialog_new (sample, _("File modified"), buf,
			 _("Close"), _("Don't close"),
			 G_CALLBACK (view_close_ok_cb), view, NULL, NULL,
			 SWEEP_EDIT_MODE_ALLOC);
  } else {
    view_close_ok_cb (NULL, view);
  }
}

void
view_volume_increase (sw_view * view)
{
  GtkAdjustment * adj = GTK_ADJUSTMENT(view->gain_adj);

  adj->value += 0.1;
  if (adj->value >= 1.0) adj->value = 1.0;

  gtk_adjustment_value_changed (adj);
}

void
view_volume_decrease (sw_view * view)
{
  GtkAdjustment * adj = GTK_ADJUSTMENT(view->gain_adj);

  adj->value -= 0.1;
  if (adj->value <= 0.0) adj->value = 0.0;

  gtk_adjustment_value_changed (adj);
}

void
view_refresh_title (sw_view * view)
{
  sw_sample * s = (sw_sample *)view->sample;

#define BUF_LEN 256
  char buf[BUF_LEN];

  if (s->sounddata->nr_frames > 0) {
    snprintf(buf, BUF_LEN,
#if 0
	     "%s (%dHz %s) %0d%% - Sweep " VERSION,
	     s->filename ? s->filename : _("Untitled"),
	     s->sounddata->format->rate,
	     s->sounddata->format->channels == 1 ? _("Mono") : _("Stereo"),
#else
	     "%s%s %0d%% - Sweep " VERSION,
	     s->modified ? _("*") : "",
	     s->pathname ? g_basename (s->pathname) : _("Untitled"),
#endif
	     100 * (view->end - view->start) / s->sounddata->nr_frames);
  } else {
    snprintf(buf, BUF_LEN,
#if 0
	     "%s (%dHz %s) %s - Sweep " VERSION,
	     s->filename ? s->filename : _("Untitled"),
	     s->sounddata->format->rate,
	     s->sounddata->format->channels == 1 ? _("Mono") : _("Stereo"),
#else
	     "%s%s %s - Sweep " VERSION,
	     s->modified ? _("*") : "",
	     s->pathname ? g_basename (s->pathname) : _("Untitled"),
#endif
	     _("Empty"));
  }

  gtk_window_set_title (GTK_WINDOW(view->window), buf);
#undef BUF_LEN
}

void
view_default_status (sw_view * view)
{
  sw_sample * s = (sw_sample *)view->sample;
  sw_sounddata * sounddata = s->sounddata;

#define BYTE_BUF_LEN 16
  char byte_buf[BYTE_BUF_LEN];

#define TIME_BUF_LEN 16
  char time_buf[TIME_BUF_LEN];

#define CHAN_BUF_LEN 16
  char chan_buf[CHAN_BUF_LEN];

#define BUF_LEN 256
  char buf [BUF_LEN];

  snprint_bytes (byte_buf, BYTE_BUF_LEN,
		 frames_to_bytes (sounddata->format, sounddata->nr_frames));
  
  snprint_time (time_buf, TIME_BUF_LEN,
		frames_to_time (sounddata->format, sounddata->nr_frames));

  switch (s->sounddata->format->channels) {
  case 1:
    snprintf (chan_buf, CHAN_BUF_LEN, _("Mono"));
    break;
  case 2:
    snprintf (chan_buf, CHAN_BUF_LEN, _("Stereo"));
    break;
  default:
    snprintf (chan_buf, CHAN_BUF_LEN, "%d %s", s->sounddata->format->channels,
	      _("channels"));
    break;
  }

  snprintf (buf, BUF_LEN,
	    "%dHz %s [%s]",
	    s->sounddata->format->rate,
	    chan_buf, time_buf);

  gtk_label_set_text (GTK_LABEL(view->status), buf);

#undef BUF_LEN
#undef BYTE_BUF_LEN
#undef TIME_BUF_LEN
}

void
view_refresh_tool_buttons (sw_view * v)
{
  GList * gl;
  GtkWidget * button;
  sw_tool_t tool;

  for (gl = v->tool_buttons; gl; gl = gl->next) {
    button = (GtkWidget *)gl->data;
    tool = (sw_tool_t) g_object_get_data (G_OBJECT(button), "default");

    g_signal_handlers_block_matched (GTK_OBJECT(button), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, v);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button),
				  (tool == v->current_tool));
    g_signal_handlers_unblock_matched (GTK_OBJECT(button), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, v);
  }
}

void
view_refresh_hruler (sw_view * v)
{
  gtk_ruler_set_range (GTK_RULER(v->time_ruler),
		       v->start, v->end,
		       v->start, v->end);
  time_ruler_set_format (TIME_RULER(v->time_ruler),
			 v->sample->sounddata->format);
}

void
view_refresh_display (sw_view * v)
{
  SampleDisplay * sd = SAMPLE_DISPLAY(v->display);

  sample_display_refresh(sd);
}

void
view_refresh_adjustment (sw_view * v)
{
  GtkAdjustment * adj = GTK_ADJUSTMENT(v->adj);
  gboolean changed = FALSE;

  adj->upper = (gfloat)v->sample->sounddata->nr_frames;
  if (adj->page_size > (gfloat)v->sample->sounddata->nr_frames) {
    adj->page_size = (gfloat)v->sample->sounddata->nr_frames;
    adj->value = 0;
    changed = TRUE;
  }

  if (adj->value > adj->upper - adj->page_size) {
    adj->value = adj->upper - adj->page_size;
    changed = TRUE;
  }

#if 0
  if (adj->page_size > adj->upper - adj->value)
    adj->page_size = adj->upper - adj->value;
#endif

#if 0
  if (v->end > v->sample->sounddata->nr_frames)
    v->end = v->sample->sounddata->nr_frames;
#endif 

  if (adj->page_increment == 0) {
    adj->page_increment = (gfloat)(v->end - v->start);
    changed = TRUE;
  }

  if (adj->page_size == 0) {
    adj->page_size = (gfloat)(v->end - v->start);
    changed = TRUE;
  }

  if (changed)
    gtk_adjustment_changed (adj);
}

void
view_refresh_looping (sw_view * view)
{
  g_signal_handlers_block_matched (GTK_OBJECT(view->loop_toggle), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(view->loop_toggle),
				view->sample->play_head->looping);
  g_signal_handlers_unblock_matched (GTK_OBJECT(view->loop_toggle), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);

  g_signal_handlers_block_matched (GTK_OBJECT(view->loop_checkmenu), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(view->loop_checkmenu),
				  view->sample->play_head->looping);
  g_signal_handlers_unblock_matched (GTK_OBJECT(view->loop_checkmenu), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
}

void
view_refresh_playrev (sw_view * view)
{
  g_signal_handlers_block_matched (GTK_OBJECT(view->playrev_toggle), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(view->playrev_toggle),
				view->sample->play_head->reverse);
  g_signal_handlers_unblock_matched (GTK_OBJECT(view->playrev_toggle), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);

  g_signal_handlers_block_matched (GTK_OBJECT(view->playrev_checkmenu), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(view->playrev_checkmenu),
				  view->sample->play_head->reverse);
  g_signal_handlers_unblock_matched (GTK_OBJECT(view->playrev_checkmenu), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
}

void
view_refresh_mute (sw_view * view)
{
  g_signal_handlers_block_matched (GTK_OBJECT(view->mute_toggle), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(view->mute_toggle),
				view->sample->play_head->mute);
  g_signal_handlers_unblock_matched (GTK_OBJECT(view->mute_toggle), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);

  g_signal_handlers_block_matched (GTK_OBJECT(view->mute_checkmenu), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(view->mute_checkmenu),
				  view->sample->play_head->mute);
  g_signal_handlers_unblock_matched (GTK_OBJECT(view->mute_checkmenu), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
}

void
view_refresh_monitor (sw_view * view)
{
  g_signal_handlers_block_matched (GTK_OBJECT(view->monitor_toggle), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(view->monitor_toggle),
				view->sample->play_head->monitor);
  g_signal_handlers_unblock_matched (GTK_OBJECT(view->monitor_toggle), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);

  g_signal_handlers_block_matched (GTK_OBJECT(view->monitor_checkmenu), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(view->monitor_checkmenu),
				  view->sample->play_head->monitor);
  g_signal_handlers_unblock_matched (GTK_OBJECT(view->monitor_checkmenu), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, view);
}

void
view_fix_adjustment (sw_view * v)
{
  GtkAdjustment * adj = GTK_ADJUSTMENT(v->adj);

  adj->value = (gfloat)v->start;
  adj->lower = (gfloat)0.0;
  adj->upper = (gfloat)v->sample->sounddata->nr_frames;
  adj->page_increment = (gfloat)(v->end - v->start);
  adj->page_size = (gfloat)(v->end - v->start);

  gtk_adjustment_changed (adj);
}

void
view_refresh (sw_view * v)
{
  view_refresh_adjustment (v);
  view_refresh_title (v);
  view_default_status (v);

  view_refresh_display (v);
  view_refresh_offset_indicators (v);
  view_refresh_tool_buttons (v);
  view_refresh_looping (v);
  view_refresh_playrev (v);

  view_refresh_channelops_menu (v);
  view_refresh_db_rulers (v);
}

void
view_sink_last_tmp_view (void)
{
  if (!last_tmp_view) return;

  sample_display_sink_tmp_sel(SAMPLE_DISPLAY(last_tmp_view->display));
}

void
view_clear_last_tmp_view (void)
{
  if (!last_tmp_view) return;

  sample_display_clear_sel(SAMPLE_DISPLAY(last_tmp_view->display));
}
