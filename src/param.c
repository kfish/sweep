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
#include <string.h>
#include <math.h>

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_types.h>
#include "sweep_app.h"
#include "interface.h"

#include "../pixmaps/ladlogo.xpm"

extern GtkStyle * style_bw;

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
sw_param_set_new (sw_procedure * proc)
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
    if (p.b) snprintf (s, n, _("TRUE"));
    else snprintf (s, n, _("FALSE"));
    break;
  case SWEEP_TYPE_INT:
    snprintf (s, n, "%d", p.i);
    break;
  case SWEEP_TYPE_FLOAT:
    snprintf (s, n, "%f", p.f);
    break;
  case SWEEP_TYPE_STRING:
    snprintf (s, n, "%s", _(p.s));
    break;
  default:
    break;
  }
}

static void
print_param (sw_param_type type, sw_param p)
{
  gchar buf[64];

  snprint_param (buf, sizeof (buf), type, p);
  printf ("%s", buf);
}

void
print_param_set (sw_procedure * proc, sw_param_set pset)
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
  sw_procedure * proc;
  sw_view * view;
  sw_param_set pset;
  GtkWidget * window;
  GtkWidget * scrolled;
  GtkWidget * table;
  sw_ps_widget * widgets;
  GList * plsk_list;
};

static sw_ps_adjuster *
ps_adjuster_new (sw_procedure * proc, sw_view * view, sw_param_set pset,
		 GtkWidget * window)
{
  sw_ps_adjuster * ps;

  ps = g_malloc (sizeof (*ps));

  ps->proc = proc;
  ps->view = view;
  ps->pset = pset;
  ps->window = window;

  ps->scrolled = NULL;
  ps->table = NULL;

  ps->widgets = g_malloc (sizeof (sw_ps_widget) * proc->nr_params);
  ps->plsk_list = NULL;

  return ps;
}

static void
ps_adjuster_destroy (sw_ps_adjuster * ps)
{
}

