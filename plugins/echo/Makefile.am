## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	-I$(top_srcdir)/src

libdir = @PACKAGE_PLUGIN_DIR@

lib_LTLIBRARIES = \
	libecho.la

libecho_la_SOURCES = echo.c
libecho_la_LDFLAGS = -version-info 1:0:0
