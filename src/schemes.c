/*
* Sweep, a sound wave editor.
*
* Copyright (C) 2000 Conrad Parker
* Copyright (C) 2008 Peter Shorthose
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
* Scheme loading, editor, menus and general scheme handling routine are here.
* the scheme object itself is in sweep-scheme.{c,h}
*/

/* 
* FIXME: add copyright info for borrowed code
*/

#include "schemes.h"
#include "callbacks.h"
#include "preferences.h"
#include "sample-display.h"
#include "view.h"

#include <sweep/sweep_i18n.h>
#include <stdlib.h>

gchar *default_colors[SCHEME_ELEMENT_LAST] = {
  "#fafafafaeded",
  "#80808a8ab8b8",
  "#dcdcaaaa7878",
  "#939393938c8c",
  "#dcdce6e6ffff",
  "#e6e600000000",
  "#f0f0e6e6f0f0",
  "#000000000000",
  "#a6a6a6a69a9a",
  "#646464646464",
  "#f0f0fafaf0f0",
  "#515165655151"
};

gchar *style_types[SCHEME_GTK_STYLE_LAST] = {
  "GTK_STYLE_NONE",
  "GTK_STYLE_FG",
  "GTK_STYLE_BG",
  "GTK_STYLE_LIGHT",
  "GTK_STYLE_MID",
  "GTK_STYLE_DARK",
  "GTK_STYLE_BASE",
  "GTK_STYLE_TEXT",
  "GTK_STYLE_TEXT_AA",
  "GTK_STYLE_BLACK",
  "GTK_STYLE_WHITE"
};

GtkWidget    * scheme_editor  = NULL;
GtkListStore * elements_store = NULL;
GtkComboBox  * schemes_combo  = NULL;
GtkWidget    * window         = NULL;
GtkWidget    * colorselection = NULL;
GtkWidget    * treeview       = NULL;
/* Part of a hack to dynamically refresh menus */
GtkMenuItem  * menu_item_proxy  = NULL; 

GdkPixbuf    * color_swatches[SCHEME_ELEMENT_LAST];
extern gchar * element_names[];
extern gchar * element_keys[];
SweepScheme  * default_scheme = NULL;
gboolean schemes_modified = FALSE; 
#define FOR_EACH_ELEMENT for (element = 0; element < SCHEME_ELEMENT_LAST; element++)

GdkColor *
copy_gdk_colour (GdkColor * color_src) 
{
  GdkColor * color_dest = g_new (GdkColor, 1);
    
  color_dest->red   = color_src->red;
  color_dest->green = color_src->green;
  color_dest->blue  = color_src->blue;
  color_dest->pixel = color_src->pixel;
    
  return color_dest;
}

GdkColor *
color_new_from_rgb (gint r, gint g, gint b) 
{
  GdkColor * color = g_new (GdkColor, 1);
    
  color->red   = r * 65535 / 255;
  color->green = g * 65535 / 255;
  color->blue  = b * 65535 / 255;
      
  return color;
}

/*
 * filename_color_hash
 *
 * Choose colour for this file based on the filename, such that
 * a file is loaded with the same colour each time it is edited.
 *
 * Idea due to Erik de Castro Lopo
 */
static int
filename_color_hash (char * filename)
{
  char * p;
  int i = 0;
  int length;

  if (filename == NULL) return -1;

  for (p = filename; *p; p++) i += (int)*p;
  
  length = g_list_length (schemes_list);
  if (length > 0)
    return (i % length);
  else
    return -1;
}

/* 
 * largely stolen from gtkcolorbutton.c in gtk.
 * creates a color preview for use in the editor.
 * i tried using the cell properties but the 
 * active selection is drawn over the cell background
 * colour, making it useless :/
 */
