#!/bin/sh

root=$(echo $0|sed 's#extras/package/win32/configure-mingw64.sh##')./

if [ -n "$1" ]
then
	CONTRIB_DIR="$1"
else
	CONTRIB_DIR="/usr/win64"
fi
export CONTRIB_DIR

CONFIGURE="${root}configure" \
CONFIGOPTS="--host=x86_64-w64-mingw32 --build=i386-linux
 --enable-mkv --enable-taglib --disable-real --enable-debug" \
sh ${root}extras/package/win32/configure-common.sh
