#!/bin/sh

if [ -z "$ANDROID_NDK" ]; then
    echo "Please set the ANDROID_NDK environment variable with its path."
    exit 1
fi

ANDROID_API=android-9

ANDROID_BIN=$ANDROID_NDK/toolchains/arm-linux-androideabi-4.4.3/prebuilt/*-x86/bin/
ANDROID_INCLUDE=$ANDROID_NDK/platforms/$ANDROID_API/arch-arm/usr/include
ANDROID_LIB=$ANDROID_NDK/platforms/$ANDROID_API/arch-arm/usr/lib
GCC_PREFIX=${ANDROID_BIN}/arm-linux-androideabi-

VLC_SOURCEDIR="`dirname $0`/../../.."

# needed for old ndk: change all the arm-linux-androideabi to arm-eabi
# the --host is kept on purpose because otherwise libtool complains..

EXTRA_CFLAGS="-mlong-calls -fstrict-aliasing -fprefetch-loop-arrays -ffast-math"
EXTRA_LDFLAGS=""
if [ -z "$NO_NEON" ]; then
	EXTRA_CFLAGS="$EXTRA_CFLAGS -mfpu=neon -mtune=cortex-a8 -ftree-vectorize -mvectorize-with-neon-quad"
	EXTRA_LDFLAGS="-Wl,--fix-cortex-a8"
else
	EXTRA_CFLAGS="$EXTRA_CFLAGS -march=armv6j -mtune=arm1136j-s -msoft-float"
fi

PATH="$ANDROID_BIN:$PATH" \
CPPFLAGS="-I$ANDROID_INCLUDE" \
LDFLAGS="-Wl,-rpath-link=$ANDROID_LIB,-Bdynamic,-dynamic-linker=/system/bin/linker -Wl,--no-undefined -Wl,-shared -L$ANDROID_LIB $EXTRA_LDFLAGS" \
CFLAGS="-nostdlib $EXTRA_CFLAGS -O2" \
CXXFLAGS="-nostdlib $EXTRA_CFLAGS -O2" \
LIBS="-lc -ldl -lgcc" \
CC="${GCC_PREFIX}gcc" \
CXX="${GCC_PREFIX}g++" \
NM="${GCC_PREFIX}nm" \
STRIP="${GCC_PREFIX}strip" \
RANLIB="${GCC_PREFIX}ranlib" \
AR="${GCC_PREFIX}ar" \
PKG_CONFIG_LIBDIR="$VLC_SOURCEDIR/extras/contrib/hosts/arm-eabi/lib/pkgconfig" \
sh $VLC_SOURCEDIR/configure --host=arm-eabi-linux --build=x86_64-unknown-linux \
                --enable-static-modules \
                --disable-vlc \
                --enable-debug \
                --enable-swscale \
                --enable-avcodec \
                --enable-avformat \
                --enable-android-vout \
                --enable-live555 --enable-realrtsp \
                --disable-libva \
                --disable-jack \
                --disable-pulse \
                --disable-alsa \
                --disable-sdl \
                --enable-opensles \
                --disable-schroedinger \
                --disable-x264 \
                --disable-mad \
                --disable-mkv \
                --disable-dv \
                --disable-vcd \
                --disable-v4l2 \
                --disable-gnomevfs \
                --disable-dvdread \
                --disable-dvdnav \
                --disable-linsys \
                --disable-xcb \
                --disable-dbus \
                --disable-atmo \
                --disable-qt4 \
                --disable-skins2 \
                --disable-libgcrypt \
                --disable-lua \
                --disable-mtp \
                --disable-sdl-image \
                --disable-taglib \
                --disable-notify \
                --disable-freetype \
                --disable-sqlite \
                --disable-udev \
                --disable-caca \
                --disable-glx \
                --disable-egl \
                --disable-gl \
                --disable-libxml2 \
                --disable-svg
