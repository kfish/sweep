#!/bin/sh

# svn-update.sh
# Update source files from an upstream Subversion repository
#
# Conrad Parker, 2004

# The files to update
FILES="tdb.c tdb.h spinlock.c spinlock.h"

# The directory in a subversion repository containing the source files
REPOSITORY="svn://svnanon.samba.org/samba/trunk/source/tdb"

# The name of the working dir for checkout
UPSTREAM="svnanon.samba.org:tdb"

# The name of the generated diff file
CHANGES="changes.diff"

## End configuration ##

COPIED=""

echo "Retrieving sources from $REPOSITORY ..."

rm -rf $UPSTREAM
svn co $REPOSITORY $UPSTREAM

[ $? != 0 ] && exit 1

[ -e $CHANGES ] && mv $CHANGES $CHANGES.bak

for i in $FILES; do
  if [ -e $i ] ; then
    diff -u $i $UPSTREAM/$i >> $CHANGES
    if [ $? != 0 ] ; then
      mv $i $i.bak
      cp $UPSTREAM/$i .
      COPIED="$COPIED $i"
    fi
  else
    cp $UPSTREAM/$i .
    COPIED="$COPIED $i"
  fi
done

if [ "x$COPIED" != "x" ] ; then
  echo "Copied files: $COPIED"
  [ -e $CHANGES ] && echo "Differences:  $CHANGES"
else
  rm $CHANGES
  echo "No changes."
fi
