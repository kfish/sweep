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
#include <string.h>

#include "sweep_types.h"
#include "sweep_typeconvert.h"
#include "sweep_undo.h"
#include "edit.h"
#include "format.h"
#include "sample.h"

edit_buffer * ebuf = NULL;

static edit_region *
edit_region_new (sw_format * format, sw_framecount_t start, sw_framecount_t end, gpointer data)
{
  edit_region * er;
  sw_framecount_t len;

  er = g_malloc (sizeof(edit_region));

  er->start = start;
  er->end = end;

  len = frames_to_bytes (format, end-start);
  er->data = g_malloc (len);

  memcpy (er->data, data, len);

  return er;
}

static edit_region *
edit_region_new0 (sw_format * format, sw_framecount_t start, sw_framecount_t end, gpointer data0)
{
  sw_framecount_t offset;
  int o2;

  offset = frames_to_bytes (format, start);
  o2 = (int)offset; /* XXX */

  return edit_region_new (format, start, end, data0+o2);
}

static edit_region *
edit_region_copy (sw_format * format, edit_region * oer)
{
  edit_region * er;

  er = edit_region_new (format, oer->start, oer->end, oer->data);

  return er;
}

edit_buffer *
edit_buffer_new (sw_format * format)
{
  edit_buffer * eb;

  eb = g_malloc0 (sizeof(edit_buffer));
  eb->format = format_copy (format);
  eb->regions = NULL;

  return eb;
}

edit_buffer *
edit_buffer_copy (edit_buffer * oeb)
{
  edit_buffer * eb;
  GList * gl;
  edit_region * oer, * er;

  eb = edit_buffer_new (oeb->format);

  for (gl = oeb->regions; gl; gl = gl->next) {
    oer = (edit_region *)gl->data;

    er = edit_region_copy (oeb->format, oer);
 
    eb->regions = g_list_append (eb->regions, er);
  }

  return eb;
}

static void
edit_buffer_clear (edit_buffer * eb)
{
  GList * gl;
  edit_region * er;

  if (!eb) return;

  g_free (eb->format);

  for (gl = eb->regions; gl; gl = gl->next) {
    er = (edit_region *)gl->data;
    g_free (er->data);
  }
  g_list_free (eb->regions);

  eb->format = NULL;
  eb->regions = NULL;
}

void
edit_buffer_destroy (edit_buffer * eb)
{
  edit_buffer_clear (eb);
  g_free (eb);
}

static void
ebuf_clear (void)
{
  if (!ebuf) return;

  edit_buffer_clear (ebuf);
  g_free (ebuf);
  ebuf = NULL;
}

static gint
edit_buffer_length (edit_buffer * eb)
{
  GList * gl;
  edit_region * er;
  gint length = 0;

  for (gl = eb->regions; gl; gl = gl->next) {
    er = (edit_region *)gl->data;

    length += (er->end - er->start);
  }

  return length;
}

static edit_buffer *
edit_buffer_from_soundfile (sw_soundfile * soundfile)
{
  edit_buffer * eb;
  GList * gl;
  sw_sel * sel;
  edit_region * er;

  eb = edit_buffer_new (soundfile->format);

  for (gl = soundfile->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    er = edit_region_new0 (eb->format,
			   sel->sel_start, sel->sel_end,
			   soundfile->data);

#ifdef DEBUG
    printf("adding eb region [%ld - %ld]\n", sel->sel_start, sel->sel_end);
#endif

    eb->regions = g_list_append (eb->regions, er);
  }

  return eb;
}

edit_buffer *
edit_buffer_from_sample (sw_sample * sample)
{
  return edit_buffer_from_soundfile (sample->soundfile);
}

