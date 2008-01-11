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

#ifndef __SWEEP_APP_H__
#define __SWEEP_APP_H__

#include <gtk/gtk.h>
#include <pthread.h>

/* #include "i18n.h" */

typedef struct _sw_perform_data sw_perform_data;

struct _sw_perform_data {
  SweepFunction func;
  sw_param_set pset;
  gpointer custom_data;
};

/* Tools */
typedef enum {
  TOOL_NONE = 0,
  TOOL_SELECT,
  TOOL_MOVE,
  TOOL_ZOOM,
  TOOL_CROP,
  TOOL_SCRUB,
  TOOL_PENCIL,
  TOOL_NOISE,
  TOOL_HAND
} sw_tool_t;

/* View colors */
enum {
  VIEW_COLOR_BLACK,
  VIEW_COLOR_RED,
  VIEW_COLOR_ORANGE,
  VIEW_COLOR_YELLOW,
  VIEW_COLOR_BLUE,
  VIEW_COLOR_WHITE,
  VIEW_COLOR_RADAR,
  VIEW_COLOR_DEFAULT_MAX=VIEW_COLOR_RADAR,
  VIEW_COLOR_BLUESCREEN,
  VIEW_COLOR_MAX
};

typedef enum {
  SW_TOOLBAR_BUTTON,
  SW_TOOLBAR_TOGGLE_BUTTON,
  SW_TOOLBAR_RADIO_BUTTON,
} sw_toolbar_button_type;

typedef struct {
  sw_framecount_t start, end;
} sw_view_bounds;

typedef struct _sw_view sw_view;

struct _sw_view {
  sw_sample * sample;

  sw_framecount_t start, end; /* bounds of visible frames */
  sw_audio_t vlow, vhigh; /* bounds of vertical zoom */
  /*  gfloat gain;*/
  /*gfloat rate;*/

  sw_tool_t current_tool;

  gint repeater_tag;

  gboolean following; /* whether or not to follow playmarker */

  gint hand_offset;

  GtkWidget * window;
  GtkWidget * time_ruler;
  GtkWidget * scrollbar;
  GtkWidget * display;
  GtkWidget * pos;
  GtkWidget * status;
  GtkWidget * menubar;
  GtkWidget * menu;
  GtkWidget * zoom_combo;
  GtkWidget * progress;

  GtkWidget * follow_toggle;
  GtkWidget * play_pos_time;
  GtkWidget * play_pos_frame;
  GtkWidget * loop_toggle;
  GtkWidget * playrev_toggle;
  GtkWidget * play_toggle;
  GtkWidget * play_sel_toggle;
  GtkWidget * mute_toggle;
  GtkWidget * monitor_toggle;

  GtkWidget * menu_sel;
  GtkWidget * menu_point;

  GtkWidget * channelops_menuitem;
  GtkWidget * channelops_submenu;
  GList * channelops_widgets;

  GtkWidget * follow_checkmenu;
  GtkWidget * color_menuitems[VIEW_COLOR_MAX];
  GtkWidget * loop_checkmenu;
  GtkWidget * playrev_checkmenu;
  GtkWidget * mute_checkmenu;
  GtkWidget * monitor_checkmenu;

  GtkObject * adj;
  GtkObject * gain_adj;
  GtkObject * rate_adj;

  GtkWidget * db_rulers_vbox;
  GList * db_rulers;

  GList * tool_buttons;

  GList * noready_widgets; /* Widgets to set insensitive on READY only */
  GList * nomodify_widgets; /* Widgets to set insensitive on MODIFY or ALLOC */
  GList * noalloc_widgets;  /* Widgets to set insensitive on ALLOC only */
};

typedef struct _sw_head sw_head;
typedef struct _sw_head_controller sw_head_controller;

typedef enum {
  SWEEP_HEAD_PLAY,
  SWEEP_HEAD_RECORD
} sw_head_t;

struct _sw_head_controller {
  sw_head * head;

  GtkWidget * follow_toggle;
  GtkWidget * pos_label;
  GtkWidget * loop_toggle;
  GtkWidget * reverse_toggle;
  GtkWidget * go_toggle;
  GtkWidget * go_sel_toggle;
  GtkWidget * mute_toggle;
  GtkObject * gain_adj;
};

struct _sw_head {
  sw_sample * sample;

  GMutex * head_mutex;
  sw_head_t type; /* SWEEP_HEAD_PLAY or SWEEP_HEAD_RECORD */
  /*  sw_transport_type transport_mode;*/
  sw_framecount_t stop_offset;
  gdouble offset;
  sw_framecount_t realoffset; /* offset according to device */
  gboolean going; /* stopped or going? */
  gboolean restricted; /* restricted to sample->sounddata->sels ? */
  gboolean looping;
  gboolean previewing;
  gboolean reverse;
  gboolean mute;
  gboolean monitor;
  gfloat delta; /* current motion delta */
  gfloat gain;
  gfloat rate;
  gfloat mix; /* record mixing level */
  
  sw_head * scrub_master; /* another head this is being scrubbed by */
  gboolean scrubbing; /* if this head is a scrub master, is it scrubbing ? */

  gint repeater_tag;
  GList * controllers;
};

typedef enum {
  SWEEP_FILE_METHOD_BY_EXTENSION=0, /* Guess */
  SWEEP_FILE_METHOD_LIBSNDFILE,
  SWEEP_FILE_METHOD_OGGVORBIS,
  SWEEP_FILE_METHOD_SPEEX,
  SWEEP_FILE_METHOD_MP3=1000666 /* Random high number -- unsupported */
} sw_file_method_t;

/*
 * sw_sample
 */
struct _sw_sample {
  sw_sounddata * sounddata;
  GList * views;

  sw_view_bounds stored_views[10];

  gchar * pathname;

  time_t last_mtime;
  gboolean edit_ignore_mtime;

  sw_file_method_t file_method; /* file handler; eg. libsndfile, libvorbis */
  int file_format; /* format within handler, eg. WAV */
  gpointer file_info; /* parameters of original file or last save */

  sw_sel * tmp_sel; /* Temporary selection, used while selecting */

  /* Operations; lock on scheduling */

  pthread_t ops_thread;

  GMutex * ops_mutex;
  GList * registered_ops;
  GList * current_undo;
  GList * current_redo;
  sw_op_instance * active_op;
  gint op_progress_tag;

  /* Per-edit locking */

  GMutex * edit_mutex; /* Mutex for access to edit state */
  sw_edit_mode edit_mode; /* READY/MODIFYING/ALLOC */
  sw_edit_state edit_state; /* IDLE/BUSY/DONE/CANCEL */
  gboolean modified; /* modified since last save ? */

  GCond * pending_cond; /* held with edit_mutex */
  GList * pending_ops;

  /* Playback, recording, scrubbing */

  sw_framecount_t user_offset; /* XXX: Actual offset of driver (frames) */
  gboolean by_user; /* Offset was last changed by user */

  GMutex * play_mutex; /* Mutex for access to play state */
  sw_head * play_head;

  sw_head * rec_head;

  gfloat rate; /* XXX: */

  gint color;

  GtkWidget * info_clist;

  gint progress_percent; /* completion percentage of current op */

  gint playmarker_tag; /* gtk_timeout tag for playmarkers */

  gboolean tmp_message_active;
  gchar * last_tmp_message;
  gint tmp_message_tag;
};

void
sweep_quit (void);

guint
sweep_timeout_add (guint32 interval, GtkFunction function, gpointer data);

void
sweep_timeout_remove (guint sweep_timeout_handler_id);

#endif /* __SWEEP_APP_H__ */