static void
fill_pixmap_from_scheme_color (GdkColor * scheme_color, GdkPixbuf ** pixbuf)
{
  gint     i, j, rowstride;
  gint     r, g ,b;
  guchar * pixels;
  gint     color[3];
  gint     height = 18;
  gint     width = 35;
    
  if (*pixbuf != NULL)
    g_object_unref (*pixbuf);
  *pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    
  r = scheme_color->red >> 8;
  g = scheme_color->green >> 8;
  b = scheme_color->blue >> 8;
    
  pixels = gdk_pixbuf_get_pixels (*pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (*pixbuf);
    
  for (j = 0; j < height; j++) 
  {    
    color[0] = r;
    color[1] = g;
    color[2] = b;
        
    for (i = 0; i < width; i++) 
    {
            
      *(pixels + j * rowstride + i * 3)     = color[0];
      *(pixels + j * rowstride + i * 3 + 1) = color[1];
      *(pixels + j * rowstride + i * 3 + 2) = color[2];
    }
        
  }
}

/*
 * returns a static system default scheme.
 * serves as a template for new schemes and
 * as a fallback if both system and user
 * schemes were deleted.
 */
SweepScheme *
schemes_get_scheme_system_default (void) 
{
  gint element;
  
  if (default_scheme == NULL) {
      
    default_scheme = sweep_scheme_new ();
    g_object_ref (default_scheme);  
      
     FOR_EACH_ELEMENT {
      
      gdk_color_parse (default_colors[element], default_scheme->scheme_colors[element]);
      default_scheme->element_enabled[element] = TRUE;
        
    }
      
    default_scheme->name     = g_strdup ("Default");
    default_scheme->modified = FALSE;
  
  }
  return default_scheme;
}

SweepScheme *
schemes_get_scheme_user_default (void) 
{
  gchar       * scheme_name;
  SweepScheme * scheme;
    
  scheme_name = prefs_get_string ("user-default-scheme");
    
  if (scheme_name != NULL) {
      
    if ((scheme = schemes_find_by_name (scheme_name)) != NULL)
      return scheme;
  }

  if ((scheme = schemes_get_nth (0)) != NULL) {
      
    prefs_set_string ("user-default-scheme", scheme->name);
    return scheme;
  }

  return schemes_get_scheme_system_default ();
}

SweepScheme *
schemes_get_scheme_from_filename (gchar * filename)
{
  SweepScheme * scheme = NULL;
  gint          index;
    
  index = filename_color_hash (filename);
  if (index > -1)
    scheme = schemes_get_nth (index);
    
  return (scheme ? scheme : schemes_get_scheme_system_default ());
}

SweepScheme *
schemes_get_scheme_random (void)
{
  SweepScheme * scheme = NULL;
  gint          max, index;
    
  max = (gint) g_list_length (schemes_list);
  if (max > 0) { 
    index = ((gint)random()) % max;
        
    scheme = schemes_get_nth (index);
  }
    
  return (scheme ? scheme : schemes_get_scheme_system_default ());
}

SweepScheme *
schemes_get_prefered_scheme (gchar * filename)
{
  switch (schemes_get_selection_method ())
  {
    case SCHEME_SELECT_DEFAULT:
      return schemes_get_scheme_user_default ();
      
    case SCHEME_SELECT_FILENAME:
      return schemes_get_scheme_from_filename (filename);
      
    case SCHEME_SELECT_RANDOM:
      return schemes_get_scheme_random ();
  
    default:
      return schemes_get_scheme_system_default ();
  }
}

void
schemes_refresh_combo (gint index) 
{
  GList        * list;
  GtkListStore * list_store;
  
  if (schemes_combo != NULL) {
      
    list_store = GTK_LIST_STORE (gtk_combo_box_get_model (schemes_combo));
    
    if (list_store != NULL)
      gtk_list_store_clear (list_store);
    
    for (list = schemes_list; list; list = list->next) {
        
      gtk_combo_box_append_text (GTK_COMBO_BOX (schemes_combo), 
                                  SWEEP_SCHEME (list->data)->name);
        
    }
    if ((gtk_combo_box_get_active (schemes_combo) == -1) && (index >= 0))
          gtk_combo_box_set_active (schemes_combo, index);
    if (gtk_combo_box_get_active (schemes_combo) == -1)
          /* second check for empty list. clear it if so. */
          schemes_refresh_list_store (-1);
  }   
}

gboolean
schemes_refresh_color_scheme_menu_cb (GtkMenuItem * menuitem, gpointer user_data)
{
  if ((menuitem == NULL) ||
     (!GTK_IS_MENU_ITEM (menuitem)))
    return FALSE;
                                        
  gtk_menu_item_set_submenu (menuitem, NULL);
  schemes_create_menu (GTK_WIDGET (menuitem), FALSE);
  gtk_widget_show_all (GTK_WIDGET (menuitem));
    
  return FALSE;
}

static gint
element_get_style_type (gchar * type)
{

        if (g_ascii_strcasecmp (type, "GTK_STYLE_NONE") == 0)
            return SCHEME_GTK_STYLE_NONE;
        if (g_ascii_strcasecmp (type, "GTK_STYLE_FG") == 0)
            return SCHEME_GTK_STYLE_FG;
        if (g_ascii_strcasecmp (type, "GTK_STYLE_BG") == 0)
            return SCHEME_GTK_STYLE_BG;
        if (g_ascii_strcasecmp (type, "GTK_STYLE_LIGHT") == 0)
            return SCHEME_GTK_STYLE_LIGHT;
        if (g_ascii_strcasecmp (type, "GTK_STYLE_DARK") == 0)
            return SCHEME_GTK_STYLE_DARK;
        if (g_ascii_strcasecmp (type, "GTK_STYLE_BASE") == 0)
            return SCHEME_GTK_STYLE_BASE;
        if (g_ascii_strcasecmp (type, "GTK_STYLE_TEXT") == 0)
            return SCHEME_GTK_STYLE_TEXT;
        if (g_ascii_strcasecmp (type, "GTK_STYLE_TEXT_AA") == 0)
            return SCHEME_GTK_STYLE_TEXT_AA;
        if (g_ascii_strcasecmp (type, "GTK_STYLE_BLACK") == 0)
            return SCHEME_GTK_STYLE_BLACK;
        if (g_ascii_strcasecmp (type, "GTK_STYLE_WHITE") == 0)
            return SCHEME_GTK_STYLE_WHITE;

            return SCHEME_GTK_STYLE_NONE;

}

static SweepScheme *
parse_scheme (GKeyFile * key_file,
              gchar    * group,
              gchar   ** keys,
              gint       length)
{
  SweepScheme * scheme;
  gchar      ** string_list;
  GError      * error = NULL;
  gsize         num_strings; 
  gint          element;
  gchar       * default_name;
    
  if (length != SCHEME_ELEMENT_LAST)
    return NULL;
    
  scheme = sweep_scheme_new ();
  scheme->name = g_strdup(group);
    
  FOR_EACH_ELEMENT {
        
    string_list = g_key_file_get_string_list (key_file,
                                     group,
                                     keys[element],
                                     &num_strings,
                                     &error);
    if ((string_list != NULL)  && 
      ((gint)num_strings == 3) &&  /* color, style-type and toggle */
      (gdk_color_parse (string_list[0], scheme->scheme_colors[element]))) {
   
      scheme->element_enabled[element] = 
       (g_ascii_strncasecmp (string_list[2], "ENABLED", 7) == 0) ? TRUE : FALSE;
    
      scheme->element_style[element]   = element_get_style_type (string_list[1]);
      
      default_name = prefs_get_string ("user-default-scheme");
      
      if (default_name != NULL) {
        if (g_utf8_collate (default_name, scheme->name) == 0)
          scheme->is_default = TRUE;
      }
      g_strfreev (string_list);
    } else {
      //unref scheme
      return NULL;
    }
  }
  return scheme;
}

void 
schemes_add_scheme (SweepScheme * scheme, gboolean prepend) 
{
  gboolean ret;
    
  if (scheme != NULL) {
      
    if (prepend)
      schemes_list = g_list_prepend (schemes_list, scheme);
    else
      schemes_list = g_list_append (schemes_list, scheme);
      
    schemes_refresh_combo ((prepend ? 0 : (g_list_length (schemes_list) - 1)));
      
    if (gtk_main_level() > 0) {
      /* trigger color schemes menu refresh */
      g_signal_emit_by_name ((gpointer)menu_item_proxy, "event", NULL, &ret, NULL);
    }
  }
}

void
schemes_remove_scheme (SweepScheme * scheme) 
{
  gboolean ret;
    
  // unref scheme triggering signals etc
  schemes_list = g_list_remove (schemes_list, scheme);
  //g_object_unref ((gpointer) scheme);
  schemes_modified = TRUE;
  schemes_refresh_combo (0);

  g_signal_emit_by_name ((gpointer)menu_item_proxy, "event", NULL, &ret, NULL);
  gtk_combo_box_set_active (schemes_combo, 0);
}

void schemes_copy_scheme (SweepScheme *scheme, gchar *newname)
{
  GtkWidget   * image;
  GtkWidget   * hbox;
  GtkWidget   * label;
  GtkWidget   * dialog;
  GtkWidget   * entry;
  gchar       * tmpstring;
  SweepScheme * scheme_copy;
  gint          response;
  gboolean      finished = FALSE;
    
  dialog =  gtk_dialog_new_with_buttons (_("Choose a name for this scheme"),
                                         GTK_WINDOW (window),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL,
                                         GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK,
                                         GTK_RESPONSE_OK,
                                         NULL);
    
  gtk_widget_set_size_request (dialog, 400, -1);
    
  entry  = gtk_entry_new_with_max_length (40);
    
  if (newname != NULL) {
    gtk_entry_set_text (GTK_ENTRY (entry), newname);
  } else {
    tmpstring = g_strconcat (_("Copy of "), scheme->name, NULL);
    
    gtk_entry_set_text (GTK_ENTRY (entry), tmpstring);
    g_free (tmpstring);
  }
  gtk_box_pack_start_defaults (GTK_BOX (GTK_DIALOG (dialog)->vbox), entry);

  gtk_widget_show_all (GTK_DIALOG (dialog)->vbox);
    
  hbox  = gtk_hbox_new (FALSE, 8);
  image = gtk_image_new_from_stock ("gtk-dialog-error", 
                                    GTK_ICON_SIZE_DIALOG);
  label = gtk_label_new (_("A scheme with that name already exists.\n"
                           "Please choose another name and try again."));  
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox,
                      FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 4);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
    
  scheme_copy = sweep_scheme_copy (scheme);

    
  while (finished == FALSE) {
      
  response = gtk_dialog_run (GTK_DIALOG (dialog));
    
  if (response == GTK_RESPONSE_OK) {
    
    g_free (scheme_copy->name);
    scheme_copy->name = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
    scheme_copy->modified = TRUE;

    if (schemes_find_by_name (scheme_copy->name) == NULL) {
        
      schemes_add_scheme (scheme_copy, FALSE);
      finished = TRUE;
        
    } else
      gtk_widget_show_all (hbox);
  } else
    finished = TRUE;   
  }
  
  gtk_widget_destroy (dialog);
  dialog = NULL;
}

