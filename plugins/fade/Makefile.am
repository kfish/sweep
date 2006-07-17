## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)/include \
 			 @GTK_CFLAGS@
libdir = $(PACKAGE_PLUGIN_DIR)

lib_LTLIBRARIES = \
	libfade.la

libfade_la_SOURCES = fade.c
libfade_la_LDFLAGS = -avoid-version -module 

install: all
	mkdir -p $(DESTDIR)/$(libdir)
	$(INSTALL_PROGRAM) .libs/libfade.so $(DESTDIR)/$(libdir);

uninstall:
	rm -f $(DESTDIR)/$(libdir)/libfade.so

