dnl Process this file with autoconf to produce a configure script.

AC_INIT(configure.in)
AM_INIT_AUTOMAKE(sweep, 0.9.0)
AM_CONFIG_HEADER(config.h)

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
AM_PROG_LIBTOOL

AC_CHECK_PROG(HAVE_GNOME_MOZ_REMOTE, 'gnome-moz-remote', yes, no)

AC_C_BIGENDIAN

AC_CHECK_FUNCS(strchr)
AC_CHECK_FUNCS(madvise)

ALL_LINGUAS="de el es_ES fr hu it pl ru en_AU"
AM_GNU_GETTEXT

dnl Overall configuration success flag
sweep_config_ok=yes

dnl Default audio handling to build in
sweep_config_driver=none

dnl
dnl  Configuration option for building of experimental code.
dnl

ac_enable_experimental=no
AC_ARG_ENABLE(experimental,
     [  --enable-experimental   enable building of experimental code ],
     [ ac_enable_experimental=yes ])

if test "x${ac_enable_experimental}" = xyes ; then
    AC_DEFINE(DEVEL_CODE)
fi


dnl
dnl  Detect pthreads libraries
dnl

PTHREAD_LIBS=error
AC_CHECK_LIB(pthread, pthread_attr_init, PTHREAD_LIBS="-lpthread")
if test "x$PTHREAD_LIBS" = xerror; then
        AC_CHECK_LIB(pthreads, pthread_attr_init, PTHREAD_LIBS="-lpthreads")
fi
if test "x$PTHREAD_LIBS" = xerror; then
        AC_CHECK_LIB(c_r, pthread_attr_init, PTHREAD_LIBS="-lc_r")
fi
if test "x$PTHREAD_LIBS" = xerror; then
        AC_CHECK_FUNC(pthread_attr_init, PTHREAD_LIBS="")
fi
AC_SUBST(PTHREAD_LIBS)



dnl
dnl  Detect Gthread, GLib and GTK+ libraries
dnl

PKG_CHECK_MODULES(GTK, glib-2.0 >=  2.2.0
                       )
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

PKG_CHECK_MODULES(GLIB, gtk+-2.0 >=  2.2.0
                       )
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(GTHREADS, gthread-2.0 >=  2.2.0
                       )
AC_SUBST(GTHREADS_CFLAGS)
AC_SUBST(GTHREADS_LIBS)

dnl
dnl  Detect libsndfile 1.0
dnl

LIBSNDFILE_VERSION="???"

# !!! libsndfile0 backport

ac_enable_libsndfile0=no
AC_ARG_ENABLE(libsndfile0,
     [  --enable-libsndfile0    enable building against libsndfile version 0],
     [ ac_enable_libsndfile0=yes ])

if test "x${ac_enable_libsndfile0}" != xno ; then

  HAVE_LIBSNDFILE1="ignore"

  AC_CHECK_LIB(sndfile, sf_open_read, HAVE_LIBSNDFILE0="yes")
  AC_CHECK_LIB(sndfile, sf_command, HAVE_LIBSNDFILE0="yes")

  if test "x$HAVE_LIBSNDFILE0" = xyes ; then
    SNDFILE_LIBS="-lsndfile"
    AC_SUBST(SNDFILE_LIBS)
    LIBSNDFILE_VERSION="0"
  else
    AC_MSG_ERROR([
***
*** You have explicitly enabled to build against libsndfile version 0,
*** but there is no compatible version of libsndfile version 0 installed.
***
*** To continue this build, you must first install libsndfile version
*** 0.27.0 or 0.28.0.
***
*** Note that unless you are building against an older system distribution,
*** it is highly recommended that you install and build against
*** libsndfile version 1.
***
])
  fi

else # !!!

PKG_CHECK_MODULES(SNDFILE, sndfile >= 1.0.0,
                  HAVE_LIBSNDFILE1="yes", HAVE_LIBSNDFILE1="no")

if test "x$HAVE_LIBSNDFILE1" = xyes ; then
  AC_DEFINE(HAVE_LIBSNDFILE1)
  AC_SUBST(SNDFILE_LIBS)
  LIBSNDFILE_VERSION="1"
else
  sweep_config_ok="no"
  AC_CHECK_LIB(sndfile, sf_open, HAVE_LIBSNDFILE1="maybe")
  AC_CHECK_LIB(sndfile, sf_open_read, HAVE_LIBSNDFILE0="yes")