gpointer
schemes_get_nth (gint n) 
{
  return g_list_nth_data (schemes_list, n);
}

SweepScheme *
schemes_find_by_name (gchar * name)
{
  GList       * list;
  gint          ret;
  SweepScheme * scheme = NULL;
    
  for (list = schemes_list; list; list = list->next)
  {
    scheme = SWEEP_SCHEME (list->data);
      
    ret = g_utf8_collate (name, scheme->name);
      
    if (ret == 0)
      return scheme;
  }
  return NULL;
}

static void
get_key_file_data (GKeyFile * key_file) 
{
  gchar ** groups;
  gchar ** keys;
  gsize    groups_length, keys_length;
  GError * error = NULL;
  gint     i;
  SweepScheme * scheme;
    
    
  groups = g_key_file_get_groups (key_file, &groups_length);
    
  if (groups != NULL) {
        
      i = (gint)groups_length - 1;
        
      while (i > -1) {
            
          keys = g_key_file_get_keys (key_file, groups[i], &keys_length, &error);
            
          if (keys != NULL) {
                
              scheme = parse_scheme (key_file, groups[i], keys, (gint)keys_length); 
                
              if (scheme != NULL)
                schemes_add_scheme (scheme, TRUE);
                
              g_strfreev(keys);
                
          }
            
          i--;
      }
        
      g_strfreev(groups);
  }
}

