#!/bin/sh

root=$(echo $0|sed 's#extras/package/win32/configure-cygwin.sh##')./

export CONTRIB_DIR="$1"

CC="gcc -mno-cygwin" CXX="g++ -mno-cygwin" \
CONFIGURE="${root}configure" \
CONFIGOPTS="--host=i686-pc-mingw32
--disable-nls --disable-taglib --disable-mkv --disable-dirac --enable-debug" \
sh ${root}extras/package/win32/configure-common.sh