fi

if test "x$HAVE_LIBSNDFILE1" = xmaybe ; then
  AC_CHECK_HEADERS(sndfile.h, HAVE_LIBSNDFILE1="ouch")
  if test "x$HAVE_LIBSNDFILE1" = xouch ; then
    AC_MSG_ERROR([
***
*** libsndfile-1.0.0 or greater seems to exist on your system, however
*** the pkg-config tool cannot find its build information.
***
*** Please set your PKG_CONFIG_PATH environment variable to include the
*** directory in which sndfile.pc was installed. For example, sndfile.pc
*** is installed in /usr/local/lib/pkgconfig by default; if that is the
*** case do:
***
***     export PKG_CONFIG_PATH="\$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig"
***
*** then run this ./configure again. If you configured libsndfile with a
*** different --prefix option, replace /usr/local above with that prefix.
***
])
  fi
fi

fi # !!! libsndfile0 backport

AC_CHECK_SIZEOF(off_t,1)


dnl
dnl Detect Ogg
dnl

HAVE_OGG=no
HAVE_VORBIS=no
HAVE_SPEEX=no

ac_enable_oggvorbis=yes
AC_ARG_ENABLE(oggvorbis,
     [  --disable-oggvorbis     disable Ogg Vorbis support],
     [ ac_enable_oggvorbis=no ], [ ac_enable_oggvorbis=yes ])

ac_enable_speex=yes
AC_ARG_ENABLE(speex,
     [  --disable-speex         disable Speex support],
     [ ac_enable_speex=no ], [ ac_enable_speex=yes ])

dnl Both Vorbis and Speex require Ogg, so check for it first either way
if test "x${ac_enable_oggvorbis}" != xno ||
   test "x${ac_enable_speex}" != xno ; then

  XIPH_PATH_OGG(HAVE_OGG="yes", HAVE_OGG="no")
  if test "x$HAVE_OGG" = xno ; then
    AC_CHECK_LIB(ogg, ogg_stream_init, HAVE_OGG="maybe")
  fi
  if test "x$HAVE_OGG" = xmaybe; then
    AC_CHECK_HEADER([ogg/ogg.h], HAVE_OGG="yes", HAVE_OGG="no")
  fi

  AC_SUBST(OGG_LIBS)
fi

dnl
dnl Detect Vorbis
dnl
dnl LIBS="$LIBS -lm -logg -lvorbis -lvorbisenc -lvorbisfile"

