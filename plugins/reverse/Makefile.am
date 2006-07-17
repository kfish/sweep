## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)/include \
			 @GTK_CFLAGS@

libdir = $(PACKAGE_PLUGIN_DIR)

lib_LTLIBRARIES = \
	libreverse.la

libreverse_la_SOURCES = reverse.c
libreverse_la_LDFLAGS = -avoid-version -module

install: all
	mkdir -p $(DESTDIR)/$(libdir)
	$(INSTALL_PROGRAM) .libs/libreverse.so $(DESTDIR)/$(libdir);

uninstall:
	rm -f $(DESTDIR)/$(libdir)/libreverse.so