static void
get_param_values (sw_procedure * proc, sw_param_set pset, sw_ps_widget * widgets)
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
    ps->proc->apply (ps->view->sample,
		     ps->pset, ps->proc->custom_data);
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
create_param_set_table (sw_ps_adjuster * ps)
{
  gint i, j;

  sw_procedure * proc = ps->proc;
  sw_param_set pset = ps->pset;

  sw_param_spec * pspec;
  sw_pl_set_known * plsk;
  gint nr_options;
  int valid=0;

  gchar buf[64];

  GtkWidget * table;
  GtkWidget * hbox;
  GtkWidget * label;
  GtkWidget * optionmenu;
  GtkWidget * menu;
  GtkWidget * menuitem;
  GtkWidget * checkbutton;
  GtkWidget * num_widget; /* numeric input widget: hscale or spinbutton */

  GtkObject * adj;
  gfloat value, lower, upper, step_inc, page_inc, page_size;
  gint digits = 1; /* nr. of decimal places to show on hscale */

  table = gtk_table_new (proc->nr_params, 2, FALSE);

  for (i=0; i < proc->nr_params; i++) {    
    pspec = &proc->param_specs[i];

    if (pspec->type == SWEEP_TYPE_BOOL) {

      checkbutton = gtk_check_button_new_with_label (_(pspec->name));
      gtk_table_attach (GTK_TABLE (table), checkbutton, 0, 2, i, i+1,
			GTK_FILL, GTK_FILL, 0, 0);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton),
				    pset[i].b);

      gtk_widget_show (checkbutton);

      ps->widgets[i].type = SW_PS_TOGGLE_BUTTON;
      ps->widgets[i].w.toggle_button = checkbutton;

      /* YYY: yes, the naming seems inconsistent. GtkToggleButton is
       * the parent class of GtkCheckButton
       */

    } else {

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (table), hbox, 0, 1, i, i+1,
			GTK_FILL|GTK_SHRINK, GTK_SHRINK, 4, 4);
      gtk_widget_show (hbox);

      label = gtk_label_new (_(pspec->name));
      gtk_box_pack_end (GTK_BOX(hbox), label, FALSE, FALSE, 4);
      gtk_widget_show (label);

      if (pspec->constraint_type == SW_PARAM_CONSTRAINED_LIST) {

	optionmenu = gtk_option_menu_new ();
	gtk_table_attach (GTK_TABLE (table), optionmenu, 1, 2, i, i+1,
			  GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);
	gtk_widget_show (optionmenu);

	menu = gtk_menu_new ();

	ps->widgets[i].w.known_param = &pset[i];

	nr_options = pspec->constraint.list[0].i;
	plsk = g_malloc (sizeof (sw_pl_set_known) * nr_options);

	ps->plsk_list = g_list_append (ps->plsk_list, plsk);

	for (j=0; j < nr_options; j++) {
	  plsk[j].p1 = &ps->widgets[i].w.known_param;
	  plsk[j].p2 = &pspec->constraint.list[j+1];

	  snprint_param (buf, sizeof (buf), pspec->type,
			 pspec->constraint.list[j+1]);

	  menuitem = gtk_menu_item_new_with_label (buf);
	  gtk_menu_append (GTK_MENU(menu), menuitem);
	  gtk_widget_show (menuitem);

          g_signal_connect (G_OBJECT(menuitem), "activate",
            G_CALLBACK(param_list_set_known_cb), &plsk[j]);

	  if (!param_cmp (pspec->type, pspec->constraint.list[j+1], pset[i])) {
	    gtk_option_menu_set_history (GTK_OPTION_MENU(optionmenu), j);
	    ps->widgets[i].w.known_param = &pspec->constraint.list[j+1];
	  }
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU(optionmenu), menu);

	ps->widgets[i].type = SW_PS_KNOWN_PARAM;


#define ADJUSTER_NUMERIC(T, DIGITS) \
                                                                           \
	value = (gfloat) pset[i].T;                                        \
                                                                           \
        if (pspec->constraint_type == SW_PARAM_CONSTRAINED_NOT) {          \
          valid = 0;                                                       \
        } else if (pspec->constraint_type == SW_PARAM_CONSTRAINED_RANGE) { \
          valid = pspec->constraint.range->valid_mask;                     \
        }                                                                  \
	                                                                   \
	if (valid & SW_RANGE_LOWER_BOUND_VALID) {                          \
	  lower = (gfloat) pspec->constraint.range->lower.T;               \
	} else {                                                           \
	  lower = G_MINFLOAT;                                              \
	}                                                                  \
	                                                                   \
	if (valid & SW_RANGE_UPPER_BOUND_VALID) {                          \
	  upper = (gfloat) pspec->constraint.range->upper.T;               \
	} else {                                                           \
	  upper = G_MAXFLOAT;                                              \
	}                                                                  \
	                                                                   \
	if (valid & SW_RANGE_STEP_VALID) {                                 \
	  step_inc = (gfloat) pspec->constraint.range->step.T;             \
	} else if (lower != G_MINFLOAT && upper != G_MAXFLOAT) {           \
          step_inc = (upper - lower) / 100.0;                              \
        } else {                                                           \
	  step_inc = (gfloat) 0.01;                                        \
	}                                                                  \
	page_inc = step_inc;                                               \
	page_size = step_inc;                                              \
        if (step_inc < 1.0)                                                \
          digits = - (gint)ceil(log10((double)step_inc));                  \
        else                                                               \
          digits = 1;                                                      \
                                                                           \
	adj = gtk_adjustment_new (value, lower, upper, step_inc,           \
				  page_inc, page_size);                    \
                                                                           \
	if ( (valid & SW_RANGE_LOWER_BOUND_VALID) &&                       \
	     (valid & SW_RANGE_UPPER_BOUND_VALID)) {                       \
          GTK_ADJUSTMENT(adj)->upper += step_inc;                          \
	  num_widget = gtk_hscale_new (GTK_ADJUSTMENT(adj));               \
          gtk_scale_set_digits (GTK_SCALE(num_widget), digits);            \
          gtk_widget_set_size_request (num_widget, 75, -1);                       \
	  gtk_table_attach (GTK_TABLE (table), num_widget, 1, 2, i, i+1,   \
			    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);        \
	} else {                                                           \
	  num_widget = gtk_spin_button_new (GTK_ADJUSTMENT(adj),           \
					    10.0, /* climb_rate */         \
					    (DIGITS)/* digits */           \
					    );                             \
          gtk_spin_button_set_numeric (GTK_SPIN_BUTTON(num_widget), TRUE); \
          gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON(num_widget),  \
                                         TRUE);                            \
          gtk_widget_set_size_request (num_widget, 75, -1);                       \
	  gtk_table_attach (GTK_TABLE (table), num_widget, 1, 2, i, i+1,   \
			    GTK_FILL|GTK_EXPAND, GTK_SHRINK, 0, 0);        \
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

  return table;
}

