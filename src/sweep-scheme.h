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

#include <gtk/gtk.h>

#ifndef __SWEEP_SCHEME_H__
#define __SWEEP_SCHEME_H__

G_BEGIN_DECLS

#define	SWEEP_TYPE_SCHEME			          (sweep_scheme_get_type ())

#define SWEEP_SCHEME(object)		        (G_TYPE_CHECK_INSTANCE_CAST ((object), \
                                          SWEEP_TYPE_SCHEME, SweepScheme))

#define SWEEP_SCHEME_CLASS(klass)		    (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                          SWEEP_TYPE_SCHEME, SweepScheme))Class))

#define SWEEP_IS_SCHEME(object)		      (G_TYPE_CHECK_INSTANCE_TYPE ((object), \
                                          SWEEP_TYPE_SCHEME))

#define SWEEP_IS_SCHEME_CLASS(klass)	  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                          SWEEP_TYPE_SCHEME))

#define	SWEEP_SCHEME_GET_CLASS(object)	(G_TYPE_INSTANCE_GET_CLASS ((object), \
                                          SWEEP_TYPE_SCHEME, SweepSchemeClass))

enum {
  SCHEME_ELEMENT_FG,
  SCHEME_ELEMENT_BG,
  SCHEME_ELEMENT_TROUGH_FG,
  SCHEME_ELEMENT_TROUGH_BG,
  SCHEME_ELEMENT_USER,
  SCHEME_ELEMENT_REC,
  SCHEME_ELEMENT_SELECTION_OUTLINE,
  SCHEME_ELEMENT_SELECTION,
  SCHEME_ELEMENT_MINMAX,
  SCHEME_ELEMENT_ZEROLINE,
  SCHEME_ELEMENT_HIGHLIGHT,
  SCHEME_ELEMENT_LOWLIGHT,
  SCHEME_ELEMENT_LAST
};

enum {
  SCHEME_GTK_STYLE_NONE,
  SCHEME_GTK_STYLE_FG,
  SCHEME_GTK_STYLE_BG,
  SCHEME_GTK_STYLE_LIGHT,
  SCHEME_GTK_STYLE_MID,
  SCHEME_GTK_STYLE_DARK,
  SCHEME_GTK_STYLE_BASE,
  SCHEME_GTK_STYLE_TEXT,
  SCHEME_GTK_STYLE_TEXT_AA,
  SCHEME_GTK_STYLE_BLACK,
  SCHEME_GTK_STYLE_WHITE,
  SCHEME_GTK_STYLE_LAST
};
 
typedef struct _SweepScheme SweepScheme;
typedef struct _SweepSchemeClass SweepSchemeClass;


/* FIXME: create struct for per element options? */ 
struct _SweepScheme {
  
  GObject     parent;
  GdkColor  * colors[SCHEME_ELEMENT_LAST];
  gboolean    enabled[SCHEME_ELEMENT_LAST]; /* always true for some elements */
  gint        styles[SCHEME_ELEMENT_LAST]; /* overrides custom colors if > 0 */
  gchar     * name;
  gboolean    modified;
  gboolean    is_default;
  GdkPixmap * preview_icon;
};



struct _SweepSchemeClass {
  GObjectClass parent_class;
    
  void (* destroy) (SweepScheme * object);
  void (* changed) (SweepScheme * object);
  void (* preview_changed) (SweepScheme * object);
};

SweepScheme *
sweep_scheme_new (void);

SweepScheme *
sweep_scheme_copy (SweepScheme *scheme);

GType
sweep_scheme_get_type (void);

void
sweep_scheme_set_element_color (SweepScheme * scheme,
                                gint element,
                                GdkColor * color);
void
sweep_scheme_set_element_enabled (SweepScheme * scheme,
                                  gint element,
                                  gboolean is_enabled);
void
sweep_scheme_set_element_style (SweepScheme * scheme,
                                gint element,
                                gint style);

G_END_DECLS

#endif /* __SWEEP_SCHEME_H__ */
