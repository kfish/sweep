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

#ifndef __SWEEP_H__
#define __SWEEP_H__

#include <gtk/gtk.h>

#define SWEEP_CURRENT_MAJOR      0

#define SWEEP_VERSION_CODE(major, minor, build)  \
  (  (((int) (major) &   0xff) << 24)     \
   | (((int) (minor) &   0xff) << 16)     \
   | (((int) (build) & 0xffff) <<  0))

#define SWEEP_VERSION_MAJOR(code)        ((((int)(code)) >> 24) &   0xff)
#define SWEEP_VERSION_MINOR(code)        ((((int)(code)) >> 16) &   0xff)
#define SWEEP_VERSION_BUILD(code)        ((((int)(code)) >>  0) & 0xffff)


/*
 * Standard gettext macros.
 */
#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

/* Tools */
enum {
  TOOL_NONE = 0,
  TOOL_SELECT,
  TOOL_MOVE,
  TOOL_ZOOM,
  TOOL_CROP
};

/* Sweep internal audio representation
 *
 * gfloats in the range [-1.0, 1.0]
 */
typedef gfloat sw_audio_t;

/* Intermediate audio representation format:
 * Use this for intermediate values for mixing etc.
 */
typedef gdouble sw_audio_intermediate_t;

#define SW_AUDIO_T_MAX (1.0)
#define SW_AUDIO_T_MIN (-1.0)

/* Sweep datatypes */

typedef struct _sw_sel sw_sel;
typedef struct _sw_format sw_format;
typedef struct _sw_sample sw_sample;
typedef struct _sw_sdata sw_sdata;
typedef struct _sw_view sw_view;

/*
 * sw_sample
 */
struct _sw_sample {
  sw_sdata * sdata;
  GList * views;

  GList * registered_ops;
  GList * current_undo;
  GList * current_redo;
};

/*
 * sw_sel: a region in a selection.
 *
 * Caution: Potential off-by-one error.
 *    The selected region is defined as going from
 *    sel_start to (sel_end - 1).
 *    Thus the length of the selection is always
 *    (sel_end - sel_start).
 *
 * Units are frame offsets from start of sample.
 *
 */
struct _sw_sel {
  glong sel_start;
  glong sel_end;
};

/*
 * sw_format: a sampling format.
 *
 * NB. Assumes 16 bit Signed samples.
 * Stereo is stored LR.
 */
struct _sw_format {
  gint f_channels;  /* nr channels per frame */
  gint f_rate;      /* sampling rate (Hz) */
};

#define SW_DIR_LEN 256

struct _sw_sdata {
  sw_format * format;
  glong s_length;    /* nr frames */

  gpointer data;

  GList * sels;     /* selection: list of sw_sels */
  sw_sel * tmp_sel; /* Temporary selection */
  GMutex * sels_mutex; /* Mutex for access to sels */

  gint playmarker_tag; /* gtk_timeout tag for playmarkers */

  gchar * filename;
  gchar directory[SW_DIR_LEN];
};

struct _sw_view {
  sw_sample * sample;

  glong v_start, v_end; /* bounds of visible frames */
  gfloat v_vol;

  GtkWidget * v_window;
  GtkWidget * v_hruler;
  GtkWidget * v_scrollbar;
  GtkWidget * v_display;
  GtkWidget * v_pos;
  GtkWidget * v_status;
  GtkWidget * v_menubar;
  GtkWidget * v_menu;
  GtkObject * v_adj;
  GtkObject * v_vol_adj;
};

typedef struct _edit_region edit_region;
typedef struct _edit_buffer edit_buffer;

/*
 * A region of data. Units are frames.
 * The length of data available *data is (end - start)
 */
struct _edit_region {
  glong start;
  glong end;

  gpointer data;
};

struct _edit_buffer {
  sw_format * format;
  GList * regions;
};

typedef void (*SweepModify) (sw_sample * sample);

