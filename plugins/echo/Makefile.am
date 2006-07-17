## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)/include \
		 @GTK_CFLAGS@

libdir = $(PACKAGE_PLUGIN_DIR)

lib_LTLIBRARIES = \
	libecho.la

libecho_la_SOURCES = echo.c
libecho_la_LDFLAGS = -avoid-version -module

install: all
	mkdir -p $(DESTDIR)/$(libdir)
	$(INSTALL_PROGRAM) .libs/libecho.so $(DESTDIR)/$(libdir);

uninstall:
	rm -f $(DESTDIR)/$(libdir)/libecho.so
