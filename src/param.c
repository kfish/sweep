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

#include <stdio.h>
#include "sweep_types.h"
#include "sweep_app.h"

static gint
param_cmp (sw_param_type type, sw_param p1, sw_param p2)
{
  switch (type) {
  case SWEEP_TYPE_BOOL:
    if (p1.b == p2.b) return 0;
    if (p2.b) return -1;
    if (p1.b) return 1;
    break;
  case SWEEP_TYPE_INT:
    if (p1.i == p2.i) return 0;
    if (p1.i < p2.i) return -1;
    if (p1.i > p2.i) return 1;
    break;
  case SWEEP_TYPE_FLOAT:
    if (p1.f == p2.f) return 0;
    if (p1.f < p2.f) return -1;
    if (p1.f > p2.f) return 1;
    break;
  case SWEEP_TYPE_STRING:
    return strcmp (p1.s, p2.s);
    break;
  default:
    return -1;
    break;
  }

  return -1;
}

/*
 * param_copy (p1, p2)
 *
 * src: p1
 * dest: p2
 */
static void
param_copy (sw_param * p1, sw_param * p2)
{
  if (p1 == p2) return;
  memcpy (p2, p1, sizeof (sw_param));
}

sw_param *
sw_param_set_new (sw_proc * proc)
{
  sw_param * p;

  p = g_malloc0 (sizeof(sw_param) * proc->nr_params);

  return p;
}

static void
print_type (sw_param_type type)
{
  switch (type) {
  case SWEEP_TYPE_BOOL:
    printf ("SWEEP_TYPE_BOOL");
    break;
  case SWEEP_TYPE_INT:
    printf ("SWEEP_TYPE_INT");
    break;
  case SWEEP_TYPE_FLOAT:
    printf ("SWEEP_TYPE_FLOAT");
    break;
  case SWEEP_TYPE_STRING:
    printf ("SWEEP_TYPE_STRING");
    break;
  default:
    break;
  }
}

static void
snprint_param (gchar * s, gint n, sw_param_type type, sw_param p)
{
  switch (type) {
  case SWEEP_TYPE_BOOL:
    if (p.b) snprintf (s, n, "TRUE");
    else snprintf (s, n, "FALSE");
    break;
  case SWEEP_TYPE_INT:
    snprintf (s, n, "%d", p.i);
    break;
  case SWEEP_TYPE_FLOAT:
    snprintf (s, n, "%f", p.f);
    break;
  case SWEEP_TYPE_STRING:
    snprintf (s, n, "%s", p.s);
    break;
  default:
    break;
  }
}

static void
print_param (sw_param_type type, sw_param p)
{
#define BUF_LEN 64
  gchar buf[BUF_LEN];

  snprint_param (buf, BUF_LEN, type, p);
  printf ("%s", buf);
#undef BUF_LEN
}

void
print_param_set (sw_proc * proc, sw_param_set pset)
{
  int i, j;
  sw_param_spec * ps;
  int valid_mask;

  printf ("\"%s\" has %d params.\n\n", proc->name, proc->nr_params);

  for (i=0; i < proc->nr_params; i++) {
    ps = &proc->param_specs[i];
    printf ("%d\t\"%s\" (\"%s\")\n", i, ps->name, ps->desc);
    printf ("\tType: ");
    print_type (ps->type);
    printf ("\n");
    if (ps->constraint_type == SW_PARAM_CONSTRAINED_NOT) {
      printf ("\tUnconstrained.\n");
    } else if (ps->constraint_type == SW_PARAM_CONSTRAINED_LIST) {
      printf ("\tConstrained to ");
      print_param (SWEEP_TYPE_INT, ps->constraint.list[0]);
      printf (" list values.");
      for (j=1; j <= ps->constraint.list[0].i; j++) {
	printf ("\n\t");
	print_param (ps->type, ps->constraint.list[j]);
      }
      printf ("\n");
    } else if (ps->constraint_type == SW_PARAM_CONSTRAINED_RANGE) {
      valid_mask = ps->constraint.range->valid_mask;

      if (valid_mask & SW_RANGE_LOWER_BOUND_VALID) {
	printf ("\tBounded below by ");
	print_param (ps->type, ps->constraint.range->lower);
	printf ("\n");
      }
      if (valid_mask & SW_RANGE_UPPER_BOUND_VALID) {
	printf ("\tBounded above by ");
	print_param (ps->type, ps->constraint.range->upper);
	printf ("\n");
      }
      if (valid_mask & SW_RANGE_STEP_VALID) {
	printf ("\tValues quantised by ");
	print_param (ps->type, ps->constraint.range->step);
	printf ("\n");
      }
    }
    printf ("\tCURRENT VALUE ");
    print_param (ps->type, pset[i]);
    printf ("\n");
  }
}