/*
 * Callback for Suggest button of param_set_adjuster
 */
static void
param_set_suggest_cb (GtkWidget * widget, gpointer data)
{
  sw_ps_adjuster * ps = (sw_ps_adjuster *)data;

  GtkWidget * table;

  if (ps->proc->suggest) {
    ps->proc->suggest (ps->view->sample,
		       ps->pset, ps->proc->custom_data);
  }

  gtk_widget_destroy (ps->table);

  table = create_param_set_table (ps);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (ps->scrolled),
					 table);
  gtk_widget_show (table);

  ps->table = table;
}

gint
create_param_set_adjuster (sw_procedure * proc, sw_view * view,
			   sw_param_set pset)
{
  sw_ps_adjuster * ps;
  GtkWidget * window;
  GtkWidget * main_vbox;
  GtkWidget * pixmap;
  GtkWidget * label;
  GtkWidget * scrolled;
  GtkWidget * ebox;
  GtkWidget * hbox;
  GtkWidget * vbox;
  GtkWidget * table;
  GtkWidget * frame;
  GtkWidget * button, * ok_button;

  /* Place the meta info about the plugin in a text widget (white background)
   *  aligned to the left of the parameter settings by defining _USE_TEXT
   */
/*#define _USE_TEXT*/

#ifdef _USE_TEXT
  GtkWidget * text;

  gchar buf[1024];
  gint n;

#endif /* _USE_TEXT */

  window = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW(window), _(proc->name));
  /*  gtk_container_border_width (GTK_CONTAINER (window), 8);*/

  ps = ps_adjuster_new (proc, view, pset, window);

  g_signal_connect (G_OBJECT(window), "destroy",
		      G_CALLBACK(param_set_cancel_cb), ps);

#ifdef _USE_TEXT
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(window)->vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  text = gtk_text_new (NULL, NULL);
  gtk_text_set_editable (GTK_TEXT (text), FALSE);
  gtk_text_set_word_wrap (GTK_TEXT (text), FALSE);
  gtk_widget_set_size_request (text, 320, -1);
  gtk_box_pack_start (GTK_BOX (hbox), text, FALSE, FALSE, 0);
  gtk_widget_show (text);

  main_vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), main_vbox, TRUE, TRUE, 0);
  gtk_widget_show (main_vbox);

  vbox = main_vbox;

  gtk_text_freeze (GTK_TEXT (text));

#else

  main_vbox = GTK_DIALOG(window)->vbox;

  ebox = gtk_event_box_new ();
  gtk_box_pack_start (GTK_BOX(main_vbox), ebox, FALSE, FALSE, 0);
  gtk_widget_set_style (ebox, style_bw);
  gtk_widget_show (ebox);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(ebox), hbox);  
  gtk_container_set_border_width (GTK_CONTAINER(hbox), 4);
  gtk_widget_show (hbox);

  pixmap = create_widget_from_xpm (window, ladlogo_xpm);
  gtk_box_pack_start (GTK_BOX(hbox), pixmap, FALSE, FALSE, 0);
  gtk_widget_show (pixmap);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);


