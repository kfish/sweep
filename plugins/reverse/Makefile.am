## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)/include

libdir = $(PACKAGE_PLUGIN_DIR)

lib_LTLIBRARIES = \
	libreverse.la

libreverse_la_SOURCES = reverse.c
libreverse_la_LDFLAGS = -avoid-version -module