typedef enum {
  SW_PS_TOGGLE_BUTTON,
  SW_PS_KNOWN_PARAM,
  SW_PS_ADJUSTMENT
} sw_ps_widget_t;

typedef struct _sw_ps_widget sw_ps_widget;
typedef struct _sw_ps_adjuster sw_ps_adjuster;

struct _sw_ps_widget {
  sw_ps_widget_t type;
  union {
    GtkWidget * toggle_button;
    sw_param * known_param;
    GtkObject * adjustment;
  } w;
};

struct _sw_ps_adjuster {
  sw_proc * proc;
  sw_view * view;
  sw_param_set pset;
  GtkWidget * window;
  GtkWidget * frame;
  GtkWidget * vbox;
  sw_ps_widget * widgets;
  GList * plsk_list;
};

static sw_ps_adjuster *
ps_adjuster_new (sw_proc * proc, sw_view * view, sw_param_set pset,
		 GtkWidget * window)
{
  sw_ps_adjuster * ps;

  ps = g_malloc (sizeof (*ps));

  ps->proc = proc;
  ps->view = view;
  ps->pset = pset;
  ps->window = window;

  ps->frame = NULL;
  ps->vbox = NULL;

  ps->widgets = g_malloc (sizeof (sw_ps_widget) * proc->nr_params);
  ps->plsk_list = NULL;

  return ps;
}

static void
ps_adjuster_destroy (sw_ps_adjuster * ps)
{
#if 0 /* XXX */
  GList * gl;

  g_free (ps->pset);

  for (gl = ps->plsk_list; gl; gl = gl->next) {
    g_free (gl->data);
  }
#endif
}

static void
get_param_values (sw_proc * proc, sw_param_set pset, sw_ps_widget * widgets)
{
  gint i;
  sw_param_spec * pspec;
  gfloat value;

  for (i=0; i < proc->nr_params; i++) {
    pspec = &proc->param_specs[i];

    switch (widgets[i].type) {
    case SW_PS_TOGGLE_BUTTON:
      /* Assume boolean */
      pset[i].b = gtk_toggle_button_get_active
	(GTK_TOGGLE_BUTTON(widgets[i].w.toggle_button));
      break;
    case SW_PS_KNOWN_PARAM:
      param_copy (widgets[i].w.known_param, &pset[i]);
      break;
    case SW_PS_ADJUSTMENT:
      value = GTK_ADJUSTMENT(widgets[i].w.adjustment)->value;
      switch (pspec->type) {
      case SWEEP_TYPE_BOOL:
	pset[i].b = (value == 0.0);
	break;
      case SWEEP_TYPE_INT:
	pset[i].i = (sw_int)(value);
	break;
      case SWEEP_TYPE_FLOAT:
	pset[i].f = (sw_float)(value);
	break;
      default:

	g_assert_not_reached ();

	/* pset[i].i = 0; */
	break;
      }
      break;
    default:
      break;
    }
  }
}

/*
 * Callback for OK button of param_set_adjuster
 */
static void
param_set_apply_cb (GtkWidget * widget, gpointer data)
{
  sw_ps_adjuster * ps = (sw_ps_adjuster *)data;

  get_param_values (ps->proc, ps->pset, ps->widgets);

#ifdef DEBUG
  print_param_set (ps->proc, ps->pset);
#endif

  if (ps->proc->apply) {
    ps->proc->apply (ps->view->sample, ps->pset, ps->proc->custom_data);
  }

  gtk_widget_destroy (ps->window);

  ps_adjuster_destroy (ps);
}

/*
 * Callback for Cancel button of param_set_adjuster
 */
static void
param_set_cancel_cb (GtkWidget * widget, gpointer data)
{
  sw_ps_adjuster * ps = (sw_ps_adjuster *)data;

  gtk_widget_destroy (ps->window);

  ps_adjuster_destroy (ps);
}

typedef struct _sw_pl_set_known sw_pl_set_known;

struct _sw_pl_set_known {
  sw_param ** p1;
  sw_param * p2;
};

/*
 * Callback for setting known parameters from lists
 */
