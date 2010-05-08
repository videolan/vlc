#!/bin/sh

root=$(echo $0|sed 's#extras/package/win32/configure-cygwin.sh##')./

if [ -n $1 ]
then
       CONTRIBS="$1"
else
       CONTRIBS="/usr/win32"
fi
export CONTRIBS

PATH="$CONTRIBS/bin:$PATH" \
PKG_CONFIG_LIBDIR=$CONTRIBS/lib/pkgconfig \
CPPFLAGS="-I$CONTRIBS/include -I$CONTRIBS/include/ebml" \
LDFLAGS="-L$CONTRIBS/lib" \
CC="gcc -mno-cygwin" CXX="g++ -mno-cygwin" \
CONFIG="${root}configure --host=i686-pc-mingw32
       --disable-nls --disable-taglib --disable-mkv --disable-dirac --enable-debug" \
sh ${root}extras/package/win32/configure-common.sh
