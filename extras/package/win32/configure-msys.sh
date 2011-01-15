#!/bin/sh

root=$(echo $0|sed 's#extras/package/win32/configure-msys.sh##')./

export CONTRIB_DIR="$1"

CC=gcc CXX=g++ \
CONFIGURE="${root}configure" \
CONFIGOPTS="--host=i586-mingw32msvc --build=i386-linux
    --disable-mkv --disable-taglib --disable-nls --enable-debug" \
sh ${root}extras/package/win32/configure-common.sh
