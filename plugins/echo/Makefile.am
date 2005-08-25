## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)/include \
		 @GTK_CFLAGS@ @GLIB_CFLAGS@ \
                @GTHREADS_CFLAGS@

libdir = $(PACKAGE_PLUGIN_DIR)

lib_LTLIBRARIES = \
	libecho.la

libecho_la_SOURCES = echo.c
libecho_la_LDFLAGS = -avoid-version -module \
		 @GTK_LIBS@ @GLIB_LIBS@ \
                @GTHREADS_LIBS@

