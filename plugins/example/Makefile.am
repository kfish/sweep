## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/include

libdir = $(PACKAGE_PLUGIN_DIR)

# Use lib_LTLIBRARIES to create a plugin that you actually want installed;
# Here, for the example plugin, we specify the noinst prefix instead of
# lib to stop it from actually being installed.
#
#lib_LTLIBRARIES = libexample.la
noinst_LTLIBRARIES = libexample.la

libexample_la_SOURCES = example.c
libexample_la_LIBADD = $(GTK_LIBS)
libexample_la_LDFLAGS = -avoid-version -module
