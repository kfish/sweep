## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)/include

libdir = $(PACKAGE_PLUGIN_DIR)

lib_LTLIBRARIES = \
	libladspameta.la

libladspameta_la_SOURCES = ladspameta.c ladspa.h
libladspameta_la_LDFLAGS = -avoid-version -module
