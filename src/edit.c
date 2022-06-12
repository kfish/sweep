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

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sweep/sweep_i18n.h>
#include <sweep/sweep_types.h>
#include <sweep/sweep_typeconvert.h>
#include <sweep/sweep_undo.h>
#include <sweep/sweep_sounddata.h>
#include <sweep/sweep_sample.h>
#include <sweep/sweep_selection.h>

#include "sweep_app.h"
#include "edit.h"
#include "format.h"


sw_edit_buffer * ebuf = NULL;

void *
sweep_large_alloc_data (size_t len, void * data, int prot)
{
  void * ptr;

#if HAVE_MMAP
  FILE * f;
  int fd;

  if ((f = tmpfile ()) == NULL) {
    perror ("tmpfile failed in sweep_large_alloc_data");
    return NULL;
  }

  if (fwrite (data, len, 1, f) != 1) {
    perror ("short fwrite in sweep_large_alloc_data");
    return NULL;
  }

  if (fseek (f, 0, SEEK_SET) == -1) {
    perror ("fseek failed in sweep_large_alloc_data");
    return NULL;
  }

  if ((fd = fileno (f)) == -1) {
    perror ("fileno failed in sweep_large_alloc_data");
    return NULL;
  }

  if ((ptr = mmap (NULL, len, prot, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
    perror ("mmap failed in sweep_large_alloc_data");
    return NULL;
  }
#else
  ptr = g_malloc (len);

  memcpy (ptr, data, len);
#endif

  return ptr;
}

void *
sweep_large_alloc_zero (size_t len, int prot)
{
  void * ptr;

#if HAVE_MMAP
  int fd;

  if ((fd = open ("/dev/zero", O_RDWR)) == -1) {
    perror ("open failed in sweep_large_alloc_zero");
    return NULL;
  }

  if (lseek (fd, len+1, SEEK_SET) == (off_t)-1) {
    perror ("lseek failed in sweep_large_alloc_zero");
    return NULL;
  }

  if (write (fd, "", 1) == -1) {
    perror ("write failed in sweep_large_alloc_zero");
    return NULL;
  }

  if (lseek (fd, 0, SEEK_SET) == (off_t)-1) {
    perror ("lseek failed in sweep_large_alloc_zero");
    return NULL;
  }

  if ((ptr = mmap (NULL, len, prot, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
    perror ("mmap failed in sweep_large_alloc_zero");
    return NULL;
  }
#else
  ptr = g_malloc0 (len);
#endif

  return ptr;
}

void
sweep_large_free (void * ptr, size_t len)
{
#if HAVE_MMAP
  if (munmap (ptr, len) == -1)
    perror ("munmap failed in sweep_large_free()");
#else
  g_free (ptr);
#endif
}

static sw_edit_region *
edit_region_new (sw_format * format, sw_framecount_t start,
		 sw_framecount_t end, gpointer data)
{
  sw_edit_region * er;
  sw_framecount_t len;

  er = g_malloc (sizeof(sw_edit_region));

  er->start = start;
  er->end = end;

  len = frames_to_bytes (format, end-start);
  er->data = sweep_large_alloc_data (len, data, PROT_READ);

  return er;
}

static sw_edit_region *
edit_region_new0 (sw_format * format, sw_framecount_t start,
		  sw_framecount_t end, gpointer data0)
{
  sw_framecount_t offset;
  int o2;

  offset = frames_to_bytes (format, start);
  o2 = (int)offset; /* XXX */

  return edit_region_new (format, start, end, data0+o2);
}

static sw_edit_region *
edit_region_copy (sw_format * format, sw_edit_region * oer)
{
  sw_edit_region * er;

  er = edit_region_new (format, oer->start, oer->end, oer->data);

  return er;
}

sw_edit_buffer *
edit_buffer_new (sw_format * format)
{
  sw_edit_buffer * eb;

  eb = g_malloc0 (sizeof(sw_edit_buffer));
  eb->format = format_copy (format);
  eb->regions = NULL;
  eb->refcount = 1;

  return eb;
}

sw_edit_buffer *
edit_buffer_copy (sw_edit_buffer * oeb)
{
  sw_edit_buffer * eb;
  GList * gl;
  sw_edit_region * oer, * er;

  eb = edit_buffer_new (oeb->format);

  for (gl = oeb->regions; gl; gl = gl->next) {
    oer = (sw_edit_region *)gl->data;

    er = edit_region_copy (oeb->format, oer);

    eb->regions = g_list_append (eb->regions, er);
  }

  return eb;
}

sw_edit_buffer *
edit_buffer_ref (sw_edit_buffer * eb)
{
  eb->refcount++;
  return eb;
}

static void
edit_buffer_clear (sw_edit_buffer * eb)
{
  GList * gl;
  sw_edit_region * er;
  size_t len;

  if (!eb) return;

  for (gl = eb->regions; gl; gl = gl->next) {
    er = (sw_edit_region *)gl->data;
    if (er) {
      len = frames_to_bytes (eb->format, er->end - er->start);
      sweep_large_free (er->data, len);
    }
  }
  g_list_free (eb->regions);

  g_free (eb->format);

  eb->format = NULL;
  eb->regions = NULL;
  eb->refcount = 0;
}

void
edit_buffer_destroy (sw_edit_buffer * eb)
{
  eb->refcount--;
  if (eb->refcount > 0) return;

  edit_buffer_clear (eb);
  g_free (eb);
}

static void
ebuf_clear (void)
{
  if (!ebuf) return;

  edit_buffer_destroy (ebuf);

  ebuf = NULL;
}

static sw_framecount_t
edit_buffer_length (sw_edit_buffer * eb)
{
  GList * gl;
  sw_edit_region * er;
  sw_framecount_t length = 0;

  for (gl = eb->regions; gl; gl = gl->next) {
    er = (sw_edit_region *)gl->data;

    length += (er->end - er->start);
  }

  return length;
}

static sw_framecount_t
edit_buffer_width (sw_edit_buffer * eb)
{
  GList * gl;
  sw_edit_region * er;
  sw_framecount_t start, end;

  if (eb == NULL) return 0;

  if ((gl = eb->regions) == NULL) return 0;
  er = (sw_edit_region *)gl->data;
  start = er->start;

  gl = g_list_last (eb->regions);
  er = (sw_edit_region *)gl->data;
  end = er->end;

  return (end - start);
}

sw_framecount_t
clipboard_width (void)
{
  return edit_buffer_width (ebuf);
}

static sw_edit_buffer *
edit_buffer_from_sounddata_sels (sw_sounddata * sounddata, GList * sels)
{
  sw_edit_buffer * eb;
  GList * gl;
  sw_sel * sel;
  sw_edit_region * er;

  eb = edit_buffer_new (sounddata->format);

  for (gl = sels; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    er = edit_region_new0 (eb->format,
			   sel->sel_start, sel->sel_end,
			   sounddata->data);

#ifdef DEBUG
    printf("adding eb region [%ld - %ld]\n", sel->sel_start, sel->sel_end);
#endif

    eb->regions = g_list_append (eb->regions, er);
  }

  return eb;
}

static sw_edit_buffer *
edit_buffer_from_sounddata (sw_sounddata * sounddata)
{
  return edit_buffer_from_sounddata_sels (sounddata, sounddata->sels);
}

sw_edit_buffer *
edit_buffer_from_sample (sw_sample * sample)
{
  return edit_buffer_from_sounddata (sample->sounddata);
}

static GList *
sels_from_edit_buffer_offset (sw_edit_buffer * eb, sw_framecount_t offset)
{
  GList * gl, * sels = NULL;
  sw_edit_region * er;
  sw_framecount_t start;

  gl = eb->regions;
  if (!gl) return NULL;

  er = (sw_edit_region *)gl->data;
  start = er->start;

  for (gl = eb->regions; gl; gl = gl->next) {
    er = (sw_edit_region *)gl->data;

    sels = sels_add_selection_1 (sels, er->start - start + offset,
				 er->end - start + offset);
  }

  return sels;
}

static sw_sample *
sample_from_edit_buffer (sw_edit_buffer * eb)
{
  sw_sample * s;
  GList * gl;
  sw_edit_region * er;
  sw_framecount_t offset0 = 0, start, length;
  sw_framecount_t offset, len;

  /* Get length of new sample */
  gl = eb->regions;

  if (!gl) return NULL;

  er = (sw_edit_region *)gl->data;
  start = er->start;

  for (; gl->next; gl = gl->next);
  er = (sw_edit_region *)gl->data;
  length = er->end - start;

  s = sample_new_empty (NULL,
			eb->format->channels,
			eb->format->rate,
			length);

  offset0 = frames_to_bytes (eb->format, start);

  for (gl = eb->regions; gl; gl = gl->next) {
    er = (sw_edit_region *)gl->data;
    offset = frames_to_bytes (eb->format, er->start) - offset0;
    len = frames_to_bytes (eb->format, er->end - er->start);

    memcpy ((gpointer)(s->sounddata->data + offset), er->data, len);

    sounddata_add_selection_1 (s->sounddata, er->start - start,
			       er->end - start);

#ifdef DEBUG
    printf("Adding sample region [%ld - %ld]\n", er->start, er->end);
#endif
  }

  return s;
}

static void
head_dec_if_within (sw_head * head, sw_framecount_t lower,
		    sw_framecount_t upper, sw_framecount_t amount)
{
  if (head == NULL) return;

  g_mutex_lock (&head->head_mutex);

  if (head->stop_offset > lower && head->stop_offset < upper)
    head->stop_offset -= amount;

  if (head->offset > lower && head->offset < upper)
    head->offset -= amount;

  g_mutex_unlock (&head->head_mutex);
}

static void
head_inc_if_gt (sw_head * head, sw_framecount_t lower, sw_framecount_t amount)
{
  if (head == NULL) return;

  g_mutex_lock (&head->head_mutex);

  if (head->stop_offset > lower)
    head->stop_offset += amount;

  if (head->offset > lower)
    head->offset += amount;

  g_mutex_unlock (&head->head_mutex);
}

static void
head_set_if_within (sw_head * head, sw_framecount_t lower,
		    sw_framecount_t upper, sw_framecount_t value)
{
  if (head == NULL) return;

  g_mutex_lock (&head->head_mutex);

  if (head->stop_offset >= lower && head->stop_offset <= upper)
    head->stop_offset = value;

  if (head->offset >= lower && head->offset <= upper)
    head->offset = value;

  g_mutex_unlock (&head->head_mutex);
}

/* modifies sounddata */
sw_sample *
splice_out_sel (sw_sample * sample)
{
  sw_sounddata * sounddata = sample->sounddata;
  sw_format * f = sounddata->format;
  gint length;
  GList * gl;
  sw_sel * osel, * sel;
  /*sw_sounddata * out;*/
  gpointer d;
  sw_framecount_t offset, len, sel_length = 0;
  sw_framecount_t move_length, run_length;

  if (!sounddata->sels) {
    printf ("Nothing to splice out.\n");
    return sample;
  }

  length = sounddata->nr_frames - sounddata_selection_nr_frames (sounddata);
  run_length = 0;

#ifdef DEBUG
  printf("Splice out: remaining length %d\n", length);
#endif

  d = sounddata->data;

  /* XXX: Force splice outs to be atomic wrt. to cancellation. For
   * multi-region selections it would be nicer to build the redo data
   * incrementally, but wtf.
   * Each region's memmove needs to be treated as atomic anyway so the
   * gain isn't so much.
   */
  g_mutex_lock (&sample->ops_mutex);

  gl = sounddata->sels;
  sel = osel = (sw_sel *)gl->data;
  if (osel->sel_start > 0) {

    if (sample->user_offset >= sel->sel_start &&
	sample->user_offset <= sel->sel_end) {
      sample->user_offset = sel->sel_start - sel_length;;
    }

    head_set_if_within (sample->play_head, sel->sel_start, sel->sel_end,
			sel->sel_start - sel_length);

    head_set_if_within (sample->rec_head, sel->sel_start, sel->sel_end,
			sel->sel_start - sel_length);

    sel_length = osel->sel_end - osel->sel_start;

    move_length = osel->sel_start;

    len = frames_to_bytes (f, move_length);
    d += len;

    run_length += move_length;
    sample_set_progress_percent (sample, run_length / (length/100));
  }
  for (gl = gl->next; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    /* Move user offset */
    if (sample->user_offset > osel->sel_end &&
	sample->user_offset < sel->sel_start) {
      sample->user_offset -= sel_length;
    }

    if (sample->user_offset >= sel->sel_start &&
	sample->user_offset <= sel->sel_end) {
      sample->user_offset = sel->sel_start - sel_length;;
    }

    head_dec_if_within (sample->play_head, osel->sel_end, sel->sel_start,
			sel_length);
    head_set_if_within (sample->play_head, sel->sel_start, sel->sel_end,
			sel->sel_start - sel_length);

    head_dec_if_within (sample->rec_head, osel->sel_end, sel->sel_start,
			sel_length);
    head_set_if_within (sample->rec_head, sel->sel_start, sel->sel_end,
			sel->sel_start - sel_length);

    sel_length += sel->sel_end - sel->sel_start;

    offset = frames_to_bytes (f, osel->sel_end);

    move_length = sel->sel_start - osel->sel_end;
    len = frames_to_bytes (f, move_length);
    g_memmove (d, (gpointer)(sounddata->data + offset), len);
    d += len;

    run_length += move_length;
    sample_set_progress_percent (sample, run_length / (length/100));

    osel = sel;
  }

  /* Move offsets occurring after last sel */
  if (sample->user_offset > sel->sel_end) {
    sample->user_offset -= sel_length;
  }

  head_dec_if_within (sample->play_head, sel->sel_end, FRAMECOUNT_MAX,
		      sel_length);

  head_dec_if_within (sample->rec_head, sel->sel_end, FRAMECOUNT_MAX,
		      sel_length);

  if (sel->sel_end != sounddata->nr_frames) {
    offset = frames_to_bytes (f, sel->sel_end);

    move_length = sounddata->nr_frames - sel->sel_end;

    len = frames_to_bytes (f, move_length);
    g_memmove (d, (gpointer)(sounddata->data + offset), len);

    run_length += move_length;
    sample_set_progress_percent (sample, run_length / (length/100));
  }

  d = g_realloc (sounddata->data, frames_to_bytes(f, length));
  sounddata->data = d;
  sounddata->nr_frames = length;

  sounddata_clear_selection (sounddata);

  sample_set_progress_percent (sample, 100);

  g_mutex_unlock (&sample->ops_mutex);

  return sample;
}

static void
sounddata_set_sel_from_eb (sw_sounddata * sounddata, sw_edit_buffer * eb)
{
  GList * gl;
  sw_edit_region * er;

  if (sounddata->sels)
    sounddata_clear_selection (sounddata);

  if (!eb) return;

  for (gl = eb->regions; gl; gl = gl->next) {
    er = (sw_edit_region *)gl->data;

    if (er->start > sounddata->nr_frames) break;

    sounddata_add_selection_1 (sounddata, er->start, MIN(er->end, sounddata->nr_frames));
  }
}

/* returns new sounddata */
static sw_sample *
splice_in_eb_data (sw_sample * sample, sw_edit_buffer * eb)
{
  sw_sounddata * sounddata = sample->sounddata;
  sw_format * f = sounddata->format;
  gint length;
  GList * gl;
  sw_edit_region * er;
  sw_framecount_t prev_start = 0;
  sw_framecount_t er_width;
  gpointer di, d;
  sw_framecount_t len;

  if (!eb) {
    return sample;
  }

  g_mutex_lock (&sample->ops_mutex);

  /* in reverse ... decrementing d, di; just like paste at only crunchy */

  length = sounddata->nr_frames + edit_buffer_length (eb);

  d = g_realloc (sounddata->data, frames_to_bytes (f, length));
  sounddata->data = d;

  /* set di to point to the end of the original data */
  di = d + frames_to_bytes (f, sounddata->nr_frames);

  /* set d to point to end of newly realloc'd buffer */
  d += frames_to_bytes (f, length);

  prev_start = length;

  gl = g_list_last (eb->regions);
  er = (sw_edit_region *)gl->data;
  if (er->start >= sounddata->nr_frames) {
    len = frames_to_bytes (f, er->end - er->start);
    d -= len;
    memcpy (d, er->data, len);
    prev_start = er->start;
    gl = gl->prev;
  }

  for (; gl; gl = gl->prev) {
    er = (sw_edit_region *)gl->data;

    /* Move the sample data in */
    len = frames_to_bytes (f, prev_start - er->end);
    di -= len;
    d -= len;
    memmove (d, di, len);

    /* Copy edit_region data in */
    len = frames_to_bytes (f, er->end - er->start);
    d -= len;
    memcpy (d, er->data, len);
    prev_start = er->start;
  }

  /* The head of the sounddata remains intact */

  /* Move offset markers */

  for (gl = eb->regions; gl; gl = gl->next) {
    er = (sw_edit_region *)gl->data;

    er_width = er->end - er->start;

    if (sample->user_offset > er->end)
      sample->user_offset += er_width;

    head_inc_if_gt (sample->play_head, er->end, er_width);
    head_inc_if_gt (sample->rec_head, er->end, er_width);
  }

  sounddata->nr_frames = length;

  g_mutex_unlock (&sample->ops_mutex);

  return sample;
}

/* returns new sounddata */
sw_sample *
splice_in_eb (sw_sample * sample, sw_edit_buffer * eb)
{
  splice_in_eb_data (sample, eb);
  sounddata_set_sel_from_eb (sample->sounddata, eb);

  return sample;
}

/* modifies sounddata */
static sw_sounddata *
crop_in_eb_data (sw_sounddata * sounddata, sw_edit_buffer * eb)
{
  sw_format * f = sounddata->format;
  sw_framecount_t length, byte_length, o_byte_length;
  GList * gl;
  sw_edit_region * er1, * er2, * er;
  sw_framecount_t len1 = 0, len2 = 0;
  sw_framecount_t byte_len1, byte_len2;
  gpointer d;

  if (!eb) {
    return sounddata;
  }

  o_byte_length = frames_to_bytes (f, sounddata->nr_frames);

  gl = eb->regions;
  er1 = (sw_edit_region *)gl->data;
  if (er1->start == 0) {
    len1 = er1->end;
  }
  byte_len1 = frames_to_bytes (f, len1);

  gl = g_list_last (eb->regions);
  er2 = (sw_edit_region *)gl->data;
  if (er2->end > sounddata->nr_frames) {
    len2 = er2->end - er2->start;
  }
  byte_len2 = frames_to_bytes (f, len2);

  if (len1 + len2 > 0) {
    length = sounddata->nr_frames + len1 + len2;
    byte_length = frames_to_bytes (f, length);

    d = g_realloc (sounddata->data, byte_length);
    sounddata->data = d;
    sounddata->nr_frames = length;

    if (len1 > 0) {
      /* Move middle region in place */
      memmove ((gpointer)(d + byte_len1), d, o_byte_length);

      /* Copy (prepend) first region */
      memcpy (d, er1->data, byte_len1);
    } else {
      /* Overwrite first region in place */
      gl = eb->regions;
      er = (sw_edit_region *)gl->data;
      memcpy ((gpointer)(d + frames_to_bytes (f, er->start)),
	      er->data, frames_to_bytes (f, er->end - er->start));
    }

    /* Overwrite with in-between regions */
    gl = eb->regions;
    for (gl = gl->next; gl && gl->next; gl = gl->next) {
      er = (sw_edit_region *)gl->data;
      memcpy ((gpointer)(d + frames_to_bytes (f, er->start)),
	      er->data, frames_to_bytes (f, er->end - er->start));
    }

    if (len2 > 0) {
      /* Copy (append) last_region */
      memcpy ((gpointer)(d + o_byte_length + byte_len1), er2->data, byte_len2);
    } else if (gl) {
      /* Overwrite last region in place */
      er = (sw_edit_region *)gl->data;
      memcpy ((gpointer)(d + frames_to_bytes (f, er->start)),
	      er->data, frames_to_bytes (f, er->end - er->start));
    }

  } else {
    d = sounddata->data;
    for (gl = eb->regions; gl; gl = gl->next) {
      er = (sw_edit_region *)gl->data;
      memcpy ((gpointer)(d + frames_to_bytes (f, er->start)),
	      er->data, frames_to_bytes (f, er->end - er->start));
    }
  }

  return sounddata;
}

sw_sample *
crop_in (sw_sample * sample, sw_edit_buffer * eb)
{
  sw_sounddata * sounddata;
  GList * gl;
  sw_edit_region * er;
  sw_framecount_t delta;

  crop_in_eb_data (sample->sounddata, eb);
  sounddata = sample->sounddata;

  gl = eb->regions;
  er = (sw_edit_region *)gl->data;
  if (er->start == 0) {
    delta = er->end;

    g_mutex_lock (&sounddata->sels_mutex);
    sounddata_selection_translate (sounddata, delta);
    g_mutex_unlock (&sounddata->sels_mutex);

    g_mutex_lock (&sample->play_mutex);
    sample->user_offset += delta;
    g_mutex_unlock (&sample->play_mutex);

    g_mutex_lock (&sample->play_head->head_mutex);
    sample->play_head->offset += delta;
    sample->play_head->stop_offset += delta;
    g_mutex_unlock (&sample->play_head->head_mutex);

    if (sample->rec_head) {
      g_mutex_lock (&sample->rec_head->head_mutex);
      sample->rec_head->offset += delta;
      sample->rec_head->stop_offset += delta;
      g_mutex_unlock (&sample->rec_head->head_mutex);
    }
  }

  return sample;
}

/* Modifies sounddata */
static void
edit_clear_sel (sw_sample * sample)
{
  sw_sounddata * sounddata = sample->sounddata;
  sw_format * f = sounddata->format;
  GList * gl;
  sw_sel * sel;
  sw_framecount_t offset, len;
  sw_framecount_t sel_total, run_total;

  gboolean active = TRUE;

  sel_total = sounddata_selection_nr_frames (sounddata) / 100;
  if (sel_total == 0) sel_total = 1;
  run_total = 0;

  for (gl = sounddata->sels; active && gl; gl = gl->next) {
    g_mutex_lock (&sample->ops_mutex);

    if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
      active = FALSE;
    } else {
      sel = (sw_sel *)gl->data;

      offset = frames_to_bytes (f, sel->sel_start);
      len = frames_to_bytes (f, sel->sel_end - sel->sel_start);

      memset ((gpointer)(sounddata->data + offset), 0, (size_t)len);

      run_total += sel->sel_end - sel->sel_start;
      sample_set_progress_percent (sample, run_total / sel_total);
    }

    g_mutex_unlock (&sample->ops_mutex);
  }
}

/* modifies sounddata */
sw_sample *
crop_out (sw_sample * sample)
{
  sw_sounddata * sounddata = sample->sounddata;
  sw_format * f = sounddata->format;
  sw_framecount_t length;
  GList * gl;
  sw_sel * sel1, * sel2, * osel, * sel;
  /*sw_sounddata * out;*/
  gpointer d;
  sw_framecount_t offset, len;
  sw_framecount_t byte_length;

  if (!sounddata->sels) {
    return sample;
  }


#ifdef DEBUG
  printf("Splice out: remaining length %d\n", length);
#endif


  /* XXX: Force crops to be atomic wrt. to cancellation. For
   * multi-region selections it would be nicer to build the redo data
   * incrementally, but wtf.
   * Each region's memmove needs to be treated as atomic anyway so the
   * gain isn't so much.
   */
  g_mutex_lock (&sample->ops_mutex);

  gl = sounddata->sels;
  sel1 = (sw_sel *)gl->data;

  gl = g_list_last (sounddata->sels);
  sel2 = (sw_sel *)gl->data;

  if (sel1->sel_start <= 0 && sel2->sel_end >= sounddata->nr_frames) {
    d = sounddata->data;
    goto zero_out;
  }

  sample_set_progress_percent (sample, 13);

  /* ok, we have something to crop */

  length = sounddata_selection_width (sounddata);
  byte_length = frames_to_bytes (f, length);
  d = sounddata->data;

  if (sel1->sel_start > 0) {
    /* Need to move */
    offset = frames_to_bytes (f, sel1->sel_start);
    g_memmove (d, (gpointer)(d + offset), byte_length);
  }

  sample_set_progress_percent (sample, 37);

  /* Need to shorten */
  d = g_realloc (sounddata->data, byte_length);
  sounddata->data = d;
  sounddata->nr_frames = length;

  /* Fix offsets */
  sample->user_offset -= sel1->sel_start;
  sample->user_offset = CLAMP(sample->user_offset, 0, length);

  g_mutex_lock (&sample->play_head->head_mutex);

  sample->play_head->offset -= sel1->sel_start;
  sample->play_head->offset =
    CLAMP(sample->play_head->offset, 0, length);

  sample->play_head->stop_offset -= sel1->sel_start;
  sample->play_head->stop_offset =
    CLAMP(sample->play_head->stop_offset, 0, length);

  g_mutex_unlock (&sample->play_head->head_mutex);

  if (sample->rec_head) {
    g_mutex_lock (&sample->rec_head->head_mutex);

    sample->rec_head->offset -= sel1->sel_start;
    sample->rec_head->offset =
      CLAMP(sample->rec_head->offset, 0, length);

    sample->rec_head->stop_offset -= sel1->sel_start;
    sample->rec_head->stop_offset =
      CLAMP(sample->rec_head->stop_offset, 0, length);

    g_mutex_unlock (&sample->rec_head->head_mutex);
  }

  /* Fix selection */
  sounddata_selection_translate (sounddata, -(sel1->sel_start));

 zero_out:
  /* Zero out data between selections */
  gl = sounddata->sels;
  osel = (sw_sel *)gl->data;

  for (gl = gl->next; gl; gl = gl->next) {
    sel = (sw_sel *)gl->data;

    offset = frames_to_bytes (f, osel->sel_end);
    len = frames_to_bytes (f, sel->sel_start) - offset;
    memset ((gpointer)(d + offset), 0, len);

    osel = sel;
  }

  sample_set_progress_percent (sample, 100);

  g_mutex_unlock (&sample->ops_mutex);

  return sample;
}

/* modifies sounddata */
static sw_sample *
paste_insert (sw_sample * sample, sw_edit_buffer * eb,
	      sw_framecount_t paste_offset)
{
  sw_sounddata * sounddata = sample->sounddata;
  sw_format * f = sounddata->format;
  sw_framecount_t paste_delta = 0, len;
  sw_framecount_t length, paste_length, sel_length = 0;
  GList * gl;
  sw_edit_region * er;
  gpointer d;

  paste_delta = frames_to_bytes (f, paste_offset);

  paste_length = edit_buffer_length (eb);
  length = MAX(sounddata->nr_frames, paste_offset) + paste_length;

  d = g_realloc (sounddata->data, frames_to_bytes (f, length));
  sounddata->data = d;

  d = (gpointer)(sounddata->data + frames_to_bytes(f, length));

  /* Copy in the tail end of the sounddata */
  if (paste_offset < sounddata->nr_frames) {
    len = frames_to_bytes (f, sounddata->nr_frames) - paste_delta;
    d -= len;
    memmove (d, (gpointer)(sounddata->data + paste_delta), len);
  }

  /* Copy in the contents of the edit buffer */
  for (gl = g_list_last(eb->regions); gl; gl = gl->prev) {
    er = (sw_edit_region *)gl->data;

    len = frames_to_bytes (f, er->end - er->start);
    sel_length += (er->end - er->start);

    d -= len;
    memcpy (d, er->data, len);
  }

  /* If paste point is beyond the previous sounddata length,
     add some silence */
  if (paste_offset > sounddata->nr_frames) {
    len = paste_delta - frames_to_bytes (f, sounddata->nr_frames);
    d -= len;
    memset (d, 0, len);
  }

  /* The head of the sounddata remains intact */

  /* Select the copied in portion of the sounddata */
  sounddata_set_selection_1 (sounddata, paste_offset,
			     paste_offset + paste_length);

  /* Move offset markers */

  if (sample->user_offset > paste_offset)
    sample->user_offset += sel_length;

  if (sample->play_head->offset > paste_offset)
    sample->play_head->offset += sel_length;
  if (sample->play_head->stop_offset > paste_offset)
    sample->play_head->stop_offset += sel_length;

  if (sample->rec_head) {
    if (sample->rec_head->offset > paste_offset)
      sample->rec_head->offset += sel_length;
    if (sample->rec_head->stop_offset > paste_offset)
      sample->rec_head->stop_offset += sel_length;
  }

  sounddata->nr_frames = length;

  return sample;
}

/* Modifies sample */
sw_sample *
paste_over (sw_sample * sample, sw_edit_buffer * eb)
{
  sw_format * f = sample->sounddata->format;
  sw_framecount_t offset, len;
  sw_framecount_t length;
  GList * gl;
  sw_edit_region * er;

  if (!eb) return sample;

  length = sample->sounddata->nr_frames;

  for (gl = eb->regions; gl; gl = gl->next) {
    er = (sw_edit_region *)gl->data;

    if (er->start > length) break;

    offset = frames_to_bytes (f, er->start);
    len = frames_to_bytes (f, MIN(er->end, length) - er->start);
    memcpy ((gpointer)(sample->sounddata->data + offset), er->data, len);
  }

  return sample;
}

/* Modifies sample */
static sw_sample *
paste_mix (sw_sample * sample, sw_edit_buffer * eb,
	   sw_framecount_t paste_offset, gdouble src_gain, gdouble dest_gain)
{
  sw_format * f = sample->sounddata->format;
  sw_framecount_t length, eb_delta;
  GList * gl;
  sw_edit_region * er;
  float * d, * e;
  sw_framecount_t offset, remaining, n, i;
  sw_framecount_t run_total, eb_total;
  gint percent;

  gboolean active = TRUE;

#ifdef DEBUG
  g_print ("paste_mix: src_gain %f, dest_gain %f\n", src_gain, dest_gain);
#endif

  if (eb == NULL || eb->regions == NULL) return sample;

  length = sample->sounddata->nr_frames;

  run_total = 0;
  eb_total = edit_buffer_length (eb);

  eb_delta = ((sw_edit_region *)eb->regions->data)->start;

  for (gl = eb->regions; active && gl; gl = gl->next) {
    er = (sw_edit_region *)gl->data;

    if (er->start > length) break;

    d = sample->sounddata->data +
      frames_to_bytes (f, er->start - eb_delta + paste_offset);
    e = er->data;

    offset = 0;
    remaining = MIN(er->end, length) - er->start;

    while (active && remaining > 0) {
      g_mutex_lock (&sample->ops_mutex);

      if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
	active = FALSE;
      } else {

	n = MIN(remaining, 1024);

	for (i = 0; i < n * f->channels; i++) {
	  d[i] = d[i] * dest_gain + e[i] * src_gain;
	}

	remaining -= n;
	offset += n;

	d += n * f->channels;
	e += n * f->channels;

	run_total += n;
	percent = run_total / (eb_total/100);
	sample_set_progress_percent (sample, percent);

#ifdef DEBUG
	g_print ("completed %d / %d frames, %d%%\n", run_total, eb_total,
		 percent);
#endif
      }

      g_mutex_unlock (&sample->ops_mutex);
    }

  }

  return sample;
}

/* Modifies sample */
static sw_sample *
paste_xfade (sw_sample * sample, sw_edit_buffer * eb,
	     sw_framecount_t paste_offset,
	     gdouble src_gain_start, gdouble src_gain_end,
	     gdouble dest_gain_start, gdouble dest_gain_end)
{
  sw_format * f = sample->sounddata->format;
  sw_framecount_t length, eb_delta;
  GList * gl;
  sw_edit_region * er;
  float * d, * e;
  sw_framecount_t offset, remaining, n, i, j, k;
  sw_framecount_t run_total, eb_total;
  gint percent;

  gdouble src_gain, dest_gain;
  gdouble src_gain_delta, dest_gain_delta;

  gboolean active = TRUE;

#ifdef DEBUG
  g_print ("paste_xfade: src: %f -> %f\tdest: %f -> %f\n",
	   src_gain_start, src_gain_end, dest_gain_start, dest_gain_end);
#endif

  if (eb == NULL || eb->regions == NULL) return sample;

  length = sample->sounddata->nr_frames;

  run_total = 0;
  eb_total = edit_buffer_length (eb);

  eb_delta = ((sw_edit_region *)eb->regions->data)->start;

  src_gain = src_gain_start;
  dest_gain = dest_gain_start;

  src_gain_delta = (src_gain_end - src_gain_start) / (gdouble)eb_total;
  dest_gain_delta = (dest_gain_end - dest_gain_start) / (gdouble)eb_total;

  for (gl = eb->regions; active && gl; gl = gl->next) {
    er = (sw_edit_region *)gl->data;

    if (er->start > length) break;

    d = sample->sounddata->data +
      frames_to_bytes (f, er->start - eb_delta + paste_offset);
    e = er->data;

    offset = 0;
    remaining = MIN(er->end, length) - er->start;

    while (active && remaining > 0) {
      g_mutex_lock (&sample->ops_mutex);

      if (sample->edit_state == SWEEP_EDIT_STATE_CANCEL) {
	active = FALSE;
      } else {

	n = MIN(remaining, 1024);

	k = 0;
	for (i = 0; i < n; i++) {
	  for (j = 0; j < f->channels; j++) {
	    d[k] = d[k] * dest_gain + e[k] * src_gain;
	    k++;
	  }
	  src_gain += src_gain_delta;
	  dest_gain += dest_gain_delta;
	}

	remaining -= n;
	offset += n;

	d += n * f->channels;
	e += n * f->channels;

	run_total += n;
	percent = run_total / (eb_total/100);
	sample_set_progress_percent (sample, percent);

#ifdef DEBUG
	g_print ("completed %d / %d frames, %d%%\n", run_total, eb_total,
		 percent);
#endif
      }

      g_mutex_unlock (&sample->ops_mutex);
    }

  }

  return sample;
}

static void
do_copy_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;

  if (sample == NULL || sample->sounddata == NULL ||
      sample->sounddata->sels == NULL) goto noop;

  ebuf_clear ();

  ebuf = edit_buffer_from_sample (sample);

  return;

 noop:
  sample_set_tmp_message (sample, _("No selection to copy"));
}

/* No undo for copy -- is input only */
static sw_operation copy_op = {
  SWEEP_EDIT_MODE_FILTER,
  (SweepCallback)do_copy_thread,
  (SweepFunction)NULL,
  (SweepCallback)NULL,
  (SweepFunction)NULL,
  (SweepCallback)NULL,
  (SweepFunction)NULL,
};

void
do_copy (sw_sample * sample)
{
  schedule_operation (sample, _("Copy"), &copy_op, NULL);
}


static void
do_cut_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  sw_edit_buffer * eb;

  if (sample == NULL || sample->sounddata == NULL ||
      sample->sounddata->sels == NULL) goto noop;

  /*  inst = sw_op_instance_new ("Cut", &cut_op);*/

  eb = edit_buffer_from_sample (sample);

  inst->redo_data = inst->undo_data = splice_data_new (eb, NULL);
  set_active_op (sample, inst);

  ebuf_clear ();

  /*ebuf = edit_buffer_copy (eb);*/
  ebuf = edit_buffer_ref (eb);

  splice_out_sel (sample);

  if (sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    register_operation (sample, inst);
  }

  return;

 noop:
  sample_set_tmp_message (sample, _("No selection to cut"));
}

