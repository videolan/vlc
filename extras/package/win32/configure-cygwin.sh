#!/bin/sh

root=$(echo $0|sed 's#extras/package/win32/configure-cygwin.sh##')./

PATH="/usr/win32/bin:$PATH" \
PKG_CONFIG_LIBDIR=/usr/win32/lib/pkgconfig \
CPPFLAGS="-I/usr/win32/include -I/usr/win32/include/ebml" \
LDFLAGS="-L/usr/win32/lib" \
CC="gcc -mno-cygwin" CXX="g++ -mno-cygwin" \
CONFIG="${root}configure --host=i686-pc-mingw32
       --disable-nls --disable-taglib --disable-mkv --disable-dirac --enable-debug" \
sh ${root}extras/package/win32/configure-common.sh
