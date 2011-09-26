#!/bin/sh

root=$(echo $0|sed 's#extras/package/win32/configure-mingw-release.sh##')./

export CONTRIB_DIR="$1"

CC=i586-mingw32msvc-gcc CXX=i586-mingw32msvc-g++ \
CONFIGURE="${root}configure" \
CONFIGOPTS="--host=i586-mingw32msvc --build=i386-linux
 --enable-mkv --enable-taglib --enable-nls --enable-projectm" \
sh ${root}extras/package/win32/configure-common.sh
