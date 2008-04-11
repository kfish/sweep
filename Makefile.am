## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = m4 intl po include src plugins doc

pkgdata_DATA = sweep_splash.png

desktop_DATA = sweep.desktop
desktopdir = $(datadir)/applications

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = sweep.svg

EXTRA_DIST = config.rpath mkinstalldirs  $(desktop_DATA) $(pkgdata_DATA)  $(pixmap_DATA) \
             sweep.spec README.Solaris README.ALSA README.i18n TODO

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 configure config.h.in \
                       stamp-h.in \
                       config.guess mkinstalldirs \
                       missing install-sh config.sub ltconfig \
                       ltmain.sh acinclude.m4

#install-data-local:
#	@$(NORMAL_INSTALL)
#	if test -d $(srcdir)/pixmaps; then \
#	  $(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/pixmaps; \
#	  for pixmap in $(srcdir)/pixmaps/*; do \
#	    if test -f $$pixmap; then \
#	      $(INSTALL_DATA) $$pixmap $(DESTDIR)$(pkgdatadir)/pixmaps; \
#	    fi \
#	  done \
#	fi

dist-hook:
	if test -d pixmaps; then \
	  mkdir $(distdir)/pixmaps; \
	  for pixmap in pixmaps/*; do \
	    if test -f $$pixmap; then \
	      cp -p $$pixmap $(distdir)/pixmaps; \
	    fi \
	  done \
	fi