static sw_operation cut_op = {
  SWEEP_EDIT_MODE_ALLOC,
  (SweepCallback)do_cut_thread,
  (SweepFunction)NULL,
  (SweepCallback)undo_by_splice_in,
  (SweepFunction)splice_data_destroy,
  (SweepCallback)redo_by_splice_out,
  (SweepFunction)splice_data_destroy
};

void
do_cut (sw_sample * sample)
{
  schedule_operation (sample, _("Cut"), &cut_op, NULL);
}

static void
do_clear_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  sw_edit_buffer * old_eb;
  paste_over_data * p;

  if (sample == NULL || sample->sounddata == NULL ||
      sample->sounddata->sels == NULL) goto noop;

  old_eb = edit_buffer_from_sample (sample);

  p = paste_over_data_new (old_eb, old_eb);
  inst->redo_data = inst->undo_data = p;
  set_active_op (sample, inst);

  edit_clear_sel (sample);

  if (sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    p->new_eb = edit_buffer_from_sample (sample);

    register_operation (sample, inst);
  }

  return;

 noop:
  sample_set_tmp_message (sample, _("No selection to clear"));
}

static sw_operation clear_op = {
  SWEEP_EDIT_MODE_FILTER,
  (SweepCallback)do_clear_thread,
  (SweepFunction)NULL,
  (SweepCallback)undo_by_paste_over,
  (SweepFunction)paste_over_data_destroy,
  (SweepCallback)redo_by_paste_over /* redo_by_reperform */,
  (SweepFunction)paste_over_data_destroy
};

