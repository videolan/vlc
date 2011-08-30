#!/bin/sh

if [ -z "$ANDROID_NDK" ]; then
    echo "Please set the ANDROID_NDK environment variable with its path."
    exit 1
fi

ANDROID_API=android-9

VLC_SOURCEDIR="`dirname $0`/../../.."

# needed for old ndk: change all the arm-linux-androideabi to arm-eabi
# the --host is kept on purpose because otherwise libtool complains..

CFLAGS="-O2 -mlong-calls -fstrict-aliasing -fprefetch-loop-arrays -ffast-math"
LDFLAGS="-Wl,-Bdynamic,-dynamic-linker=/system/bin/linker -Wl,--no-undefined"

if [ -z "$NO_NEON" ]; then
    CXX_TARGET="armeabi-v7a"
    CFLAGS="$CFLAGS -mfpu=neon -mtune=cortex-a8 -ftree-vectorize -mvectorize-with-neon-quad"
    LDFLAGS="$LDFLAGS -Wl,--fix-cortex-a8"
    EXTRA_PARAMS=""
else
    CXX_TARGET="armeabi"
    CFLAGS="$CFLAGS -march=armv6j -mtune=arm1136j-s -msoft-float"
    EXTRA_PARAMS=" --disable-neon"
fi

CPPFLAGS="-I${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/include -I${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/libs/${CXX_TARGET}/include"

SYSROOT=$ANDROID_NDK/platforms/$ANDROID_API/arch-arm
ANDROID_BIN=$ANDROID_NDK/toolchains/arm-linux-androideabi-4.4.3/prebuilt/*-x86/bin/
CROSS_COMPILE=${ANDROID_BIN}/arm-linux-androideabi-

CPPFLAGS="$CPPFLAGS" \
CFLAGS="$CFLAGS" \
CXXFLAGS="$CFLAGS" \
LDFLAGS="$LDFLAGS" \
CC="${CROSS_COMPILE}gcc --sysroot=${SYSROOT}" \
CXX="${CROSS_COMPILE}g++ --sysroot=${SYSROOT}" \
NM="${CROSS_COMPILE}nm" \
STRIP="${CROSS_COMPILE}strip" \
RANLIB="${CROSS_COMPILE}ranlib" \
AR="${CROSS_COMPILE}ar" \
PKG_CONFIG_LIBDIR="$VLC_SOURCEDIR/extras/contrib/hosts/arm-eabi/lib/pkgconfig" \
sh $VLC_SOURCEDIR/configure --host=arm-eabi-linux --build=x86_64-unknown-linux $EXTRA_PARAMS \
                --enable-live555 --enable-realrtsp \
                --enable-avformat \
                --enable-swscale \
                --enable-avcodec \
                --enable-opensles \
                --enable-android-surface \
                --enable-debug \
                --enable-mkv \
                --enable-taglib \
                --disable-vlc --disable-shared \
                --disable-vlm --disable-sout \
                --disable-dbus \
                --disable-lua \
                --disable-libgcrypt \
                --disable-vcd \
                --disable-v4l2 \
                --disable-gnomevfs \
                --disable-dvdread \
                --disable-dvdnav \
                --disable-bluray \
                --disable-linsys \
                --disable-decklink \
                --disable-libva \
                --disable-dv \
                --disable-mod \
                --disable-sid \
                --disable-mad \
                --disable-x264 \
                --disable-mad \
                --disable-schroedinger --disable-dirac \
                --disable-sdl-image \
                --disable-zvbi \
                --disable-fluidsynth \
                --disable-jack \
                --disable-pulse \
                --disable-alsa \
                --disable-portaudio \
                --disable-sdl \
                --disable-xcb \
                --disable-atmo \
                --disable-qt4 \
                --disable-skins2 \
                --disable-mtp \
                --enable-taglib \
                --disable-notify \
                --disable-freetype \
                --disable-libass \
                --disable-svg \
                --disable-sqlite \
                --disable-udev \
                --disable-libxml2 \
                --disable-caca \
                --disable-glx \
                --disable-egl \
                --disable-gl \
                --disable-gles1 --disable-gles2 \
                --disable-goom \
                --disable-projectm
