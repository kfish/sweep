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

#include <glib.h>
#include <gtk/gtk.h>
#include <sweep/sweep_i18n.h>
#include "sweep-scheme.h"
#include "schemes.h"
#include "callbacks.h"

enum {
  DESTROY,
  CHANGED,
  LAST_SIGNAL
};



enum {
  PROP_0,
  PROP_ELEMENT_COLOR
};

gchar * element_names[SCHEME_ELEMENT_LAST] = {
  _("Wave foreground"),
  _("Wave background"),
  _("Scroll trough foreground"),
  _("Scroll trough background"),
  _("Playback cursor"),
  _("Record cursor"),
  _("Selection outline"),
  _("Selection"),
  _("Min/Max"),
  _("Zero crossing"),
  _("Wave highlight"),
  _("Wave lowlight"),
}; 

gchar * element_keys[SCHEME_ELEMENT_LAST] = {
  "wave-fg",
  "wave-bg",
  "scroll-trough-fg",
  "scroll-trough-bg",
  "playback-cursor",
  "record-cursor",
  "selection-outline",
  "selection",
  "min-max",
  "zero",
  "wave-highlight",
  "wave-shadow",
}; 



G_DEFINE_TYPE (SweepScheme, sweep_scheme, G_TYPE_OBJECT)

static guint object_signals[LAST_SIGNAL] = { 0 };

static void
sweep_scheme_dispose (GObject * object) 
{
  g_object_unref ((gpointer) SWEEP_SCHEME (object)->preview_icon);
    
  G_OBJECT_CLASS (sweep_scheme_parent_class)->dispose (object);
}

static void
sweep_scheme_finalize (GObject * object) 
{
  gint element;
  SweepScheme * scheme = SWEEP_SCHEME (object);

  g_free (scheme->name);
  
  for (element = 0; element < SCHEME_ELEMENT_LAST; element++)
  {      
    g_free (scheme->colors[element]);
  }
  G_OBJECT_CLASS (sweep_scheme_parent_class)->finalize (object);      
}
static void
sweep_scheme_init (SweepScheme * scheme)
{
  gint element;
  GdkVisual *visual;

  visual = gdk_screen_get_system_visual (gdk_screen_get_default ());
  scheme->modified = FALSE;
  scheme->is_default = FALSE;
  scheme->preview_icon = gdk_pixmap_new (NULL, 16, 16, visual->depth);

  for (element = 0; element < SCHEME_ELEMENT_LAST; element++)
  {
    scheme->colors[element]  = g_new0 (GdkColor, 1);
    scheme->enabled[element] = TRUE;
    scheme->styles[element]  = SCHEME_GTK_STYLE_NONE;
  }
}

static void
sweep_scheme_class_init (SweepSchemeClass * klass) 
{

  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose  = sweep_scheme_dispose;
  gobject_class->finalize = sweep_scheme_finalize;

  object_signals[DESTROY] =
    g_signal_new ("destroy",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		  G_STRUCT_OFFSET (SweepSchemeClass, destroy),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
    
  object_signals[CHANGED] =
    g_signal_new ("changed",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		  G_STRUCT_OFFSET (SweepSchemeClass, changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /* this may not be necessary */
  object_signals[CHANGED] =
    g_signal_new ("preview-changed",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		  G_STRUCT_OFFSET (SweepSchemeClass, changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
    
  klass->changed = NULL;
  klass->preview_changed = NULL;
    
}

SweepScheme *
sweep_scheme_new (void) 
{
    return g_object_new (SWEEP_TYPE_SCHEME, NULL);
}

static void
update_scheme_preview (SweepScheme * scheme)
{
  GdkDrawable * preview_icon = GDK_DRAWABLE (scheme->preview_icon);
  GdkGC       * gc = gdk_gc_new (preview_icon);
    
  gdk_gc_set_rgb_fg_color (gc, scheme->colors[SCHEME_ELEMENT_BG]);
  gdk_draw_rectangle (preview_icon, 
                      gc,
                      TRUE, 0, 0, 16, 16);
    
  gdk_gc_set_rgb_fg_color (gc, scheme->colors[SCHEME_ELEMENT_FG]);
  gdk_draw_rectangle (preview_icon, 
                      gc,
                      TRUE, 4, 4, 8, 8);
   
  g_signal_emit_by_name ((gpointer) scheme, "preview-changed");
    
  g_object_unref ((gpointer) gc);
}

SweepScheme *
sweep_scheme_copy (SweepScheme *scheme)
{
  
  gint element;
  SweepScheme * scheme_copy;
    
  if (!SWEEP_IS_SCHEME (scheme))
    return NULL;
  else {
    scheme_copy = sweep_scheme_new ();
    
    scheme_copy->name       = g_strdup (scheme->name);
    scheme_copy->modified   = FALSE; //scheme->modified; 
    scheme_copy->is_default = FALSE;
      
    for (element = 0; element < SCHEME_ELEMENT_LAST; element++)
    {
        scheme_copy->colors[element]->red   = scheme->colors[element]->red;
        scheme_copy->colors[element]->green = scheme->colors[element]->green;
        scheme_copy->colors[element]->blue  = scheme->colors[element]->blue;
        scheme_copy->enabled[element]       = scheme->enabled[element];
        scheme_copy->styles[element]        = scheme->styles[element];
    }
    update_scheme_preview (scheme_copy);

  }
  return scheme_copy;

}

void
sweep_scheme_set_element_color (SweepScheme * scheme,
                                gint element,
                                GdkColor * color)
{
  GdkColor * old_color;
    
  g_return_if_fail (scheme != NULL);
  g_return_if_fail (color != NULL);
  g_return_if_fail ((element >= 0) || (element < SCHEME_ELEMENT_LAST));
      
  old_color = scheme->colors[element];
  scheme->colors[element] = copy_gdk_colour (color);
  g_free (old_color);
          
  g_signal_emit_by_name ((gpointer) scheme, "changed");
      
  if ((element == SCHEME_ELEMENT_FG) || (element == SCHEME_ELEMENT_BG))
    update_scheme_preview (scheme);
}

void
sweep_scheme_set_element_enabled (SweepScheme * scheme,
                                  gint element,
                                  gboolean is_enabled)
{
  g_return_if_fail (scheme != NULL);
  g_return_if_fail ((element >= 0) || (element < SCHEME_ELEMENT_LAST));

    if (scheme->enabled[element] == is_enabled) /* no change */
      return;
  
    scheme->enabled[element] = is_enabled;
            
    g_signal_emit_by_name ((gpointer) scheme, "changed");
}

void
sweep_scheme_set_element_style (SweepScheme * scheme,
                                gint element,
                                gint style)
{
  g_return_if_fail (scheme != NULL);
  g_return_if_fail ((element >= 0) || (element < SCHEME_ELEMENT_LAST));
  g_return_if_fail ((element >= 0) || (element < SCHEME_GTK_STYLE_LAST));
        
  if (scheme->styles[element] == style) /* no change */
    return;
  
  scheme->styles[element] = style;
            
  g_signal_emit_by_name ((gpointer) scheme, "changed");
      
  if ((element == SCHEME_ELEMENT_FG) || (element == SCHEME_ELEMENT_BG))
    update_scheme_preview (scheme);
}