typedef void (*SweepFunction) (gpointer data);
typedef void (*SweepCallback) (sw_sample * sample, gpointer data);

typedef struct _sw_operation sw_operation;
typedef struct _sw_op_instance sw_op_instance;

struct _sw_operation {
  SweepCallback undo;
  SweepFunction purge_undo;
  SweepCallback redo;
  SweepFunction purge_redo;
};

struct _sw_op_instance {
  char * description;
  sw_operation * op;
  gpointer undo_data;
  gpointer redo_data;
};

/*
 * Basic types for parameters
 */
typedef enum {
  SWEEP_TYPE_BOOL = 0,
  SWEEP_TYPE_INT,
  SWEEP_TYPE_FLOAT,
  SWEEP_TYPE_STRING,
} sw_param_type;

typedef enum {
  SWEEP_UNIT_NONE = 0,
  SWEEP_UNIT_FRAMES,
  SWEEP_UNIT_SECONDS,
  SWEEP_UNIT_HERTZ,
  SWEEP_UNIT_RADIANS,
  SWEEP_UNIT_DECIBELS,
} sw_unit;

typedef gboolean sw_bool;
typedef gint sw_int;
typedef gdouble sw_float;
typedef gchar * sw_string;


/*
 * Instances of Parameter Sets
 */
typedef union _sw_param sw_param;
typedef sw_param * sw_param_set;

union _sw_param {
  sw_bool b;
  sw_int  i;
  sw_float f;
  sw_string s;
};

/*
 * Specifications for Parameter Sets
 *
 */

/*
 * Constraint types. These are used within the param_spec to indicate
 * the usage of the sw_constraint.
 */
typedef enum {
/*
 * SW_PARAM_CONSTRAINED_NOT indicates that the parameter is completely
 * unconstrained.
 *
 * Useful in verse; with iambic pentameter, accent the "-ED" eg.
 *
 *     This 'x' is SW_CONSTRAINED_NOT!
 *     How free is its life, how wretched its lot!
 */
  SW_PARAM_CONSTRAINED_NOT=0,

/*
 * SW_PARAM_CONSTRAINED_LIST indicates that the parameter is constrained
 * to values given in a param list.
 */
  SW_PARAM_CONSTRAINED_LIST,

/*
 * SW_PARAM_CONSTRAINED_RANGE indicates that the parameter is constrained
 * to a given range.
 */
  SW_PARAM_CONSTRAINED_RANGE,

} sw_constraint_type;


/* VALID PORTIONS OF RANGES */

/*
 * SW_RANGE_LOWER_BOUND_VALID indicates that the 'lower' field
 * of the constraint, if interpreted as a range, is valid. If this bit
 * is not set, then the parameter is known to have no lower bound.
 * If the constraint is valid and the 'step' field is set, then the
 * value of 'lower' is used to determine a basis for the parameter,
 * even if SW_RANGE_LOWER_BOUND_VALID is not set.
 *
 * Note that the constraint is not interpreted as a range if
 * SW_PARAM_CONSTRAINED_LIST is set.
 */
#define SW_RANGE_LOWER_BOUND_VALID (1<<0)

/*
 * SW_RANGE_UPPER_BOUND_VALID indicates that the 'upper' field
 * of the constraint, if interpreted as a range, is valid. If this bit
 * is not set, then the parameter is known to have no upper bound.
 */
#define SW_RANGE_UPPER_BOUND_VALID (1<<1)

/*
 * SW_RANGE_STEP_VALID indicates that the 'step' field of
 * the constraint, if interpreted as a range, is valid. If this bit
 * is not set, then the parameter is assumed to be continuous.
 * If this field is valid, then the parameter will only have values
 * equal to  (lower + n*step) for integer n.
 *
 * This constraint is ignored for string paramters.
 */
#define SW_RANGE_STEP_VALID (1<<2)
 