if test "x$HAVE_OGG" = xyes && test "x${ac_enable_oggvorbis}" != xno ; then

  XIPH_PATH_VORBIS(HAVE_VORBIS="yes", HAVE_VORBIS="no")

  dnl Fall back to explicit checks if this fails
  if test "x$HAVE_VORBIS" = xno ; then
    AC_MSG_RESULT([
*** Trying explicit detection of Ogg Vorbis libraries and headers ...])
    AC_CHECK_LIB(vorbis, vorbis_info_init, HAVE_VORBIS="maybe", ,
                 [-lm -logg])
    AC_CHECK_LIB(vorbisenc, vorbis_encode_init, HAVE_VORBISENC="maybe", ,
                 [-lm -logg -lvorbis])
    AC_CHECK_LIB(vorbisfile, ov_open, HAVE_VORBISFILE="maybe", ,
                 [-lm -logg -lvorbis])
    AC_CHECK_HEADERS([vorbis/codec.h vorbis/vorbisfile.h vorbis/vorbisenc.h],
                     HAVE_VORBIS_HEADERS="yes", HAVE_VORBIS_HEADERS="no")

    if test "x$HAVE_VORBIS" = xmaybe && test "x$HAVE_VORBISENC" = xmaybe &&
       test "x$HAVE_VORBISFILE" = xmaybe &&
       test "x$HAVE_VORBIS_HEADERS" = xyes ; then

       VORBIS_LIBS="-lvorbis -lm"
       VORBISFILE_LIBS="-lvorbisfile"
       VORBISENC_LIBS="-lvorbisenc"
       VORBIS_CFLAGS=""

       dnl HAVE_VORBIS="maybe"
       HAVE_VORBIS="yes"
    else
       HAVE_VORBIS="no"
    fi
  fi

  AC_SUBST(VORBIS_LIBS)
  AC_SUBST(VORBISENC_LIBS)
  AC_SUBST(VORBISFILE_LIBS)

  if test "x$HAVE_VORBIS" = xyes ; then
    AC_DEFINE(HAVE_OGGVORBIS)

    dnl Test out ov_read_float
    AC_MSG_CHECKING([for deprecated ov_read_float() interface])

    ac_save_CFLAGS="$CFLAGS"
    ac_save_LIBS="$LIBS"
    CFLAGS="$CFLAGS $OGG_CFLAGS $VORBIS_CFLAGS"
    LIBS="$LIBS $OGG_LIBS $VORBIS_LIBS $VORBISFILE_LIBS"

    AC_TRY_COMPILE([#include <vorbis/codec.h>
                    #include <vorbis/vorbisfile.h> ],
                   [OggVorbis_File * vf; float ** pcm; int b;
                    ov_read_float (vf, &pcm, &b);],
		   [ AC_DEFINE(OV_READ_FLOAT_THREE_ARGS)
		     AC_MSG_RESULT(yes) ],
                   [ AC_MSG_RESULT(no) ])

    CFLAGS="$ac_save_CFLAGS"
    LIBS="$ac_save_LIBS"

  else
    sweep_config_ok="no"
  fi

else
  HAVE_VORBIS=disabled
fi

dnl
dnl Detect speex
dnl
dnl LIBS="$LIBS -lm -logg -lspeex"

if test "x$HAVE_OGG" = xyes && test "x${ac_enable_speex}" != xno ; then

  XIPH_PATH_SPEEX(HAVE_SPEEX="yes", HAVE_SPEEX="no")

  dnl Fall back to explicit checks if this fails
  if test "x$HAVE_SPEEX" = xno ; then
    AC_CHECK_LIB(speex, speex_encoder_init, HAVE_SPEEX="maybe", , [-lm -logg])
    if test "x$HAVE_SPEEX" = xmaybe; then
      AC_CHECK_HEADER(speex.h, HAVE_SPEEX="yes",
        [AC_CHECK_HEADER([speex/speex.h], HAVE_SPEEX="yes", HAVE_SPEEX="no")]
      )
    fi
  fi

  if test "x$HAVE_SPEEX" = xyes ; then
    AC_CHECK_HEADER([speex/speex.h],
                    AC_DEFINE(HAVE_SPEEX_SUBDIR, [1], [Define to 1 if speex headers are eg. <speex/speex.h>])
    )

    AC_DEFINE(HAVE_SPEEX)
    SPEEX_LIBS="-lspeex -lm"
    AC_SUBST(SPEEX_LIBS)
  fi

else
  HAVE_SPEEX=disabled
fi


dnl
dnl Detect libmad
dnl

HAVE_MAD=no

ac_enable_mad=yes
AC_ARG_ENABLE(mad,
     [  --disable-mad           disable libmad (MPEG audio loading) support],
     [ ac_enable_mad=no ], [ ac_enable_mad=yes ])

if test "x${ac_enable_mad}" != xno ; then
  AC_CHECK_LIB(mad, mad_decoder_init, HAVE_MAD="maybe")

  if test "x$HAVE_MAD" = xmaybe ; then
    AC_CHECK_HEADER(mad.h, HAVE_MAD="yes", HAVE_MAD="no")
  fi
  if test "x$HAVE_MAD" = xyes ; then
    AC_DEFINE(HAVE_MAD)
    MAD_LIBS="-lmad"
    AC_SUBST(MAD_LIBS)
  fi

else
  HAVE_MAD=disabled
fi

dnl
dnl Detect secret rabbit code
dnl

HAVE_LIBSAMPLERATE=no

ac_enable_src=yes
AC_ARG_ENABLE(src,
     [  --disable-src            enable secret rabbit code (libsamplerate)],
     [ ac_enable_src=no ], [ ac_enable_src=yes])

if test "x${ac_enable_src}" != xno ; then
  PKG_CHECK_MODULES(SAMPLERATE, samplerate >= 0.0.9,
                    HAVE_LIBSAMPLERATE="yes", HAVE_LIBSAMPLERATE="no")

  if test "x$HAVE_LIBSAMPLERATE" = xyes ; then
    AC_DEFINE(HAVE_LIBSAMPLERATE)
    AC_SUBST(SAMPLERATE_LIBS)
  fi
else
  HAVE_LIBSAMPLERATE=disabled
fi


dnl
dnl Test for ALSA -- requires explicit enabling
dnl


ac_enable_alsa=no
AC_ARG_ENABLE(alsa,
     [  --enable-alsa           enable use of ALSA, and disable use of OSS ],
     [ ac_enable_alsa=yes ])

if test "x${ac_enable_alsa}" != xno ; then

  AC_CHECK_HEADERS(alsa/asoundlib.h)
  if test "x${ac_cv_header_alsa_asoundlib_h}" = xyes ; then
    AC_DEFINE(DRIVER_ALSA)
    LIBS="$LIBS -lasound"
  fi

  sweep_config_driver="ALSA"
fi

dnl Test for OSS
AC_CHECK_HEADERS(sys/soundcard.h machine/soundcard.h)
if test "x${ac_cv_header_sys_soundcard_h}" = xyes || \
  test "x${ac_cv_header_machine_soundcard_h}" = xyes; then
  if test "x$ac_enable_alsa" = xno ; then
    AC_DEFINE(DRIVER_OSS)

    dnl Check for NetBSD's OSS compatability layer
    AC_CHECK_LIB(ossaudio, _oss_ioctl, LIBS="$LIBS -lossaudio")

    sweep_config_driver="OSS"
  fi
fi

dnl Test for Solaris audio
case $host in
*-*-solaris*)
AC_CHECK_HEADERS(sys/audioio.h)
if test "x${ac_cv_header_sys_audioio_h}" = xyes; then
    AC_DEFINE(DRIVER_SOLARIS_AUDIO)
    sweep_config_driver="Solaris"
