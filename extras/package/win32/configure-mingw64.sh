#!/bin/sh

root=$(echo $0|sed 's#extras/package/win32/configure-mingw64.sh##')./

if [ -n "$1" ]
then
	CONTRIB_DIR="$1"
else
	CONTRIB_DIR="/usr/win64"
fi
export CONTRIB_DIR

CC=amd64-mingw32msvc-gcc CXX=amd64-mingw32msvc-g++ \
CONFIGURE="${root}configure" \
CONFIGOPTS="--host=amd64-mingw32msvc --build=i386-linux
 --enable-mkv --enable-taglib --enable-debug --enable-projectm
 --disable-qt4 --disable-skins2" \
sh ${root}extras/package/win32/configure-common.sh
