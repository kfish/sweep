## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	-I$(top_srcdir)/include \
	@GTK_CFLAGS@ \
	@AUDIOFILE_CFLAGS@

bin_PROGRAMS = sweep

sweep_SOURCES = \
	sweep_version.h sweep_app.h \
	main.c sweep_types.h \
	about_dialog.c about_dialog.h \
	file_dialogs.c file_dialogs.h \
	file_ops.c file_ops.h \
	format.c format.h \
	interface.c interface.h \
	callbacks.c callbacks.h \
	cursors.c cursors.h \
	driver.c driver.h \
	edit.c edit.h \
	notes.c notes.h \
	param.c param.h \
	plugin.c plugin.h \
	print.c print.h \
	sample-display.c sample-display.h \
	sweep_filter.c sweep_filter.h \
	sweep_sample.c sweep_sample.h sample.h \
	sweep_sounddata.c sweep_sounddata.h \
	sweep_selection.c sweep_selection.h \
	sweep_typeconvert.c sweep_typeconvert.h \
	sweep_undo.c sweep_undo.h \
	time_ruler.c time_ruler.h \
	view.c view.h \
	pixmaps.h \
	view_pixmaps.h \
	horiz.xpm horiz_plus.xpm

sweep_LDADD = @GTK_LIBS@ @AUDIOFILE_LIB@ $(INTLLIBS) $(PTHREAD_LIBS)

