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

#ifndef __I18N_H__
#define __I18N_H__

/*
 * Standard gettext macros.
 */
#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)

#  define gettext_noop(String) String
#  define N_(String) gettext_noop(String)

/*
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) ("NOT TRANSLATED")
#  endif
*/

#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)

#ifdef DEVEL_CODE
#  define _(String) ("UNTRANSLATED")
#  define N_(String) ("UNTRANSLATED")
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#endif

#endif /* __I18N_H__ */
