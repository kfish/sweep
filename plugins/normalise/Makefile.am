## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	-I$(top_srcdir)/src \
	@GTK_CFLAGS@ \
	@AUDIOFILE_CFLAGS@

libdir = @PACKAGE_PLUGIN_DIR@

lib_LTLIBRARIES = \
	libnormalise.la

libnormalise_la_SOURCES = normalise.c

