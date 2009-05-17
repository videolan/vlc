#!/bin/sh

root=$(echo $0|sed 's#extras/package/win32/configure-msys.sh##')./

PATH="/usr/win32/bin:$PATH" \
PKG_CONFIG_LIBDIR=/usr/win32/lib/pkgconfig \
CPPFLAGS="-I/usr/win32/include -I/usr/win32/include/ebml" \
LDFLAGS="-L/usr/win32/lib" \
CC=gcc CXX=g++ \
CONFIG="${root}configure --host=i586-mingw32msvc --build=i386-linux
    --disable-mkv --disable-taglib --disable-nls --disable-dirac --enable-debug" \
sh ${root}extras/package/win32/configure-common.sh