gboolean
schemes_were_modified (void) 
{
  GList * list;
    
  for (list = schemes_list; list; list = list->next) {
        
      if ((SWEEP_SCHEME (list->data)->modified) ||
          (schemes_modified == TRUE))
        return TRUE;
  }
    
  return FALSE;
}

gint 
schemes_get_selection_method (void) 
{
  gint * method_ptr = prefs_get_int ("scheme-selection-method");
  
  if (method_ptr == NULL)
    return SCHEME_SELECT_FILENAME; 

    gint method = *method_ptr;
    return method;
}

static void
schemes_load (void) 
{
  GKeyFile * key_file = NULL;
  GError   * error    = NULL;
  gchar    * schemes_path = NULL;
  gchar    * schemes_path_system = NULL;
  gboolean  key_file_loaded;

  schemes_path = g_strconcat (g_get_home_dir (),
                              "/.sweep/sweep-schemes.ini", NULL);

  key_file = g_key_file_new ();
  key_file_loaded = g_key_file_load_from_file (key_file, schemes_path,
                                 G_KEY_FILE_NONE, &error);
                                 
  if (key_file_loaded == TRUE) {
    get_key_file_data (key_file);
  } 
      
  if (schemes_path != NULL)
      g_free (schemes_path);
    
  if (key_file_loaded == FALSE) { /* loading failed. try the system copy */
      
    schemes_path_system = g_strconcat (PACKAGE_DATA_DIR, "/sweep-schemes.ini",
                                       NULL);
    key_file_loaded = g_key_file_load_from_file (key_file, schemes_path_system,
                                 G_KEY_FILE_NONE, &error); 
                                 
    if (key_file_loaded == TRUE) {
      get_key_file_data (key_file);
      schemes_modified = TRUE; /* mark modified to trigger an ini write on close */
    }
    if (schemes_path_system != NULL)  
      g_free (schemes_path_system);
  }
  
  g_key_file_free (key_file);
  
  if (key_file_loaded == FALSE) {
    /* no schemes loaded. show error message? */
  }
}

void
save_schemes (void) 
{
  GKeyFile    * key_file;
  GError      * error = NULL;
  GList       * list;
  gint          element;
  gchar       * string_list[3], *key_data;
  SweepScheme * scheme;
  gsize         length;
  gchar       * schemes_path;
    
  if (schemes_were_modified () == FALSE)
      return;

  key_file = g_key_file_new ();
  
  g_key_file_set_comment (key_file, NULL, NULL, _(" Generated by sweep. Do not edit"), &error);
                          
  for (list = schemes_list; list; list = list->next) {
      
    scheme = SWEEP_SCHEME(list->data);
      
    FOR_EACH_ELEMENT {
        
      string_list[0] = gdk_color_to_string (scheme->scheme_colors[element]);
      string_list[1] = style_types[scheme->element_style[element]];
      string_list[2] = 
         scheme->element_enabled ? g_strdup ("ENABLED") : g_strdup ("DISABLED");
      
      g_key_file_set_string_list (key_file, scheme->name,
                                  element_keys[element], 
                                  (const gchar **)&string_list, 3); 
      
      g_free (string_list[0]);
      g_free (string_list[2]);
      
    }
      
  }
  key_data = g_key_file_to_data (key_file, &length, &error);
  
  schemes_path = g_strconcat (g_get_home_dir (),
                              "/.sweep/sweep-schemes.ini", NULL);
    
  FILE * fp = fopen (schemes_path, "w");
    
  if (fp != NULL) {
        fprintf (fp, "%s", key_data);
        fclose (fp);
  } else {
    /* error writing schemes. complain bitterly. */
  }
    
  if (key_file != NULL) 
    g_key_file_free (key_file);
  if (schemes_path != NULL)
    g_free (schemes_path);
}

void
init_schemes (void) 
{
  schemes_load (); 
  menu_item_proxy = GTK_MENU_ITEM (gtk_menu_item_new ());
}

void
schemes_create_menu (GtkWidget * parent_menuitem,
                     gboolean connect_signals) 
{
  GtkWidget * menuitem;
  GtkWidget * submenu;
  GList     * list;
  sw_view   * view;

    
  submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(parent_menuitem), submenu);

  menuitem = gtk_menu_item_new_with_label (_("Show color scheme editor ..."));
  gtk_menu_append (GTK_MENU(submenu), menuitem);
  
  view = (sw_view *) g_object_get_data (G_OBJECT (parent_menuitem), "view");
  g_signal_connect (G_OBJECT(menuitem), "activate",
		     G_CALLBACK(schemes_show_editor_window_cb), view);
    
  menuitem = gtk_menu_item_new(); /* Separator */
  gtk_menu_append(GTK_MENU(submenu), menuitem);
  

  if (connect_signals)
    g_signal_connect_swapped (G_OBJECT (menu_item_proxy),
                              "event",
                              G_CALLBACK (schemes_refresh_color_scheme_menu_cb),
                              parent_menuitem);
  
    
  for (list = schemes_list; list; list = list->next) {
        
    menuitem = gtk_menu_item_new_with_label (SWEEP_SCHEME (list->data)->name);
    
    g_object_set_data (G_OBJECT(menuitem), "scheme", 
			                  list->data); /* scheme */

    gtk_menu_append (GTK_MENU(submenu), menuitem);
    g_signal_connect (G_OBJECT(menuitem), "activate",
		       G_CALLBACK(sample_set_color_cb), view);
        
    }
}


