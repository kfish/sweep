## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)/include \
			 @GTK_CFLAGS@
libdir = $(PACKAGE_PLUGIN_DIR)

lib_LTLIBRARIES = \
	libnormalise.la

libnormalise_la_SOURCES = normalise.c
libnormalise_la_LDFLAGS = -avoid-version -module

install: all
	mkdir -p $(DESTDIR)/$(libdir)
	$(INSTALL_PROGRAM) .libs/libnormalise.so $(DESTDIR)/$(libdir);

uninstall:
	rm -f $(DESTDIR)/$(libdir)/libnormalise.so