fi
;;
esac


dnl Set PACKAGE_LOCALE_DIR in config.h.
if test "x${prefix}" = xNONE; then
  AC_DEFINE_UNQUOTED(PACKAGE_LOCALE_DIR, "${ac_default_prefix}/${DATADIRNAME}/locale")
else
  AC_DEFINE_UNQUOTED(PACKAGE_LOCALE_DIR, "${prefix}/${DATADIRNAME}/locale")
fi

dnl Set PACKAGE_DATA_DIR in config.h.
if test "x${datadir}" = 'x${prefix}/share'; then
  if test "x${prefix}" = xNONE; then
    AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${ac_default_prefix}/share/${PACKAGE}")
  else
    AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${prefix}/share/${PACKAGE}")
  fi
else
  AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${datadir}/${PACKAGE}")
fi

dnl Set PACKAGE_PLUGIN_DIR in Makefiles and config.h.
PACKAGE_PLUGIN_DIR=$libdir/sweep
AC_SUBST(PACKAGE_PLUGIN_DIR)
if test "x${prefix}" = xNONE; then
  sweep_plugin_dir="${ac_default_prefix}/lib/${PACKAGE}"
else
  sweep_plugin_dir="${prefix}/lib/${PACKAGE}"
fi
AC_DEFINE_UNQUOTED(PACKAGE_PLUGIN_DIR, "$sweep_plugin_dir")

dnl Set PACKAGE_SOURCE_DIR in config.h.
packagesrcdir=`cd $srcdir && pwd`
AC_DEFINE_UNQUOTED(PACKAGE_SOURCE_DIR, "${packagesrcdir}")


dnl Use -Wall if we have gcc.
dnl changequote(,)dnl
if test "x$ac_cv_prog_gcc" = xyes ; then
  CFLAGS="$CFLAGS -Wall"
fi
dnl changequote([,])dnl


dnl
dnl  Configuration tests complete -- provide summary of results.
dnl

if test $sweep_config_ok = no ; then

AC_MSG_RESULT([
**************************************************************

*** Sweep $VERSION: Automatic configuration FAILED.

*** The file config.log has full details.

*** The following required libraries are missing or
*** misconfigured on your system:
])

if test "x$HAVE_GLIB" != xyes ; then
  AC_MSG_RESULT([
*** GLib version 1.2.x, available from  http://www.gtk.org/
])
fi
if test "x$HAVE_GLIB" = xmaybe ; then
  AC_MSG_RESULT([
      Development files missing: The libglib binary seems
      to be installed, but the glib-config program required
      for building programs could not be found.
])
fi

