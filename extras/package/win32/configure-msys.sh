#!/bin/sh

root=$(echo $0|sed 's#extras/package/win32/configure-msys.sh##')./

if [ -n "$1" ]
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
CC=gcc CXX=g++ \
CONFIG="${root}configure --host=i586-mingw32msvc --build=i386-linux
    --disable-mkv --disable-taglib --disable-nls --disable-dirac --enable-debug" \
sh ${root}extras/package/win32/configure-common.sh
