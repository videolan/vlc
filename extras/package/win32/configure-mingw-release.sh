#!/bin/sh

root=$(echo $0|sed 's#extras/package/win32/configure-mingw-release.sh##')./

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
CC=i586-mingw32msvc-gcc CXX=i586-mingw32msvc-g++ \
CONFIG="${root}configure --host=i586-mingw32msvc --build=i386-linux
 --enable-dirac --enable-mkv --enable-taglib --enable-release --enable-nls --enable-projectm" \
sh ${root}extras/package/win32/configure-common.sh