static sw_sample *
sample_from_edit_buffer (edit_buffer * eb)
{
  sw_sample * s;
  GList * gl;
  edit_region * er;
  sw_framecount_t offset0 = 0, start, length;
  sw_framecount_t offset, len;

  /* Get length of new sample */
  gl = eb->regions;

  if (!gl) return NULL;

  start = ((edit_region *)gl->data)->start;
  
  for (; gl->next; gl = gl->next);
  er = (edit_region *)gl->data;
  length = er->end - start;

  s = sample_new_empty (NULL, "Untitled",
			eb->format->channels,
			eb->format->rate,
			length);

  offset0 = frames_to_bytes (eb->format, start);

  for (gl = eb->regions; gl; gl = gl->next) {
    er = (edit_region *)gl->data;
    offset = frames_to_bytes (eb->format, er->start) - offset0;
    len = frames_to_bytes (eb->format, er->end - er->start);

    memcpy ((gpointer)(s->soundfile->data + offset), er->data, len);

#ifdef DEBUG
    printf("Adding sample region [%ld - %ld]\n", er->start, er->end);
#endif
  }

  return s;
}

/* returns new soundfile */
sw_soundfile *
splice_out_sel (sw_soundfile * soundfile)
{
  sw_format * f = soundfile->format;
  gint length;
  GList * gl;
  sw_sel * osel, * sel;
  sw_soundfile * out;
  gpointer d;
  sw_framecount_t offset, len;

  if (!soundfile->sels) {
    printf ("Nothing to splice out.\n");
    return soundfile;
  }

  length = soundfile->nr_frames - soundfile_selection_length (soundfile);

#ifdef DEBUG
  printf("Splice out: remaining length %d\n", length);
#endif

  out = soundfile_new_empty (soundfile->directory, soundfile->filename,
			 f->channels, f->rate, length);

  d = out->data;

  gl = soundfile->sels;
  sel = osel = (sw_sel *)gl->data;
  if (osel->sel_start > 0) {
    len = frames_to_bytes (f, osel->sel_start);
    memcpy (d, soundfile->data, len);
    d += len;
  }
  gl = gl->next;

  for (; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    offset = frames_to_bytes (f, osel->sel_end);
    len = frames_to_bytes (f, sel->sel_start - osel->sel_end);
    memcpy (d, (gpointer)(soundfile->data + offset), len);
    d += len;

    osel = sel;
  }

  if (sel->sel_end != soundfile->nr_frames) {
    offset = frames_to_bytes (f, sel->sel_end);
#ifdef DEBUG
    printf("Calculated offset %ld [=== %ld]\n", offset, sel->sel_end);
#endif

    len = frames_to_bytes (f, soundfile->nr_frames - sel->sel_end);
    memcpy (d, (gpointer)(soundfile->data + offset), len);
  }

  return out;
}

static void
soundfile_set_sel_from_eb (sw_soundfile * soundfile, edit_buffer * eb)
{
  GList * gl;
  edit_region * er;

  if (soundfile->sels)
    soundfile_clear_selection (soundfile);

  if (!eb) return;
  
  for (gl = eb->regions; gl; gl = gl->next) {
    er = (edit_region *)gl->data;

    if (er->start > soundfile->nr_frames) break;

    soundfile_add_selection_1 (soundfile, er->start, MIN(er->end, soundfile->nr_frames));
  }
}

/* returns new soundfile */
sw_soundfile *
splice_in_eb (sw_soundfile * soundfile, edit_buffer * eb)
{
  sw_format * f = soundfile->format;
  gint length;
  GList * gl;
  edit_region * er;
  sw_framecount_t prev_end = 0;
  sw_soundfile * out;
  gpointer di, d;
  sw_framecount_t len;

  if (!eb) {
    return soundfile;
  }

  length = soundfile->nr_frames + edit_buffer_length (eb);

  di = soundfile->data;

  out = soundfile_new_empty (soundfile->directory, soundfile->filename,
			 f->channels, f->rate, length);

  d = out->data;

  gl = eb->regions;
  er = (edit_region *)gl->data;
  if (er->start <= 0) {
    len = frames_to_bytes (f, er->end - er->start);
    memcpy (d, er->data, len);
    d += len;
    prev_end = er->end;
    gl = gl->next;
  }

  for (; gl; gl = gl->next) {
    er = (edit_region *)gl->data;

    /* Copy sample data in */
    len = frames_to_bytes (f, er->start - prev_end);
    memcpy (d, di, len);
    di += len;
    d += len;

    /* Copy edit_region data in */
    len = frames_to_bytes (f, er->end - er->start);
    memcpy (d, er->data, len);
    d += len;

    prev_end = er->end;
  }

  /* Copy remaining sample data in, if any */
  if (prev_end < length) {
    len = frames_to_bytes (f, length - prev_end);
    memcpy (d, di, len);
  }
 
  soundfile_set_sel_from_eb (out, eb);
 
  return out;
}