#endif

  if (proc->name != NULL) {

#ifdef _USE_TEXT
/*
 * font =
 * gdk_font_load("-Adobe-Helvetica-Medium-R-Normal--*-140-*-*-*-*-*-*");
 *
 * n = snprintf (buf, sizeof (buf), "%s\n\n", _(proc->name));
 * gtk_text_insert (GTK_TEXT (text), font, NULL, NULL, buf, n);
 */
#else

/* pangoise?
 *  
 * style = gtk_style_new ();
 * gdk_font_unref (style->font);
 * style->font =
 * gdk_font_load("-adobe-helvetica-medium-r-normal-*-*-180-*-*-*-*-*-*");
 * gtk_widget_push_style (style);
 */
    label = gtk_label_new (_(proc->name));
    gtk_misc_set_padding (GTK_MISC (label), 10, 10);
    gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

/*  gtk_widget_pop_style ();*/

#endif
  }

#ifdef _USE_TEXT
  n = 0;

  if (proc->description != NULL) {
    n = snprintf (buf, sizeof (buf), "%s\n\n", _(proc->description));
  }

  if (n < sizeof (buf) && proc->author != NULL) {
    n += snprintf (buf+n, sizeof (buf)-n, "by %s", proc->author);
  }

  if (n < sizeof (buf) && proc->copyright != NULL) {
    n += snprintf (buf+n, sizeof (buf)-n, ", %s.\n", proc->copyright);
  }

  if (n < sizeof (buf) && proc->url != NULL) {
    n += snprintf (buf+n, sizeof (buf)-n, "\nFor more information see\n%s\n",
		   proc->url);
  }

  if (n > 0) {
    gtk_text_insert (GTK_TEXT (text), NULL, NULL, NULL, buf, n);
  }

  gtk_text_thaw (GTK_TEXT (text));

#else
  if (proc->description != NULL) {
    label = gtk_label_new (_(proc->description));
    gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);
  }

  if (proc->author != NULL) {
    label = gtk_label_new (proc->author);
    gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);
  }

  if (proc->copyright != NULL) {
    label = gtk_label_new (proc->copyright);
    gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);
  }

  if (proc->url != NULL) {
    label = gtk_label_new (proc->url);
    gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);
  }
#endif

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(main_vbox), hbox, TRUE, TRUE, 8);
  gtk_widget_show (hbox);

  frame = gtk_frame_new (_("Parameters"));
  gtk_box_pack_start (GTK_BOX(hbox), frame, TRUE, TRUE, 8);
  gtk_widget_show (frame);

  vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_container_set_border_width (GTK_CONTAINER(vbox), 8);
  gtk_widget_show (vbox);

  button = gtk_button_new_with_label (_("Defaults"));
  gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, FALSE, 8);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK (param_set_suggest_cb), ps);

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (scrolled, -1, 240);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
				  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 8);
  gtk_widget_show (scrolled);

  table = create_param_set_table (ps);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled),
					 table);
  gtk_widget_show (table);

  ps->scrolled = scrolled;
  ps->table = table;

  ok_button = gtk_button_new_with_label (_("OK"));
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (ok_button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(window)->action_area), ok_button,
		      TRUE, TRUE, 0);
  gtk_widget_show (ok_button);
  g_signal_connect (G_OBJECT(ok_button), "clicked",
		      G_CALLBACK (param_set_apply_cb), ps);
  

  button = gtk_button_new_with_label (_("Cancel"));
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (button), GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(window)->action_area), button,
		      TRUE, TRUE, 0);
  gtk_widget_show (button);
  g_signal_connect (G_OBJECT(button), "clicked",
		      G_CALLBACK (param_set_cancel_cb), ps);

  gtk_widget_grab_default (ok_button);

  gtk_widget_show (window);

  return 1;
}
