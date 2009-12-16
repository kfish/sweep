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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <glib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#include <sweep/sweep_i18n.h>
#include "question_dialogs.h"

#include "tdb.h"

static TDB_CONTEXT * prefs_tdb = NULL;
gboolean ignore_failed_tdb_lock = FALSE;


#define DIR_MODE (S_IRWXU)
#define FILE_MODE (S_IRUSR | S_IWUSR)

void
prefs_init ()
{
  G_CONST_RETURN char * prefs_path;
  struct stat sbuf;
  
  prefs_path = g_get_home_dir ();
  prefs_path = g_strconcat (prefs_path, "/.sweep", NULL);

  if (stat (prefs_path, &sbuf) == -1) {
    switch (errno) {
    case ENOENT:
      if (mkdir (prefs_path, DIR_MODE) == -1) {
	if (errno != EEXIST) {
	  perror (_("Error creating ~/.sweep"));
	  exit (1);
	}
      } else {
	printf (_("Created %s/ mode %04o\n"), prefs_path, DIR_MODE);
      }
      break;
    case EACCES:
      /* Directory exists, permission denied -- handled at access test */
      break;
    default:
      perror (_("Error on ~/.sweep"));
      exit (1);
    }
  }

  if (access (prefs_path, W_OK) == -1) {
    switch (errno) {
    case EACCES:
      if (chmod (prefs_path, DIR_MODE) == -1) {
	perror (_("Error setting permissions on ~/.sweep"));
	exit (1);
      } else {
	printf ("Changed mode of %s to %04o\n", prefs_path, DIR_MODE);
      }
      break;
    default:
      perror (_("Error accessing ~/.sweep"));
      exit (1);
    }
  }

  prefs_path = g_strconcat (prefs_path, "/preferences.tdb", NULL);

  if (access (prefs_path, R_OK | W_OK) == -1) {
    switch (errno) {
    case EACCES:
      if (chmod (prefs_path, FILE_MODE) == -1) {
	perror ("Error setting permissions on ~/.sweep/preferences.tdb");
	exit (1);
      } else {
	printf ("Changed mode of %s to %04o\n", prefs_path, FILE_MODE);
      }
      break;
    default:
      break;
    }
  }

  prefs_tdb = tdb_open (prefs_path, 0, 0, O_RDWR | O_CREAT, FILE_MODE);

  if (prefs_tdb == NULL) {
	  
	if (ignore_failed_tdb_lock == TRUE)
	{
      prefs_tdb = tdb_open (prefs_path, 0, TDB_NOLOCK, O_RDWR | O_CREAT, FILE_MODE);
	  if (prefs_tdb != NULL) {
        fprintf(stderr,  "Warning: couldn't get lock to  ~/.sweep/preferences.tdb.\n"
           "         opened without locking\n");
		  return;
      } 
    }
	perror (_("Error opening ~/.sweep/preferences.tdb"));
    exit (1);
  }
}

int
prefs_close (void)
{
  if (prefs_tdb == NULL) return 0;

  return tdb_close (prefs_tdb);
}

int
prefs_delete (char * key)
{
  TDB_DATA key_data;

  if (prefs_tdb == NULL) return -1;

  key_data.dptr = key;
  key_data.dsize = strlen (key);

  return tdb_delete (prefs_tdb, key_data);
}

int *
prefs_get_int (char * key)
{
  TDB_DATA key_data, val_data;
  int nval, val;

  if (prefs_tdb == NULL) return NULL;

  key_data.dptr = key;
  key_data.dsize = strlen (key);

  val_data = tdb_fetch (prefs_tdb, key_data);

  if (val_data.dptr == NULL) {
    return NULL;
  }

  nval = * (int *)val_data.dptr;
  val = g_ntohl (nval);

  * (int *)val_data.dptr = val;

  return (int *)val_data.dptr;
}

int
prefs_set_int (char * key, int val)
{
  TDB_DATA key_data, val_data;
  int nval;

  if (prefs_tdb == NULL) return -1;

  key_data.dptr = key;
  key_data.dsize = strlen (key);

  nval = g_htonl (val);

  val_data.dptr = (char *)&nval;
  val_data.dsize = sizeof (int);

  return tdb_store (prefs_tdb, key_data, val_data, TDB_REPLACE);
}

long *
prefs_get_long (char * key)
{
  TDB_DATA key_data, val_data;
  long nval, val;

  if (prefs_tdb == NULL) return NULL;

  key_data.dptr = key;
  key_data.dsize = strlen (key);

  val_data = tdb_fetch (prefs_tdb, key_data);

  if (val_data.dptr == NULL) {
    return NULL;
  }

  nval = * (long *)val_data.dptr;
#if 0
  val = g_ntohl (nval);
#else
  val = nval;
#endif

  * (long *)val_data.dptr = val;

  return (long *)val_data.dptr;
}

int
prefs_set_long (char * key, long val)
{
  TDB_DATA key_data, val_data;
  long nval;

  if (prefs_tdb == NULL) return -1;

  key_data.dptr = key;
  key_data.dsize = strlen (key);

#if 0
  nval = g_htonl (val);
#else
  nval = val;
#endif

  val_data.dptr = (char *)&nval;
  val_data.dsize = sizeof (long);

  return tdb_store (prefs_tdb, key_data, val_data, TDB_REPLACE);
}

float *
prefs_get_float (char * key)
{
  TDB_DATA key_data, val_data;
  int nval, val;
  float fval;
  union { int ip; float fp;} fp;
  if (prefs_tdb == NULL) return NULL;

  key_data.dptr = key;
  key_data.dsize = strlen (key);

  val_data = tdb_fetch (prefs_tdb, key_data);

  if (val_data.dptr == NULL) {
    return NULL;
  }

  nval = *(int *)val_data.dptr;
  val = g_ntohl (nval);

  fp.ip = val;
  fval = fp.fp;

#ifdef DEBUG
  printf ("preferences.c: got %s is %f\n", key, fval);
#endif

  *(float *)val_data.dptr = fval;

  return (float *)val_data.dptr;
}

int
prefs_set_float (char * key, float val)
{
  TDB_DATA key_data, val_data;
  int nval, ival;
  union { int ip; float fp;} fp;

  if (prefs_tdb == NULL) return -1;

#ifdef DEBUG
  printf ("preferences.c: setting %s to %f\n", key, val);
#endif

  key_data.dptr = key;
  key_data.dsize = strlen (key);

  fp.fp = val;
  ival = fp.ip;
  nval = g_htonl (ival);

  val_data.dptr = (char *)&nval;
  val_data.dsize = sizeof (float);

  return tdb_store (prefs_tdb, key_data, val_data, TDB_REPLACE);
}

char *
prefs_get_string (char * key)
{
  TDB_DATA key_data, val_data;

  if (prefs_tdb == NULL) return NULL;

  key_data.dptr = key;
  key_data.dsize = strlen (key);

  val_data = tdb_fetch (prefs_tdb, key_data);

  return (char *)val_data.dptr;
}

int
prefs_set_string (char * key, char * val)
{
  TDB_DATA key_data, val_data;

  if (prefs_tdb == NULL) return -1;

  key_data.dptr = key;
  key_data.dsize = strlen (key);

  val_data.dptr = val;
  val_data.dsize = strlen (val) + 1;

  return tdb_store (prefs_tdb, key_data, val_data, TDB_REPLACE);
}
