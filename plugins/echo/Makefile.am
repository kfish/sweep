## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)/include

libdir = $(PACKAGE_PLUGIN_DIR)

lib_LTLIBRARIES = \
	libecho.la

libecho_la_SOURCES = echo.c
libecho_la_LDFLAGS = -avoid-version -module
