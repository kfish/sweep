## Process this file with automake to produce Makefile.in

SUBDIRS = intl po include src plugins doc

pkgdata_DATA = sweep_logo.ppm

desktop_DATA = sweep.desktop
desktopdir = $(datadir)/gnome/apps/Multimedia

icon_DATA = sweep.png
icondir = $(datadir)/pixmaps

EXTRA_DIST = $(desktop_DATA) $(icon_DATA) $(pkgdata_DATA) \
             sweep.spec README.Solaris README.ALSA

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 configure config.h.in \
                       stamp-h.in \
                       config.guess mkinstalldirs \
                       missing install-sh config.sub ltconfig \
                       ltmain.sh acinclude.m4

install-data-local:
	@$(NORMAL_INSTALL)
	if test -d $(srcdir)/pixmaps; then \
	  $(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/pixmaps; \
	  for pixmap in $(srcdir)/pixmaps/*; do \
	    if test -f $$pixmap; then \
	      $(INSTALL_DATA) $$pixmap $(DESTDIR)$(pkgdatadir)/pixmaps; \
	    fi \
	  done \
	fi

dist-hook:
	if test -d pixmaps; then \
	  mkdir $(distdir)/pixmaps; \
	  for pixmap in pixmaps/*; do \
	    if test -f $$pixmap; then \
	      cp -p $$pixmap $(distdir)/pixmaps; \
	    fi \
	  done \
	fi