/* Modifies soundfile */
static sw_soundfile *
edit_clear_sel (sw_soundfile * soundfile)
{
  sw_format * f = soundfile->format;
  GList * gl;
  sw_sel * sel;
  sw_framecount_t offset, len;

  for (gl = soundfile->sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    offset = frames_to_bytes (f, sel->sel_start);
    len = frames_to_bytes (f, sel->sel_end - sel->sel_start);

    memset ((gpointer)(soundfile->data + offset), 0, (size_t)len);
  }

  return soundfile;
}

/* New sample */
static sw_soundfile *
paste_at (sw_soundfile * soundfile, edit_buffer * eb)
{
  sw_format * f = soundfile->format;
  sw_framecount_t paste_point = 0;
  sw_framecount_t paste_offset = 0, len;
  sw_framecount_t length, paste_length;
  GList * gl;
  edit_region * er;
  gpointer d;
  sw_soundfile * out;

  if (soundfile->sels) {
    paste_point = ((sw_sel *)(soundfile->sels)->data)->sel_start;
    paste_offset = frames_to_bytes (f, paste_point);
  }

  paste_length = edit_buffer_length (eb);
  length = soundfile->nr_frames + paste_length;

  out = soundfile_new_empty (soundfile->directory, soundfile->filename,
			 f->channels, f->rate, length);

  d = out->data;

  if (paste_point > 0) {
    len = paste_offset;
    memcpy (d, soundfile->data, len);
    d += len;
  }

  for (gl = eb->regions; gl; gl = gl->next) {
    er = (edit_region *)gl->data;

    len = frames_to_bytes (f, er->end - er->start);
    memcpy (d, er->data, len);
    d += len;
  }

  if (paste_point < soundfile->nr_frames) {
    len = frames_to_bytes (f, soundfile->nr_frames) - paste_offset;
    memcpy (d, (gpointer)(soundfile->data + paste_offset), len);
  }

  /*
  soundfile_copyin_selection (soundfile, out);
  soundfile_selection_translate (out, paste_point);
  */

  soundfile_set_selection_1 (out, paste_point, paste_point + paste_length);

  return out;
}

/* Modifies sample */
sw_sample *
paste_over (sw_sample * sample, edit_buffer * eb)
{
  sw_format * f = sample->soundfile->format;
  sw_framecount_t offset, len;
  sw_framecount_t length;
  GList * gl;
  edit_region * er;

  if (!eb) return sample;

  length = sample->soundfile->nr_frames;

  for (gl = eb->regions; gl; gl = gl->next) {
    er = (edit_region *)gl->data;

    if (er->start > length) break;

    offset = frames_to_bytes (f, er->start);
    len = frames_to_bytes (f, MIN(er->end, length) - er->start);
    memcpy ((gpointer)(sample->soundfile->data + offset), er->data, len);
  }

  return sample;
}


/* No undo for copy -- is input only */
void
do_copy (sw_sample * sample)
{
  ebuf_clear ();

  ebuf = edit_buffer_from_sample (sample);
}

static sw_operation cut_op = {
  /*  OP_FILTER,*/
  /*  {(SweepFilter *)do_cut},*/
  (SweepCallback)undo_by_splice_in,
  (SweepFunction)splice_data_destroy,
  (SweepCallback)redo_by_splice_out,
  (SweepFunction)splice_data_destroy
};

sw_op_instance *
do_cut (sw_sample * sample)
{
  sw_soundfile * out;
  sw_op_instance * inst;
  edit_buffer * eb;

  inst = sw_op_instance_new ("Cut", &cut_op);
  eb = edit_buffer_from_sample (sample);

  ebuf_clear ();
  ebuf = edit_buffer_copy (eb);

  out = splice_out_sel (sample->soundfile);

  inst->redo_data = inst->undo_data =
    splice_data_new (eb);

  soundfile_destroy (sample->soundfile);
  sample->soundfile = out;

  sample_refresh_views (sample);

  register_operation (sample, inst);

  return inst;
}