void
schemes_show_editor_window_cb (GtkMenuItem * menuitem,
                            gpointer user_data) 
{
  GtkWidget * editor;
  sw_view   * view;
  GList     * element;
  gint        index;
  
  if (window == NULL) {
      
    view = (sw_view *)user_data;
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  
    //attach_window_close_accel(window);
    
    gtk_window_set_title (GTK_WINDOW (window), _("Sweep: Color Scheme Options"));
    gtk_widget_set_size_request (window, 620, -1);
  
    
    g_signal_connect ((gpointer) window, "destroy",
                     G_CALLBACK(schemes_ed_destroy_cb),
                     NULL);
    g_signal_connect ((gpointer) window, "destroy",
                     G_CALLBACK(gtk_widget_destroyed),
                     &window);
    g_signal_connect ((gpointer) window, "delete_event",
                     G_CALLBACK(schemes_ed_delete_event_cb),
                     NULL);

    element = g_list_find (schemes_list, 
                           (SAMPLE_DISPLAY (view->display)->scheme ? 
                            SAMPLE_DISPLAY (view->display)->scheme :
                            view->sample->scheme));
    if (element)
      index = g_list_position (schemes_list, element);
    else
      index = 0;
      
    editor = schemes_create_editor (index);
    gtk_container_add (GTK_CONTAINER (window), editor);
    gtk_widget_show_all (window);
      
  } else
        
    gdk_window_raise (GDK_WINDOW (window->window));
}

static void
treeview_set_selected (GtkTreeView  * treeview, gint index, gint max)
{
  GtkTreeSelection * selection;
  GtkTreePath      * path;
  GtkTreeModel     * model;
  GtkTreeIter        iter;
  
  g_return_if_fail ((max < 0) || (GTK_IS_TREE_VIEW (treeview)));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

  if ((index < 0) || (index > max))  
    path = gtk_tree_path_new_first ();
  else
    path = gtk_tree_path_new_from_indices (index, -1);
  
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);
  gtk_tree_selection_select_iter (selection, &iter);
}

void
schemes_refresh_list_store (gint scheme_index) 
{
  GtkTreeIter   iter;
  gint          element;
  SweepScheme * scheme = NULL;
  
  scheme = g_list_nth_data (schemes_list, scheme_index);
  
  gtk_list_store_clear (elements_store);
    
  if (scheme == NULL)
    return;
  
  FOR_EACH_ELEMENT {
      
    fill_pixmap_from_scheme_color (scheme->scheme_colors[element], 
                                   &color_swatches[element]);
      
    gtk_list_store_append (elements_store, 
                           &iter);
    gtk_list_store_set (elements_store, 
                        &iter,
                        ELEMENT_NAME_COLUMN, element_names[element],
                        COLOR_SWATCH_COLUMN,  color_swatches[element],
                        SCHEME_OBJECT_COLUMN, scheme,
                        ELEMENT_NUMBER_COLUMN, element,
                        -1);      
  }
  treeview_set_selected (GTK_TREE_VIEW (treeview), 0, 0);
}

void
schemes_picker_set_edited_color (SweepScheme * scheme, gint element)
{
  if ((colorselection == NULL) ||
      (scheme == NULL) ||
      (element < 0) ||
      (element >= SCHEME_ELEMENT_LAST))
    return;

  g_signal_handlers_block_by_func     (colorselection, 
                                       schemes_ed_color_changed_cb,
                                       NULL);

  gtk_color_selection_set_current_color (GTK_COLOR_SELECTION (colorselection),
                                         scheme->scheme_colors[element]);
  gtk_color_selection_set_previous_color (GTK_COLOR_SELECTION (colorselection),
                                         scheme->scheme_colors[element]);
    
  g_signal_handlers_unblock_by_func     (colorselection, 
                                         schemes_ed_color_changed_cb,
                                         NULL);
}

static GtkWidget *
schemes_create_tree_view (void) 
{
  GtkCellRenderer   * renderer;
  GtkTreeViewColumn * column;
  
  
  elements_store = gtk_list_store_new (N_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_OBJECT, G_TYPE_STRING, G_TYPE_INT);
  treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (elements_store));
  
  renderer = gtk_cell_renderer_pixbuf_new ();
  
  column   = gtk_tree_view_column_new_with_attributes ("Color",
                                                       renderer,
                                                       "pixbuf", COLOR_SWATCH_COLUMN,
                                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW(treeview), column);
  renderer = gtk_cell_renderer_text_new ();
  
  column   = gtk_tree_view_column_new_with_attributes ("Scheme element",
                                                       renderer,
                                                       "text", ELEMENT_NAME_COLUMN,
                                                       NULL);
  
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW(treeview), column);
  

  
  return treeview;
}

void
schemes_set_active_element_color (GtkColorSelection * colorselection) 
{
  GtkTreeSelection * selection;
  GtkTreeModel     * model;
  GtkTreeIter        iter;
  GdkColor         * color;
  gint               element;
  SweepScheme      * scheme;
    
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    
  if (gtk_tree_selection_get_selected (selection,
                                        &model,
                                        &iter)) {
    gtk_tree_model_get (model,
                        &iter,
                        SCHEME_OBJECT_COLUMN, &scheme,
                        ELEMENT_NUMBER_COLUMN, &element,
                        -1);
    color = g_new (GdkColor, 1);
    gtk_color_selection_get_current_color (colorselection, 
                                          color);  
    sweep_scheme_set_element_color (scheme, element, color);
                                            
    fill_pixmap_from_scheme_color (scheme->scheme_colors[element], 
                                   &color_swatches[element]);
                                             
    gtk_list_store_set (GTK_LIST_STORE (model), 
                       &iter,
                        COLOR_SWATCH_COLUMN,  color_swatches[element],
                        -1);
    scheme->modified = TRUE;



    g_free (color);
  }
}

