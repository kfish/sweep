## Process this file with automake to produce Makefile.in

SUBDIRS = tdb

INCLUDES = \
	-I$(top_srcdir)/intl \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/src/tdb \
	@SNDFILE_CFLAGS@ \
	@OGG_CFLAGS@ @VORBIS_CFLAGS@ \
	@SAMPLERATE_CFLAGS@ \
	@GTK_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@GTHREADS_CFLAGS@ \
	@ALSA_CFLAGS@

TDB_LIBS = $(top_builddir)/src/tdb/libtdb.a

bin_PROGRAMS = sweep

sweep_SOURCES = \
	sweep_app.h sweep_compat.h\
	main.c \
	about_dialog.c about_dialog.h \
	callbacks.c callbacks.h \
	channelops.c channelops.h \
	cursors.c cursors.h \
	db_ruler.c db_ruler.h \
	db_slider.c db_slider.h \
	driver.c driver.h \
	driver_alsa.c \
	driver_oss.c \
	driver_solaris.c \
	edit.c edit.h \
	file_dialogs.c file_dialogs.h \
	file_sndfile.h \
	file_sndfile1.c \
	file_mad.c \
	file_speex.c \
	file_vorbis.c \
	format.c format.h \
	head.c head.h \
	interface.c interface.h \
	levelmeter.c levelmeter.h \
	notes.c notes.h \
	param.c param.h \
	paste_dialogs.c paste_dialogs.h \
	pcmio.h \
	pixmaps.h \
	play.c play.h \
	plugin.c plugin.h \
	preferences.c preferences.h \
	print.c print.h \
	question_dialogs.c question_dialogs.h \
	record.c record.h \
	sample-display.c sample-display.h \
	samplerate.c \
	sw_chooser.c sw_chooser.h \
	sweep_filter.c \
	sweep_sample.c sample.h \
	sweep_sounddata.c \
	sweep_selection.c \
	sweep_typeconvert.c \
	sweep_undo.c \
	time_ruler.c time_ruler.h \
	timeouts.c \
	undo_dialog.c undo_dialog.h \
	view.c view.h \
	view_pixmaps.h

sweep_LDADD = $(TDB_LIBS) \
	@GTK_LIBS@ $(INTLLIBS) $(PTHREAD_LIBS) \
	$(SNDFILE_LIBS) \
	$(OGG_LIBS) $(VORBIS_LIBS) $(VORBISFILE_LIBS) $(VORBISENC_LIBS) \
	$(MAD_LIBS) $(SPEEX_LIBS) \
	$(SAMPLERATE_LIBS) \
	@GLIB_LIBS@ @GTHREADS_LIBS@ \
	@ALSA_LIBS@

sweep_LDFLAGS = @EXPORT_DYNAMIC_FLAGS@

sweep_CFLAGS = -DPACKAGE_DATA_DIR='@PACKAGE_DATA_DIR@'
