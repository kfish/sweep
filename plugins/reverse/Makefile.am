## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	-I$(top_srcdir)/src \
	@GTK_CFLAGS@ \
	@AUDIOFILE_CFLAGS@

libdir = @PACKAGE_PLUGIN_DIR@

lib_LTLIBRARIES = \
	libreverse.la

libreverse_la_SOURCES = reverse.c
libreverse_la_LDFLAGS = -version-info 1:0:0
