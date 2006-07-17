## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)/include \
			 @GLIB_CFLAGS@ \
			 @GTK_CFLAGS@
libdir = $(PACKAGE_PLUGIN_DIR)

lib_LTLIBRARIES = \
	libladspameta.la

libladspameta_la_SOURCES = ladspameta.c ladspa.h
libladspameta_la_LDFLAGS = -avoid-version -module

install: all
	mkdir -p $(DESTDIR)/$(libdir)
	$(INSTALL_PROGRAM) .libs/libladspameta.so $(DESTDIR)/$(libdir);

uninstall:
	rm -f $(DESTDIR)/$(libdir)/libladspameta.so
