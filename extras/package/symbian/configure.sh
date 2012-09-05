#!/bin/sh

SYMBIAN_ROOT=/mnt/vlc/symbian
SYMBIAN_SDK=$SYMBIAN_ROOT/sdks
EPOCROOT=$SYMBIAN_SDK
PATH=$SYMBIAN_ROOT/gnupoc:$SYMBIAN_ROOT/cls-gcc/bin:$SYMBIAN_ROOT/qt_4.7.1/bin:$PATH
SYMBIAN_INCLUDE=$SYMBIAN_SDK/epoc32/include

echo "export EPOCROOT=$EPOCROOT;export PATH=$PATH;"

CC=arm-none-symbianelf-gcc CXX=arm-none-symbianelf-g++ \
PKG_CONFIG_PATH="" PKG_CONFIG_LIBDIR="" \
LDFLAGS="-L$SYMBIAN_ROOT/cls-gcc/arm-none-symbianelf/lib -L$EPOCROOT/epoc32/release/armv5/lib -nostdlib -shared -Wl,--no-undefined $EPOCROOT/epoc32/release/armv5/lib/libc.dso " \
CPPFLAGS="-D_UNICODE -D__GCCE__ -D__SYMBIAN32__ -D__S60_3X__ -D__FreeBSD_cc_version -include $SYMBIAN_INCLUDE/gcce/gcce.h  -I$SYMBIAN_INCLUDE/stdapis -I$SYMBIAN_INCLUDE/stdapis/sys -I$SYMBIAN_INCLUDE/variant -I$SYMBIAN_INCLUDE " \
../configure --host=arm-none-symbianelf --build=i386-linux \
  --disable-vlc --enable-static --disable-shared --enable-static-modules \
  --disable-mad --disable-avcodec --disable-a52 --disable-libgcrypt --disable-remoteosd \
  --disable-lua --disable-swscale --disable-postproc --disable-qt --disable-skins2