GtkWidget *
schemes_create_editor (gint index) 
{
  GtkWidget * editor_vbox;
  GtkWidget * general_vbox;
  GtkWidget * sel_options_vbox;
  GtkWidget * checkbutton;
  GtkWidget * color_picker;
  GtkWidget * treeview;
  GtkWidget * button;
  GtkWidget * frame;
  GtkWidget * hbuttonbox;
  GtkWidget * label;
  GtkWidget * notebook;
  GtkWidget * image;
  GtkWidget * hbox;
  GtkWidget * radiobuttons[SCHEME_SELECT_LAST];
  GtkTooltips * tooltips;
  GtkTreeSelection * selection;
  gint method;

  tooltips = gtk_tooltips_new ();
    
  scheme_editor = gtk_vbox_new (FALSE, 0);
  notebook      = gtk_notebook_new ();
    
  editor_vbox   = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (editor_vbox), 2);
    
  general_vbox  = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (general_vbox), 2);
    
  gtk_box_pack_start_defaults (GTK_BOX (scheme_editor), notebook);
  gtk_container_set_border_width (GTK_CONTAINER (notebook), 4);
 
  /* scheme editor notebook tab widgets */
    
  hbox = gtk_hbox_new (FALSE, 0);
  image = gtk_image_new_from_stock ("gtk-select-color", GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX (hbox), image, TRUE, TRUE, 0);
  gtk_misc_set_padding (GTK_MISC (image), 2, 0);
    
  label = gtk_label_new (_("Color scheme editor"));
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_misc_set_padding (GTK_MISC (label), 2, 0);
  gtk_widget_show_all (hbox);
        
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), editor_vbox, hbox);

    
  /* general notebook tab widgets */
    
  hbox = gtk_hbox_new (FALSE, 0);
  image = gtk_image_new_from_stock ("gtk-preferences", GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  gtk_misc_set_padding (GTK_MISC (image), 2, 0);

  label = gtk_label_new (_("General scheme options"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_misc_set_padding (GTK_MISC (label), 2, 0);
  gtk_widget_show_all (hbox);
    
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), general_vbox, hbox);
    
  /** scheme selection and operations **/  
    
  /* schemes combo and label */
    
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (editor_vbox), hbox, FALSE, FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 3);
    
  label = gtk_label_new (_("<b>Selected scheme</b>"));
  gtk_misc_set_padding (GTK_MISC (label), 4, 0);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (label), FALSE, FALSE, 0);
    
    
  schemes_combo = GTK_COMBO_BOX (gtk_combo_box_new_text ());
  g_signal_connect ((gpointer) schemes_combo, "changed",
                    G_CALLBACK (scheme_ed_combo_changed_cb),
                    NULL);
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (schemes_combo), TRUE, TRUE, 2);
    
  /* new scheme button */
    
  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  image = gtk_image_new_from_stock ("gtk-new", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_tooltips_set_tip (tooltips, button, 
                          _("Create a new scheme"),
                          _("Create a new scheme"));
  g_signal_connect ((gpointer) button, "clicked",
                      G_CALLBACK (scheme_ed_new_clicked_cb),
                      NULL);
    
  /* copy scheme button */
    
  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_tooltips_set_tip (tooltips, button, 
                          _("Create a copy of the selected scheme"),
                          _("Create a copy of the selected scheme"));
  image = gtk_image_new_from_stock ("gtk-copy", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);
  g_signal_connect ((gpointer) button, "clicked",
                      G_CALLBACK (scheme_ed_copy_clicked_cb),
                      schemes_combo);
    
  /* delete scheme button */
    
  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_tooltips_set_tip (tooltips, button, 
                          _("Delete the selected scheme"),
                          _("Delete the selected scheme"));
  image = gtk_image_new_from_stock ("gtk-delete", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);
  g_signal_connect ((gpointer) button, "clicked",
                     G_CALLBACK (scheme_ed_delete_clicked_cb),
                      schemes_combo);
    
  /* is default toggle button */
    
  checkbutton = gtk_check_button_new_with_label (_("Default"));
  gtk_box_pack_start (GTK_BOX (hbox), checkbutton, FALSE, FALSE, 0);
  gtk_tooltips_set_tip (tooltips, checkbutton, 
                          _("Toggle whether the selected scheme is the default"),
                          _("Toggle whether the selected scheme is the default"));
    
  g_signal_connect ((gpointer) checkbutton, "toggled",
                    G_CALLBACK (scheme_ed_default_button_toggled_cb),
                    schemes_combo);
  /* sync with selected scheme */
  g_signal_connect ((gpointer) schemes_combo, "changed",
                    G_CALLBACK (scheme_ed_update_default_button_cb),
                    checkbutton);
    
  
  /** color swatches and color selection **/
    
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (editor_vbox), hbox, TRUE, TRUE, 0);
    
  /* color swatches treeview */

  frame = gtk_frame_new (NULL);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 0);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, TRUE, 2);
    
  treeview = schemes_create_tree_view ();
  gtk_container_add (GTK_CONTAINER (frame), treeview);
  gtk_tooltips_set_tip (tooltips, treeview, 
                          _("Select an element to edit it"),
                          _("Select an element to edit it"));
  
  /* color selection */  
    
  color_picker = schemes_create_color_picker ();
  gtk_box_pack_start (GTK_BOX (hbox), color_picker, TRUE, TRUE, 2);
    
  /** global dialog close button box **/
    
  hbuttonbox = gtk_hbutton_box_new ();
  gtk_box_pack_start (GTK_BOX (scheme_editor), hbuttonbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbuttonbox), 3);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox), GTK_BUTTONBOX_END);
    
  /* close button */

  button = gtk_button_new_from_stock ("gtk-close");
  gtk_container_add (GTK_CONTAINER (hbuttonbox), button);
  gtk_container_set_border_width (GTK_CONTAINER (button), 1);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
    
  /** scheme save / revert buttons **/
    
  hbuttonbox = gtk_hbutton_box_new ();
  gtk_box_pack_start (GTK_BOX (editor_vbox), hbuttonbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbuttonbox), 4);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox), GTK_BUTTONBOX_END);
    
  /* revert scheme button */

  button = gtk_button_new ();
  label  = gtk_label_new (_("Revert"));
  image  = gtk_image_new_from_stock ("gtk-revert-to-saved", GTK_ICON_SIZE_MENU);
  hbox   = gtk_hbox_new (FALSE, 0);
  
  gtk_container_add (GTK_CONTAINER (button), hbox);
  gtk_box_pack_start (GTK_BOX (hbox), image, TRUE, TRUE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 2); 
  gtk_misc_set_alignment (GTK_MISC (image), 1, 0.5);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  g_signal_connect ((gpointer) button, "clicked",
                    G_CALLBACK (scheme_ed_revert_clicked_cb),
                    scheme_editor);
  gtk_container_add (GTK_CONTAINER (hbuttonbox), button);
  gtk_container_set_border_width (GTK_CONTAINER (button), 1);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  //gtk_widget_set_sensitive (button, FALSE);
    
  /* save scheme button */
    
  button = gtk_button_new ();
  label  = gtk_label_new (_("Save"));
  image  = gtk_image_new_from_stock ("gtk-save", GTK_ICON_SIZE_MENU);
  hbox   = gtk_hbox_new (FALSE, 0);

  gtk_container_add (GTK_CONTAINER (button), hbox);
  gtk_box_pack_start (GTK_BOX (hbox), image, TRUE, TRUE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 2); 
  gtk_misc_set_alignment (GTK_MISC (image), 1, 0.5);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  g_signal_connect ((gpointer) button, "clicked",
                    G_CALLBACK (scheme_ed_save_clicked_cb),
                    scheme_editor);
  gtk_container_add (GTK_CONTAINER (hbuttonbox), button);
  gtk_container_set_border_width (GTK_CONTAINER (button), 1);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  
  /** general tab option widgets **/
    
  /* scheme selection radios */
    
  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (general_vbox), frame, FALSE, TRUE, 0);
  label = gtk_label_new (_("Automatic scheme selection"));
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 8);

    
  sel_options_vbox = gtk_vbox_new (TRUE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (sel_options_vbox), 5);
  gtk_container_add (GTK_CONTAINER (frame), sel_options_vbox);
    
  radiobuttons[SCHEME_SELECT_DEFAULT] = gtk_radio_button_new_with_label (NULL, 
                                    _("Always use the default scheme"));
  gtk_box_pack_start_defaults (GTK_BOX (sel_options_vbox),
                               radiobuttons[SCHEME_SELECT_DEFAULT]);
    
  radiobuttons[SCHEME_SELECT_FILENAME] = 
      gtk_radio_button_new_with_label_from_widget (
                    GTK_RADIO_BUTTON (radiobuttons[SCHEME_SELECT_DEFAULT]),
                    _("Select scheme by filename"));
    
  gtk_box_pack_start_defaults (GTK_BOX (sel_options_vbox),
                               radiobuttons[SCHEME_SELECT_FILENAME]);

  radiobuttons[SCHEME_SELECT_RANDOM] = 
      gtk_radio_button_new_with_label_from_widget (
                    GTK_RADIO_BUTTON (radiobuttons[SCHEME_SELECT_DEFAULT]),
                    _("Select random scheme"));
    
  g_signal_connect ((gpointer) GTK_TOGGLE_BUTTON (radiobuttons[0]), "toggled",
                      G_CALLBACK (schemes_ed_radio_toggled_cb),
                      GINT_TO_POINTER (SCHEME_SELECT_DEFAULT));
  g_signal_connect ((gpointer) GTK_TOGGLE_BUTTON (radiobuttons[1]), "toggled",
                      G_CALLBACK (schemes_ed_radio_toggled_cb),
                      GINT_TO_POINTER (SCHEME_SELECT_FILENAME));
  g_signal_connect ((gpointer) GTK_TOGGLE_BUTTON (radiobuttons[2]), "toggled",
                      G_CALLBACK (schemes_ed_radio_toggled_cb),
                      GINT_TO_POINTER (SCHEME_SELECT_RANDOM));
    
  gtk_box_pack_start_defaults (GTK_BOX (sel_options_vbox), 
                               radiobuttons[SCHEME_SELECT_RANDOM]);
    
  method = schemes_get_selection_method ();
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobuttons[method]), TRUE);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  g_signal_connect ((gpointer) selection, "changed",
                    G_CALLBACK (schemes_ed_treeview_selection_changed_cb),
                    treeview);
    
  schemes_refresh_combo (index);

  return scheme_editor;
}

