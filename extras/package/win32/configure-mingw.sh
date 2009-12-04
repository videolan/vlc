#!/bin/sh

root=$(echo $0|sed 's#extras/package/win32/configure-mingw.sh##')./

PATH="/usr/win32/bin:$PATH" \
PKG_CONFIG_LIBDIR=/usr/win32/lib/pkgconfig \
CPPFLAGS="-I/usr/win32/include -I/usr/win32/include/ebml" \
LDFLAGS="-L/usr/win32/lib" \
CC=i586-mingw32msvc-gcc CXX=i586-mingw32msvc-g++ \
CONFIG="${root}configure --host=i586-mingw32msvc --build=i386-linux
 --enable-dirac --enable-mkv --enable-taglib --enable-debug --enable-projectm" \
sh ${root}extras/package/win32/configure-common.sh