static sw_operation clear_op = {
  (SweepCallback)undo_by_paste_over,
  (SweepFunction)paste_over_data_destroy,
  (SweepCallback)redo_by_paste_over /* redo_by_reperform */,
  (SweepFunction)paste_over_data_destroy
};

sw_op_instance *
do_clear (sw_sample * sample)
{
  sw_op_instance * inst;
  edit_buffer * old_eb, * new_eb;

  inst = sw_op_instance_new ("Clear", &clear_op);
  old_eb = edit_buffer_from_sample (sample);

  edit_clear_sel (sample->soundfile);
  new_eb = edit_buffer_from_sample (sample);

  inst->redo_data = inst->undo_data =
    paste_over_data_new (old_eb, new_eb);

  sample_refresh_views (sample);

  register_operation (sample, inst);

  return inst;
}

static sw_operation delete_op = {
  (SweepCallback)undo_by_splice_in,
  (SweepFunction)splice_data_destroy,
  (SweepCallback)redo_by_splice_out,
  (SweepFunction)splice_data_destroy
};

sw_op_instance *
do_delete (sw_sample * sample)
{
  sw_soundfile * out;
  sw_op_instance * inst;
  edit_buffer * eb;

  inst = sw_op_instance_new ("Delete", &delete_op);
  eb = edit_buffer_from_sample (sample);

  out = splice_out_sel (sample->soundfile);

  inst->redo_data = inst->undo_data =
    splice_data_new (eb);

  sample->soundfile = out;
  /* delete old */

  sample_refresh_views (sample);

  register_operation (sample, inst);

  return inst;
}

static sw_operation paste_in_op = {
  (SweepCallback)undo_by_splice_out,
  (SweepFunction)splice_data_destroy,
  (SweepCallback)redo_by_splice_in,
  (SweepFunction)splice_data_destroy
};

sw_op_instance *
do_paste_in (sw_sample * in, sw_sample ** out)
{
  sw_op_instance * inst;
  edit_buffer * eb;

  inst = sw_op_instance_new ("Paste in", &paste_in_op);
  eb = edit_buffer_copy (ebuf);

  (*out)->soundfile = splice_in_eb (in->soundfile, ebuf);

  inst->redo_data = inst->undo_data =
    splice_data_new (eb);

  sample_refresh_views (*out);

  return inst;
}

static sw_operation paste_at_op = {
  (SweepCallback)undo_by_splice_out,
  (SweepFunction)splice_data_destroy,
  (SweepCallback)redo_by_splice_in,
  (SweepFunction)splice_data_destroy
};

sw_op_instance *
do_paste_at (sw_sample * sample)
{
  sw_soundfile * out;
  sw_op_instance * inst;
  edit_buffer * eb1, *eb2;

  inst = sw_op_instance_new ("Paste", &paste_at_op);
  eb1 = edit_buffer_copy (ebuf);
  inst->undo_data = splice_data_new (eb1);

  out = paste_at (sample->soundfile, ebuf);
  eb2 = edit_buffer_from_soundfile (out);
  inst->redo_data = splice_data_new (eb2);

  sample->soundfile = out;
  /* delete old soundfile */

  sample_refresh_views (sample);

  register_operation (sample, inst);

  return inst;
}


static sw_operation paste_over_op = {
  (SweepCallback)undo_by_paste_over,
  (SweepFunction)paste_over_data_destroy,
  (SweepCallback)redo_by_paste_over,
  (SweepFunction)paste_over_data_destroy
};

sw_op_instance *
do_paste_over (sw_sample * in, sw_sample **out)
{
  sw_op_instance * inst;
  edit_buffer * old_eb, * new_eb;

  inst = sw_op_instance_new ("Paste over", &paste_over_op);
  old_eb = edit_buffer_from_sample (in);

  *out = paste_over (in, ebuf);
  new_eb = edit_buffer_from_sample (*out);

  inst->redo_data = inst->undo_data =
    paste_over_data_new (old_eb, new_eb);

  sample_refresh_views (*out);

  return inst;
}

void
do_paste_as_new (sw_sample * in, sw_sample ** out)
{
  *out = sample_from_edit_buffer (ebuf);
}