GtkWidget *
schemes_create_color_picker (void)
{
  GtkWidget * vbox1;
  GtkWidget * hbox1;
  GtkWidget * label1;
  GtkWidget * combobox1;
  GtkWidget * notebook1;
  GtkWidget * vbox2;
  GtkWidget * radiobutton1;
  GSList    * radiobutton1_group = NULL;
  GtkWidget * radiobutton2;
  GtkWidget * radiobutton3;
  GtkWidget * radiobutton4;
  GtkWidget * radiobutton6;
  GtkWidget * radiobutton5;
  GtkWidget * radiobutton7;
  GtkWidget * radiobutton8;
  GtkWidget * radiobutton9;
  GtkWidget * radiobutton10;
  GtkWidget * label8;
  GtkWidget * empty_notebook_page;
  GtkWidget * scrollwindow;
  GtkWidget * viewport;

  scrollwindow = gtk_scrolled_window_new (NULL, NULL);
  vbox1 = gtk_vbox_new (FALSE, 0);
    
  viewport = gtk_viewport_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (viewport), vbox1);
  gtk_container_add (GTK_CONTAINER (scrollwindow), viewport);
  //gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrollwindow), vbox1);
  //gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollwindow), 
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 2);

  label1 = gtk_label_new (_("Color source:"));
  gtk_box_pack_start (GTK_BOX (hbox1), label1, FALSE, FALSE, 0);
  gtk_misc_set_padding (GTK_MISC (label1), 6, 0);

  combobox1 = gtk_combo_box_new_text ();
  gtk_box_pack_start (GTK_BOX (hbox1), combobox1, FALSE, FALSE, 2);
  gtk_combo_box_append_text (GTK_COMBO_BOX (combobox1), _("Custom color"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combobox1), _("GtkStyle color"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combobox1), _("Disable this element"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (combobox1), 0);
  gtk_widget_set_sensitive (combobox1, FALSE);
  gtk_widget_set_size_request (combobox1, 280, -1);

  notebook1 = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (vbox1), notebook1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (notebook1), 5);
  GTK_WIDGET_UNSET_FLAGS (notebook1, GTK_CAN_FOCUS);
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook1), FALSE);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook1), FALSE);

  colorselection = gtk_color_selection_new ();
  gtk_color_selection_set_update_policy (GTK_COLOR_SELECTION (colorselection),
                                         GTK_UPDATE_DELAYED);
  gtk_color_selection_set_has_opacity_control (GTK_COLOR_SELECTION (colorselection), FALSE);
  gtk_container_add (GTK_CONTAINER (notebook1), colorselection);
  gtk_container_set_border_width (GTK_CONTAINER (colorselection), 2);
  g_object_set_data (G_OBJECT (scheme_editor), "color-selection", (gpointer) colorselection);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (notebook1), vbox2);
  gtk_container_set_border_width (GTK_CONTAINER (vbox2), 4);
  gtk_widget_set_sensitive (vbox2, FALSE);

  radiobutton1 = gtk_radio_button_new_with_mnemonic (NULL, _("Foreground"));
  gtk_box_pack_start (GTK_BOX (vbox2), radiobutton1, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton1), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton1));

  radiobutton2 = gtk_radio_button_new_with_mnemonic (NULL, _("Background"));
  gtk_box_pack_start (GTK_BOX (vbox2), radiobutton2, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton2), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton2));

  radiobutton3 = gtk_radio_button_new_with_mnemonic (NULL, _("Light"));
  gtk_box_pack_start (GTK_BOX (vbox2), radiobutton3, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton3), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton3));

  radiobutton4 = gtk_radio_button_new_with_mnemonic (NULL, _("Mid"));
  gtk_box_pack_start (GTK_BOX (vbox2), radiobutton4, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton4), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton4));

  radiobutton6 = gtk_radio_button_new_with_mnemonic (NULL, _("Dark"));
  gtk_box_pack_start (GTK_BOX (vbox2), radiobutton6, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton6), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton6));

  radiobutton5 = gtk_radio_button_new_with_mnemonic (NULL, _("Base"));
  gtk_box_pack_start (GTK_BOX (vbox2), radiobutton5, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton5), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton5));

  radiobutton7 = gtk_radio_button_new_with_mnemonic (NULL, _("Text"));
  gtk_box_pack_start (GTK_BOX (vbox2), radiobutton7, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton7), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton7));

  radiobutton8 = gtk_radio_button_new_with_mnemonic (NULL, _("Text AA"));
  gtk_box_pack_start (GTK_BOX (vbox2), radiobutton8, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton8), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton8));

  radiobutton9 = gtk_radio_button_new_with_mnemonic (NULL, _("Black"));
  gtk_box_pack_start (GTK_BOX (vbox2), radiobutton9, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton9), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton9));

  radiobutton10 = gtk_radio_button_new_with_mnemonic (NULL, _("White"));
  gtk_box_pack_start (GTK_BOX (vbox2), radiobutton10, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton10), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton10));

  label8 = gtk_label_new (_("DISABLED"));
  gtk_container_add (GTK_CONTAINER (notebook1), label8);
  gtk_widget_set_sensitive (label8, FALSE);
  gtk_label_set_use_markup (GTK_LABEL (label8), TRUE);

  empty_notebook_page = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (notebook1), empty_notebook_page);

  empty_notebook_page = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (notebook1), empty_notebook_page);

  empty_notebook_page = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (notebook1), empty_notebook_page);
    
  g_signal_connect ((gpointer) colorselection, "color-changed",
                      G_CALLBACK (schemes_ed_color_changed_cb),
                      NULL);

  gtk_widget_show_all (scrollwindow);
  return scrollwindow;
}
