## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	@GTK_CFLAGS@ \
	@AUDIOFILE_CFLAGS@

bin_PROGRAMS = sweep

sweep_SOURCES = \
	main.c sweep.h \
	about_dialog.c about_dialog.h \
	file_dialogs.c file_dialogs.h \
	file_ops.c file_ops.h \
	format.c format.h \
	interface.c interface.h \
	callbacks.c callbacks.h \
	cursors.c cursors.h \
	driver.c driver.h \
	edit.c edit.h \
	filter_ops.c filter_ops.h \
	notes.c notes.h \
	param.c param.h \
	plugin.c plugin.h \
	sample.c sample.h \
	sample-display.c sample-display.h \
	undo.c undo.h \
	view.c view.h \
	pixmaps.h \
	view_pixmaps.h \
	horiz.xpm horiz_plus.xpm

sweep_LDADD = @GTK_LIBS@ @AUDIOFILE_LIB@ $(INTLLIBS) $(PTHREAD_LIBS)

libdir = @PACKAGE_PLUGIN_DIR@

lib_LTLIBRARIES = \
	libnormalise.la \
	libreverse.la \
	libecho.la

libecho_la_SOURCES = echo.c echo.h
libnormalise_la_SOURCES = normalise.c normalise.h
libreverse_la_SOURCES = reverse.c reverse.h