static void
param_list_set_known_cb (GtkWidget * widget, gpointer data)
{
  sw_pl_set_known * plsk = (sw_pl_set_known *)data;

  *(plsk->p1) = plsk->p2;
}

static GtkWidget *
create_param_set_vbox (sw_ps_adjuster * ps)
{
  gint i, j;

  sw_proc * proc = ps->proc;
  sw_param_set pset = ps->pset;

  sw_param_spec * pspec;
  sw_pl_set_known * plsk;
  gint nr_options;
  int valid=0;

#define BUF_LEN 64
  gchar buf[BUF_LEN];

  GtkWidget * vbox;
  GtkWidget * hbox;
  GtkWidget * label;
  GtkWidget * optionmenu;
  GtkWidget * menu;
  GtkWidget * menuitem;
  GtkWidget * checkbutton;
  GtkWidget * num_widget; /* numeric input widget: hscale or spinbutton */

  GtkObject * adj;
  gfloat value, lower, upper, step_inc, page_inc, page_size;

  vbox = gtk_vbox_new (FALSE, 2);

  for (i=0; i < proc->nr_params; i++) {
    hbox = gtk_hbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show (hbox);
    
    pspec = &proc->param_specs[i];

    if (pspec->type == SWEEP_TYPE_BOOL) {

      checkbutton = gtk_check_button_new_with_label (pspec->name);
      gtk_box_pack_start (GTK_BOX(hbox), checkbutton, FALSE, TRUE, 0);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton),
				    pset[i].b);

      gtk_widget_show (checkbutton);

      ps->widgets[i].type = SW_PS_TOGGLE_BUTTON;
      ps->widgets[i].w.toggle_button = checkbutton;

      /* YYY: yes, the naming seems inconsistent. GtkToggleButton is
       * the parent class of GtkCheckButton
       */

    } else {

      label = gtk_label_new (pspec->name);
      gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
      gtk_widget_show (label);

      if (pspec->constraint_type == SW_PARAM_CONSTRAINED_LIST) {

	optionmenu = gtk_option_menu_new ();
	gtk_box_pack_start (GTK_BOX(hbox), optionmenu, FALSE, FALSE, 0);
	gtk_widget_show (optionmenu);

	menu = gtk_menu_new ();

	ps->widgets[i].w.known_param = &pset[i];

	nr_options = pspec->constraint.list[0].i;
	plsk = g_malloc (sizeof (sw_pl_set_known) * nr_options);

	ps->plsk_list = g_list_append (ps->plsk_list, plsk);

	for (j=0; j < nr_options; j++) {
	  plsk[j].p1 = &ps->widgets[i].w.known_param;
	  plsk[j].p2 = &pspec->constraint.list[j+1];

	  snprint_param (buf, BUF_LEN, pspec->type,
			 pspec->constraint.list[j+1]);

	  menuitem = gtk_menu_item_new_with_label (buf);
	  gtk_menu_append (GTK_MENU(menu), menuitem);
	  gtk_widget_show (menuitem);

          gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
            GTK_SIGNAL_FUNC(param_list_set_known_cb), &plsk[j]);

	  if (!param_cmp (pspec->type, pspec->constraint.list[j+1], pset[i])) {
	    gtk_option_menu_set_history (GTK_OPTION_MENU(optionmenu), j);
	    ps->widgets[i].w.known_param = &pspec->constraint.list[j+1];
	  }
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU(optionmenu), menu);

	ps->widgets[i].type = SW_PS_KNOWN_PARAM;


#define ADJUSTER_NUMERIC(T, DIGITS) \
                                                                           \
	value = (gfloat) pset[i].##T##;                                    \
                                                                           \
        if (pspec->constraint_type == SW_PARAM_CONSTRAINED_NOT) {          \
          valid = 0;                                                       \
        } else if (pspec->constraint_type == SW_PARAM_CONSTRAINED_RANGE) { \
          valid = pspec->constraint.range->valid_mask;                     \
        }                                                                  \
	                                                                   \
	if (valid & SW_RANGE_LOWER_BOUND_VALID) {                          \
	  lower = (gfloat) pspec->constraint.range->lower.##T##;           \
	} else {                                                           \
	  lower = G_MINFLOAT;                                              \
	}                                                                  \
	                                                                   \
	if (valid & SW_RANGE_UPPER_BOUND_VALID) {                          \
	  upper = (gfloat) pspec->constraint.range->upper.##T##;           \
	} else {                                                           \
	  upper = G_MAXFLOAT;                                              \
	}                                                                  \
	                                                                   \
	if (valid & SW_RANGE_STEP_VALID) {                                 \
	  step_inc = (gfloat) pspec->constraint.range->step.##T##;         \
	} else {                                                           \
	  step_inc = (gfloat) 1.0;                                         \
	}                                                                  \
	page_inc = step_inc;                                               \
	page_size = step_inc;                                              \
                                                                           \
	adj = gtk_adjustment_new (value, lower, upper, step_inc,           \
				  page_inc, page_size);                    \
                                                                           \
	if ( (valid & SW_RANGE_LOWER_BOUND_VALID) &&                       \
	     (valid & SW_RANGE_UPPER_BOUND_VALID)) {                       \
	  num_widget = gtk_hscale_new (GTK_ADJUSTMENT(adj));               \
	  gtk_box_pack_start (GTK_BOX(hbox), num_widget, TRUE, TRUE, 0);   \
	} else {                                                           \
	  num_widget = gtk_spin_button_new (GTK_ADJUSTMENT(adj),           \
					    10.0, /* climb_rate */         \
					    (DIGITS)/* digits */           \
					    );                             \
          gtk_spin_button_set_numeric (GTK_SPIN_BUTTON(num_widget), TRUE); \
          gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON(num_widget),  \
                                         TRUE);                            \
          gtk_widget_set_usize (num_widget, 75, -1);                       \
	  gtk_box_pack_start (GTK_BOX(hbox), num_widget, TRUE, TRUE, 0);   \
	}                                                                  \
                                                                           \
	gtk_widget_show (num_widget);                                      \
        ps->widgets[i].type = SW_PS_ADJUSTMENT;                            \
        ps->widgets[i].w.adjustment = adj;

      } else if (pspec->type == SWEEP_TYPE_INT) {
	ADJUSTER_NUMERIC(i, 0);
      } else if (pspec->type == SWEEP_TYPE_FLOAT) {
	ADJUSTER_NUMERIC(f, 3);
      }
    }
  }

  return vbox;
}

