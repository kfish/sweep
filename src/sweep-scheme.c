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
sweep_scheme_finalize (GObject *object) 
{
    
  gint element;
  SweepScheme *scheme = SWEEP_SCHEME (object);
    
  g_free (scheme->name);
  
  for (element = 0; element < SCHEME_ELEMENT_LAST; element++)
  {      
    g_free (scheme->scheme_colors[element]);
  }
  g_free (scheme);
      
}
static void
sweep_scheme_init (SweepScheme *scheme)
{
  
  gint element;

  scheme->modified = FALSE;
  scheme->preview_icon = NULL;

  for (element = 0; element < SCHEME_ELEMENT_LAST; element++)
  {
    scheme->scheme_colors[element] = g_new0 (GdkColor, 1);
    scheme->element_enabled[element] = TRUE; 
  }
}

static void
sweep_scheme_class_init (SweepSchemeClass *klass) 
{

  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  //gobject_class->dispose = gtk_object_dispose;
  gobject_class->finalize = sweep_scheme_finalize;

  //klass->destroy = gtk_object_real_destroy;

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
    
  klass->changed = NULL;
    
}

SweepScheme *
sweep_scheme_new (void) 
{
    
    return g_object_new (SWEEP_TYPE_SCHEME, NULL);
    
}

SweepScheme *
sweep_scheme_copy (SweepScheme *scheme)
{
  
  gint element;
  SweepScheme *scheme_copy;
    
  if (!SWEEP_IS_SCHEME (scheme))
    return NULL;
  else {
    scheme_copy = sweep_scheme_new ();
    
    scheme_copy->name     = g_strdup (scheme->name);
    scheme_copy->modified = scheme->modified;
      
    for (element = 0; element < SCHEME_ELEMENT_LAST; element++)
    {
        scheme_copy->scheme_colors[element]->red   = scheme->scheme_colors[element]->red;
        scheme_copy->scheme_colors[element]->green = scheme->scheme_colors[element]->green;
        scheme_copy->scheme_colors[element]->blue  = scheme->scheme_colors[element]->blue;
        scheme_copy->element_enabled[element]      = scheme->element_enabled[element]; 
    }

  }
  return scheme_copy;

}

void
sweep_scheme_set_element_color (SweepScheme *scheme,
                                gint element,
                                GdkColor *color)
{
    GdkColor *old_color;
    
    if ((scheme != NULL) ||
        (color != NULL)) {
        
        if ((element < 0) || (element >= SCHEME_ELEMENT_LAST))
          return;
        
        old_color = scheme->scheme_colors[element];
        scheme->scheme_colors[element] = copy_gdk_colour (color);
        g_free (old_color);
            
        g_signal_emit_by_name ((gpointer) scheme, "changed");
        
    }
    
}

void
sweep_scheme_set_element_enabled (SweepScheme *scheme,
                                  gint element,
                                  gboolean is_enabled)
{
  if (scheme != NULL) {
        
    if ((element < 0) || (element >= SCHEME_ELEMENT_LAST))
      return;
    if (scheme->element_enabled[element] == is_enabled) /* no change */
      return;
  
    scheme->element_enabled[element] = is_enabled;
            
    g_signal_emit_by_name ((gpointer) scheme, "changed");
        
  } 
}

void
sweep_scheme_set_element_style (SweepScheme *scheme,
                                gint element,
                                gint style)
{
  if (scheme != NULL) {
        
    if ((element < 0) || (element >= SCHEME_ELEMENT_LAST))
      return;
    if ((style < 0) || (style >= SCHEME_GTK_STYLE_LAST))
      return;
    if (scheme->element_style[element] == style) /* no change */
      return;
  
    scheme->element_style[element] = style;
            
    g_signal_emit_by_name ((gpointer) scheme, "changed");
        
  } 
    
}