void
do_clear (sw_sample * sample)
{
  schedule_operation (sample, _("Clear"), &clear_op, NULL);
}

static void
do_delete_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  sw_edit_buffer * eb;

  if (sample == NULL || sample->sounddata == NULL ||
      sample->sounddata->sels == NULL) goto noop;

  eb = edit_buffer_from_sample (sample);

  inst->redo_data = inst->undo_data = splice_data_new (eb, NULL);
  set_active_op (sample, inst);

  splice_out_sel (sample);

  if (sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    register_operation (sample, inst);
  }

  return;

 noop:
  sample_set_tmp_message (sample, _("No selection to delete"));
}

static sw_operation delete_op = {
  SWEEP_EDIT_MODE_ALLOC,
  (SweepCallback)do_delete_thread,
  (SweepFunction)NULL,
  (SweepCallback)undo_by_splice_in,
  (SweepFunction)splice_data_destroy,
  (SweepCallback)redo_by_splice_out,
  (SweepFunction)splice_data_destroy
};


void
do_delete (sw_sample * sample)
{
  schedule_operation (sample, _("Delete"), &delete_op, NULL);
}

static void
do_crop_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  GList * sels;
  sw_edit_buffer * eb;

  if (sample == NULL || sample->sounddata == NULL ||
      sample->sounddata->sels == NULL) goto noop;

  sels = sels_copy (sample->sounddata->sels);
  sels = sels_invert (sels, sample->sounddata->nr_frames);

  if (sels == NULL) goto noop;

  eb = edit_buffer_from_sounddata_sels (sample->sounddata, sels);

  /* If there's nothing to crop out, do nothing and register nothing */
  if (eb == NULL || eb->regions == NULL) goto noop;

  inst->redo_data = inst->undo_data = splice_data_new (eb, NULL);
  set_active_op (sample, inst);

  crop_out (sample);

  if (sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    register_operation (sample, inst);
  }

  return;

 noop:
  sample_set_tmp_message (sample, _("Nothing to crop out"));
}