/*
 * Callback for Suggest button of param_set_adjuster
 */
static void
param_set_suggest_cb (GtkWidget * widget, gpointer data)
{
  sw_ps_adjuster * ps = (sw_ps_adjuster *)data;

  GtkWidget * vbox;

  if (ps->proc->suggest) {
    ps->proc->suggest (ps->view->sample, ps->pset, ps->proc->custom_data);
  }

  gtk_widget_destroy (ps->vbox);

  vbox = create_param_set_vbox (ps);
  gtk_container_add (GTK_CONTAINER(ps->frame), vbox);
  gtk_widget_show (vbox);

  ps->vbox = vbox;
}

gint
create_param_set_adjuster (sw_proc * proc, sw_view * view,
			   sw_param_set pset)
{
  sw_ps_adjuster * ps;

  GtkWidget * window;
  GtkWidget * main_vbox;
  GtkWidget * vbox;
  GtkWidget * hbox;
  GtkWidget * frame;
  GtkWidget * button;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW(window), proc->name);

  ps = ps_adjuster_new (proc, view, pset, window);

  gtk_signal_connect (GTK_OBJECT(window), "destroy",
		      GTK_SIGNAL_FUNC(param_set_cancel_cb), ps);
  
  main_vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(window), main_vbox);
  gtk_widget_show (main_vbox);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(main_vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  button = gtk_button_new_with_label ("Suggest");
  gtk_box_pack_start (GTK_BOX(hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);
  gtk_signal_connect (GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC (param_set_suggest_cb), ps);

  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX(main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  vbox = create_param_set_vbox (ps);
  gtk_container_add (GTK_CONTAINER(frame), vbox);
  gtk_widget_show (vbox);

  ps->frame = frame;
  ps->vbox = vbox;

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(main_vbox), hbox, FALSE, FALSE, 4);
  gtk_widget_show (hbox);

  button = gtk_button_new_with_label ("OK");
  gtk_box_pack_start (GTK_BOX(hbox), button, TRUE, FALSE, 0);
  gtk_widget_show (button);
  gtk_signal_connect (GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC (param_set_apply_cb), ps);

  button = gtk_button_new_with_label ("Cancel");
  gtk_box_pack_start (GTK_BOX(hbox), button, TRUE, FALSE, 0);
  gtk_widget_show (button);
  gtk_signal_connect (GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC (param_set_cancel_cb), ps);

  gtk_widget_show (window);

  return 1;
}