/*
 * HINTS for user interface semantics
 */

typedef int sw_hints;

/*
 * SW_PARAM_HINT_LOGARITHMIC indicates that the parameter should be
 * interpreted as logarithmic.
 */
#define SW_PARAM_HINT_LOGARITHMIC  (1<<1)

/*
 * SW_PARAM_HINT_FILENAME indicates that the parameter should be
 * interpreted as a valid filename on the user's system.
 */
#define SW_PARAM_HINT_FILENAME     (1<<2)
 

typedef struct _sw_param_spec sw_param_spec;
typedef struct _sw_param_range sw_param_range;
typedef union _sw_constraint sw_constraint;

/*
 * sw_param_range: a range of acceptable parameter values.
 *
 * NB: this is a hard limit. Values <lower and values >upper
 * need not be expected by plugins.
 *
 * The first parameter is a mask consisting of a bitwise or of between
 * zero and three of SW_RANGE_LOWER_BOUND_VALID,
 * SW_RANGE_UPPER_BOUND_VALID, and SW_RANGE_STEP_VALID.
 *
 * The three following parameters are interpreted as the type of the
 * parameter they constrain. The 'step' parameter is never valid for
 * string parameters.
 */
struct _sw_param_range {
  int valid_mask;
  sw_param lower;
  sw_param upper;
  sw_param step;
};

/*
 * sw_constraint
 *
 * Constraints on parameters. Constraints, if valid, are hard limits.
 *
 * All constraints are disregarded for boolean parameters.
 */
union _sw_constraint {
  /*
   * param_list: Values are constrained to those within a list of
   * parameters. NB: the length of this list is given by the
   * value of the first parameter, interpreted as an integer.
   * ie. this length = constraint->param_list[0].i
   */
  sw_param * list;

  /*
   * param_range, as described above.
   */
  sw_param_range * range;  /* param range */
};


/*
 * sw_param_spec: specification for a parameter.
 */
struct _sw_param_spec {
  /* A short name for this parameter */
  gchar * name;

  /* A longer description of the parameter's purpose and usage */
  gchar * desc;

  /* The type of the parameter */
  sw_param_type type;

  /* Constraints */
  sw_constraint_type constraint_type;
  sw_constraint constraint;

  /* Hints */
  sw_hints hints;
};


/*
 * Plugins and procedures
 */

typedef struct _sw_proc sw_proc;
typedef struct _sw_plugin sw_plugin;

struct _sw_proc {
  gchar * proc_name;
  gchar * proc_desc;
  gchar * proc_author;
  gchar * proc_copyright;
  gchar * proc_url;

  gchar * category;
  guint accel_key;
  GdkModifierType accel_mods;

  gint nr_params;
  sw_param_spec * param_specs;

  /* proc_suggest sets suggested values for the members of pset,
   * using sample if necessary.
   *
   * It is assumed that proc_suggest() knows nr_params because
   * it is part of the whole proc's code.
   *
   * If nr_params is 0 this function will not be called.
   * If this function is NULL default (zero) values will be used.
   */
  void (*proc_suggest) (sw_sample * sample, sw_param_set pset,
			gpointer custom_data);

  /* This is the function that actually does the work!
   *
   * proc_apply applies the parameter set pset to a sample
   *
   * It is assumed that this "apply" function knows nr_params,
   * because it is part of this whole proc's code.
   *
   * If nr_params is 0 this function will be passed a NULL pset.
   */
  sw_op_instance * (*proc_apply) (sw_sample * sample,
				  sw_param_set pset, gpointer custom_data);

  /* custom data to pass to the suggest and apply functions */
  gpointer proc_data;
};

struct _sw_plugin {
  /* plugin_init () returns a list of procs */
  GList * (*plugin_init) (void);

  /* plugin_cleanup() frees the plugin's private data structures */
  void (*plugin_cleanup) (void);
};

#endif  /* __SWEEP_H__ */






