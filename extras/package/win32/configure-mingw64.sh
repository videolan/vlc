#!/bin/sh

root=$(echo $0|sed 's#extras/package/win32/configure-mingw64.sh##')./

if [ -n "$1" ]
then
	CONTRIBS="$1"
else
	CONTRIBS="/usr/win64"
fi
export CONTRIBS

PATH="$CONTRIBS/bin:$PATH" \
PKG_CONFIG_LIBDIR=$CONTRIBS/lib/pkgconfig \
CPPFLAGS="-I$CONTRIBS/include -I$CONTRIBS/include/ebml" \
LDFLAGS="-L$CONTRIBS/lib" \
CC=amd64-mingw32msvc-gcc CXX=amd64-mingw32msvc-g++ \
CONFIGURE="${root}configure" \
CONFIGOPTS="--host=amd64-mingw32msvc --build=i386-linux
 --enable-dirac --enable-mkv --enable-taglib --enable-debug --enable-projectm
 --disable-qt4 --disable-skins2 --disable-activex --disable-mozilla" \
sh ${root}extras/package/win32/configure-common.sh
