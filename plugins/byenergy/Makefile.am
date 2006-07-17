## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)/include \
			@GTK_CFLAGS@

libdir = $(PACKAGE_PLUGIN_DIR)

lib_LTLIBRARIES = \
	libbyenergy.la

libbyenergy_la_SOURCES = byenergy.c
libbyenergy_la_LDFLAGS = -avoid-version -module

install: all
	mkdir -p $(DESTDIR)/$(libdir)
	$(INSTALL_PROGRAM) .libs/libbyenergy.so $(DESTDIR)/$(libdir);

uninstall:
	rm -f $(DESTDIR)/$(libdir)/libbyenergy.so
