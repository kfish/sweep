#!/bin/sh

# autogoat!
# an omnivorous assistant for autotools
#
#         (__) 
#         (oo) 
#   /------\/ 
#  / |    ||   
# *  /\---/\ 
#    ~~   ~~   

# clean function
clean ()
{
  # remove autotools cruft
  rm -f aclocal.m4 configure config.log
  rm -Rf autom4te.cache
  # remove old autotools extra cruft
  rm -f config.guess config.sub missing mkinstalldirs compile depcomp install-sh
  # remove libtool cruft
  rm -f ltmain.sh libtool ltconfig
}


#
# option checking
#

if test "x$1" = "xclean"; then
  set -x
  clean
  set +x
  exit 0
fi



#
# check automake version number -- we require >= 1.5
#

automake_version="none"
if automake-1.7 --version >/dev/null 2>&1; then
  automake_version="-1.7"
elif automake-1.6 --version >/dev/null 2>&1; then
  automake_version="-1.6"
elif automake-1.5 --version >/dev/null 2>&1; then
  automake_version="-1.5"
elif automake --version > /dev/null 2>&1; then
  automake_version=""
  case "`automake --version | sed -e '1s/[^0-9]*//' -e q`" in
    0|0.*|1|1.[01234]|1.[01234][-.]*) automake_version="none" ;;
    1.5*)                             automake_version="-1.5" ;;
    1.6*)                             automake_version="-1.6" ;;
    1.7*)                             automake_version="-1.7" ;;
    1.8*)                             automake_version="-1.8" ;;
    1.9*)                             automake_version="-1.9" ;;
  esac
fi

if test "x${automake_version}" = "xnone"; then
  set +x
  echo "you need automake version 1.5 or later"
  exit 1
fi

automake_version_major=`echo "$automake_version" | cut -d. -f2`
automake_version_minor=`echo "$automake_version" | cut -d. -f3`

# need at least automake >= 1.5
if test "$automake_version_major" -lt "5"; then
  echo "$0"': this project requires automake >= 1.5.  Please upgrade your version of automake to at least 1.5'
  exit 1
fi


#
# do we need pkg-config?
#

if grep -q PKG_CHECK_MODULES configure.*; then
  if ! pkg-config --version > /dev/null 2> /dev/null; then
    cat << EOF
pkg-config is required, but it's not installed or can't be found in your
search path.

EOF
    # be nice to the user if they have fink!
    if test -d /sw ; then
      cat << EOF
You can install it via Fink with the command:

 apt-get install pkgconfig

EOF
    fi
    exit 1
  fi
fi


#
# autogoat bootstrap process
# 

ACLOCAL=${ACLOCAL:-aclocal}
AUTOCONF=${AUTOCONF:-autoconf}
AUTOHEADER=${AUTOHEADER:-autoheader}
AUTOMAKE=${AUTOMAKE:-automake}

# clean out old cruft
clean

# add Fink's /sw path to various search directories
if [ -d /sw ]; then
  ACLOCAL="$ACLOCAL -I /sw/share/aclocal"
  FINK_DETECTED=1
fi

eval "$ACLOCAL -I m4"

# do we need libtool?
if grep -q PROG_LIBTOOL configure.*; then
  # what's libtoolize called?
  if glibtoolize --version > /dev/null 2> /dev/null; then
    LIBTOOLIZE="glibtoolize"
  elif libtoolize --version > /dev/null 2> /dev/null; then
    LIBTOOLIZE="libtoolize"
  fi

  # check libtool version -- only support 1.4 or 1.5 for now
  if "$LIBTOOLIZE" --version | egrep -q '1\.4|1\.5'; then
    if grep -q AC_LIBLTDL_CONVENIENCE configure.*; then
      "$LIBTOOLIZE" --ltdl --copy --force
    else
      "$LIBTOOLIZE" --copy --force
    fi
  else
    # libtool version is too old :(
    echo "$0: need libtool >= 1.4 installed"
    exit 1
  fi
fi

eval "$AUTOCONF"
grep -q CONFIG_HEADER configure.* && "$AUTOHEADER"
eval "$AUTOMAKE" --add-missing --copy

# Print warning message if Fink detected
if test "$FINK_DETECTED" = 1; then
  cat << EOF

Fink detected; added /sw/share/aclocal to aclocal's include directories.
Make sure you have CPPFLAGS, LDFLAGS and PKG_CONFIG_PATH including Fink's
distribution directories, e.g.:

 export CPPFLAGS="-I/sw/include \$CPPFLAGS"
 export LDFLAGS="-L/sw/lib \$LDFLAGS"
 export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:/sw/lib/pkgconfig:\$PKG_CONFIG_PATH"

 ./configure

EOF
fi