static sw_operation crop_op = {
  SWEEP_EDIT_MODE_ALLOC,
  (SweepCallback)do_crop_thread,
  (SweepFunction)NULL,
  (SweepCallback)undo_by_crop_in,
  (SweepFunction)splice_data_destroy,
  (SweepCallback)redo_by_crop_out,
  (SweepFunction)splice_data_destroy
};

void
do_crop (sw_sample * sample)
{
  schedule_operation (sample, _("Crop"), &crop_op, NULL);
}

static void
do_paste_insert_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  sw_sounddata * out;
  sw_edit_buffer * eb1, *eb2;

  if (ebuf == NULL) goto noop;

  if (!format_equal (ebuf->format, sample->sounddata->format))
    goto incompatible;

  /*eb1 = edit_buffer_copy (ebuf);*/
  eb1 = edit_buffer_ref (ebuf);

  inst->undo_data = splice_data_new (eb1, sample->sounddata->sels);
  inst->redo_data = NULL;
  set_active_op (sample, inst);

  paste_insert (sample, ebuf, sample->user_offset);

  if (sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    out = sample->sounddata;
    eb2 = edit_buffer_from_sounddata (out);
    inst->redo_data = splice_data_new (eb2, NULL);

    register_operation (sample, inst);
  }

  return;

 noop:
  sample_set_tmp_message (sample, _("Clipboard empty"));
  return;

 incompatible:
  sample_set_tmp_message (sample, _("Clipboard data has incompatible format"));
  return;
}

