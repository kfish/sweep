## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	-I$(top_srcdir)/src \
	@GTK_CFLAGS@ \
	@AUDIOFILE_CFLAGS@

libdir = @PACKAGE_PLUGIN_DIR@

lib_LTLIBRARIES = \
	libladspameta.la

libladspameta_la_SOURCES = ladspameta.c ladspa.h
libladspameta_la_LDFLAGS = -version-info 1:0:0