if test "x$HAVE_GTK" != xyes ; then
  AC_MSG_RESULT([
*** GTK+ version 1.2.x, available from  http://www.gtk.org/
])
fi
if test "x$HAVE_GTK" = xmaybe ; then
  AC_MSG_RESULT([
      Development files missing: The libgtk binary seems
      to be installed, but the gtk-config program required
      for building programs could not be found.
])
fi

if test "x$HAVE_LIBSNDFILE1" != xyes ; then
  AC_MSG_RESULT([
*** libsndfile version 1.0.0 or greater, available from
*** http://www.mega-nerd.com/libsndfile/
])
fi
if test "x$HAVE_LIBSNDFILE1" = xmaybe ; then
    AC_MSG_RESULT([
      Development files missing: libsndfile-1.0.0 or greater
      seems to exist on your system, however the header files
      are missing and the pkg-config tool cannot find its build
      information.
])
fi
if test "x$HAVE_LIBSNDFILE0" = xyes ; then
    AC_MSG_RESULT([
      Incorrect version: Your system appears to have an old
      version of libsndfile (below 1.0.0) installed.
])
fi

if test "x$HAVE_VORBIS" != xyes ; then
    AC_MSG_RESULT([
*** Ogg Vorbis libraries, available from  http://www.xiph.org/ogg/vorbis/
])
fi
if test "x$HAVE_VORBIS" = xmaybe ; then
    AC_MSG_RESULT([
      Development files missing: The libogg, libvorbis, libvorbisenc and
      libvorbisfile library binaries seem to be installed, but building of
      a test program failed.
])
fi 
if test "x$HAVE_VORBIS" != xyes ; then
    AC_MSG_RESULT([
*** Or, to build a version of Sweep without Ogg Vorbis support, do:
***
***         ./configure --disable-oggvorbis
])
fi

if test "x$HAVE_GLIB" = xmaybe || test "x$HAVE_GTK" = xmaybe ||
   test "x$HAVE_LIBSNDFILE1" = xmaybe || test "x$HAVE_TDB" = xmaybe ||
   test "x$HAVE_OGG" = xmaybe || test "x$HAVE_VORBIS" = xmaybe ; then
  AC_MSG_RESULT([
*** HOW TO FIX "development files missing": you have probably
*** installed the library package from your distribution. You
*** must also install the library's "development" package.
])
fi

AC_MSG_RESULT([
*** If you install the required libraries from source, you
*** need to inform the dynamic linker of their location. If
*** you install them in a system-wide directory such as
*** /usr/local (the default), you must ensure that
*** /usr/local/lib is listed in /etc/ld.so.conf, then run
*** ldconfig to update the dynamic linking system.
*** Alternatively, you can set your LD_LIBRARY_PATH environment
*** variable to include the library installation directory.
])


AC_MSG_RESULT([
**************************************************************
])

AC_MSG_ERROR([
***
*** After fixing the above problems, you must run ./configure again.
***
])

else

AC_OUTPUT([
Makefile
sweep.spec
m4/Makefile
include/Makefile
include/sweep/Makefile
src/Makefile
src/tdb/Makefile
plugins/Makefile
plugins/echo/Makefile
plugins/normalise/Makefile
plugins/fade/Makefile
plugins/reverse/Makefile
plugins/ladspa/Makefile
plugins/byenergy/Makefile
plugins/example/Makefile
doc/Makefile
intl/Makefile
po/Makefile.in
])

AC_MSG_RESULT([
  ************************************************************
 * Sweep $VERSION:  Automatic configuration OK.
**
**  Configuration summary:
**
**     Experimental code: ....... ${ac_enable_experimental}
**     Audio device handling: ... $sweep_config_driver
**     PCM: libsndfile version .. $LIBSNDFILE_VERSION
**     Ogg Vorbis support: ...... $HAVE_VORBIS
**     MPEG (MP3) loading: ...... $HAVE_MAD
**     Speex support: ........... $HAVE_SPEEX
**     Secret Rabbit Code: ...... $HAVE_LIBSAMPLERATE
**     Translations: ............ $ALL_LINGUAS
**
**  Installation directories:
**
**     Program: ................. ${prefix}/bin
**     Plugins: ................. $sweep_plugin_dir
**
**************************************************************
**
**  Type 'make' to compile Sweep.
**
**  Type 'make install' to install Sweep. Plugins will not be
**  accessible until you have done this.
**  
**  Have fun with Scrubby!
 *
  ************************************************************
])

fi