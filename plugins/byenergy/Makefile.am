## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)/include \
		@GTK_CFLAGS@ @GLIB_CFLAGS@ \
		@GTHREADS_CFLAGS@

libdir = $(PACKAGE_PLUGIN_DIR)

lib_LTLIBRARIES = \
	libbyenergy.la

libbyenergy_la_SOURCES = byenergy.c
libbyenergy_la_LDFLAGS = -avoid-version -module \
			@GTK_LIBS@ @GLIB_LIBS@ \
			@GTHREADS_LIBS@