static sw_operation paste_insert_op = {
  SWEEP_EDIT_MODE_ALLOC,
  (SweepCallback)do_paste_insert_thread,
  (SweepFunction)NULL,
  (SweepCallback)undo_by_splice_out,
  (SweepFunction)splice_data_destroy,
  (SweepCallback)redo_by_splice_in,
  (SweepFunction)splice_data_destroy
};


void
do_paste_insert (sw_sample * sample)
{
  schedule_operation (sample, _("Paste insert"), &paste_insert_op, NULL);
}

typedef struct {
  gdouble src_gain;
  gdouble dest_gain;
} paste_mix_op_data;

static void
do_paste_mix_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  paste_mix_op_data * pd = (paste_mix_op_data *)inst->do_data;
  sw_sounddata * out;
  GList * old_sels, * sels;
  sw_edit_buffer * eb1, *eb2;
  gdouble src_gain, dest_gain;

  src_gain = pd->src_gain;
  dest_gain = pd->dest_gain;
  g_free (pd);

  if (ebuf == NULL) goto noop;

  if (!format_equal (ebuf->format, sample->sounddata->format))
    goto incompatible;

  old_sels = sels_copy (sample->sounddata->sels);
  sels = sels_from_edit_buffer_offset (ebuf, sample->user_offset);
  sample_set_selection (sample, sels);

  eb1 = edit_buffer_from_sounddata (sample->sounddata);

  inst->undo_data = splice_data_new (eb1, old_sels);
  inst->redo_data = NULL;
  set_active_op (sample, inst);

  paste_mix (sample, ebuf, sample->user_offset, src_gain, dest_gain);

  if (sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    out = sample->sounddata;
    eb2 = edit_buffer_from_sounddata (out);
    inst->redo_data = splice_data_new (eb2, sels);

    register_operation (sample, inst);
  } else {
    sample_set_selection (sample, old_sels);
  }

  return;

 noop:
  sample_set_tmp_message (sample, _("Clipboard empty"));
  return;

 incompatible:
  sample_set_tmp_message (sample, _("Clipboard data has incompatible format"));
  return;
}

static sw_operation paste_mix_op = {
  SWEEP_EDIT_MODE_FILTER,
  (SweepCallback)do_paste_mix_thread,
  (SweepFunction)NULL,
  (SweepCallback)undo_by_splice_over,
  (SweepFunction)splice_data_destroy,
  (SweepCallback)redo_by_splice_over,
  (SweepFunction)splice_data_destroy
};

void
do_paste_mix (sw_sample * sample, gdouble src_gain, gdouble dest_gain)
{
  paste_mix_op_data * pd;

  pd = g_malloc (sizeof(paste_mix_op_data));
  pd->src_gain = src_gain;
  pd->dest_gain = dest_gain;

  schedule_operation (sample, _("Paste mix"), &paste_mix_op, pd);
}

typedef struct {
  gdouble src_gain_start;
  gdouble src_gain_end;
  gdouble dest_gain_start;
  gdouble dest_gain_end;
} paste_xfade_op_data;

static void
do_paste_xfade_thread (sw_op_instance * inst)
{
  sw_sample * sample = inst->sample;
  paste_xfade_op_data * pd = (paste_xfade_op_data *)inst->do_data;
  sw_sounddata * out;
  GList * old_sels, * sels;
  sw_edit_buffer * eb1, *eb2;
  gdouble src_gain_start, src_gain_end, dest_gain_start, dest_gain_end;

  src_gain_start = pd->src_gain_start;
  src_gain_end = pd->src_gain_end;
  dest_gain_start = pd->dest_gain_start;
  dest_gain_end = pd->dest_gain_end;
  g_free (pd);

  if (ebuf == NULL) goto noop;

  if (!format_equal (ebuf->format, sample->sounddata->format))
    goto incompatible;

  old_sels = sels_copy (sample->sounddata->sels);
  sels = sels_from_edit_buffer_offset (ebuf, sample->user_offset);
  sample_set_selection (sample, sels);

  eb1 = edit_buffer_from_sounddata (sample->sounddata);

  inst->undo_data = splice_data_new (eb1, old_sels);
  inst->redo_data = NULL;
  set_active_op (sample, inst);

  paste_xfade (sample, ebuf, sample->user_offset, src_gain_start,
	       src_gain_end, dest_gain_start, dest_gain_end);

  if (sample->edit_state == SWEEP_EDIT_STATE_BUSY) {
    out = sample->sounddata;
    eb2 = edit_buffer_from_sounddata (out);
    inst->redo_data = splice_data_new (eb2, sels);

    register_operation (sample, inst);
  } else {
    sample_set_selection (sample, old_sels);
  }

  return;

 noop:
  sample_set_tmp_message (sample, _("Clipboard empty"));
  return;

 incompatible:
  sample_set_tmp_message (sample, _("Clipboard data has incompatible format"));
  return;
}

static sw_operation paste_xfade_op = {
  SWEEP_EDIT_MODE_FILTER,
  (SweepCallback)do_paste_xfade_thread,
  (SweepFunction)NULL,
  (SweepCallback)undo_by_splice_over,
  (SweepFunction)splice_data_destroy,
  (SweepCallback)redo_by_splice_over,
  (SweepFunction)splice_data_destroy
};

void
do_paste_xfade (sw_sample * sample, gdouble src_gain_start,
		gdouble src_gain_end, gdouble dest_gain_start,
		gdouble dest_gain_end)
{
  paste_xfade_op_data * pd;

  pd = g_malloc (sizeof(paste_xfade_op_data));
  pd->src_gain_start = src_gain_start;
  pd->src_gain_end = src_gain_end;
  pd->dest_gain_start = dest_gain_start;
  pd->dest_gain_end = dest_gain_end;

  schedule_operation (sample, _("Paste xfade"), &paste_xfade_op, pd);
}

sw_sample *
do_paste_as_new (void)
{
  if (ebuf == NULL) return NULL;

  return sample_from_edit_buffer (ebuf);
}

