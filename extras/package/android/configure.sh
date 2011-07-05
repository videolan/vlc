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
EXTRA_PARAMS=""
if [ -z "$NO_NEON" ]; then
	EXTRA_CFLAGS="$EXTRA_CFLAGS -mfpu=neon -mtune=cortex-a8 -ftree-vectorize -mvectorize-with-neon-quad"
	EXTRA_LDFLAGS="-Wl,--fix-cortex-a8"
else
	EXTRA_CFLAGS="$EXTRA_CFLAGS -march=armv6j -mtune=arm1136j-s -msoft-float"
	EXTRA_PARAMS=" --disable-neon"
fi

PATH="$ANDROID_BIN:$PATH" \
CC="${GCC_PREFIX}gcc" \
CXX="${GCC_PREFIX}g++" \
NM="${GCC_PREFIX}nm" \
STRIP="${GCC_PREFIX}strip" \
RANLIB="${GCC_PREFIX}ranlib" \
AR="${GCC_PREFIX}ar" \
CPPFLAGS="-I$ANDROID_INCLUDE -I$VLC_SOURCEDIR/extras/contrib/hosts/arm-eabi/include \
        -I${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/include \
        -I${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/libs/armeabi-v7a/include/" \
LDFLAGS="-Wl,-rpath-link=$ANDROID_LIB,-Bdynamic,-dynamic-linker=/system/bin/linker -Wl,--no-undefined -Wl,-shared -L$ANDROID_LIB $EXTRA_LDFLAGS" \
CFLAGS="-nostdlib $EXTRA_CFLAGS -O2" \
CXXFLAGS="-nostdlib $EXTRA_CFLAGS -O2 -D__STDC_VERSION__=199901L -D__STDC_CONSTANT_MACROS" \
LIBS="-lc -ldl -lgcc" \
PKG_CONFIG_LIBDIR="$VLC_SOURCEDIR/extras/contrib/hosts/arm-eabi/lib/pkgconfig" \
sh $VLC_SOURCEDIR/configure --host=arm-eabi-linux --build=x86_64-unknown-linux $EXTRA_PARAMS \
                --enable-static-modules \
                --disable-vlc \
                --enable-debug \
                --disable-vlm --disable-sout \
                --disable-dbus \
                --disable-lua \
                --disable-libgcrypt \
                --enable-live555 --enable-realrtsp \
                --disable-vcd \
                --disable-v4l2 \
                --disable-gnomevfs \
                --disable-dvdread \
                --disable-dvdnav \
                --disable-bluray \
                --disable-linsys \
                --disable-decklink \
                --enable-avformat \
                --enable-swscale \
                --enable-avcodec \
                --disable-libva \
                --disable-mkv \
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
                --enable-opensles \
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
                --disable-taglib \
                --disable-notify \
                --disable-freetype \
                --disable-libass \
                --disable-svg \
                --disable-sqlite \
                --disable-udev \
                --disable-libxml2 \
                --enable-android-surface \
                --disable-caca \
                --disable-glx \
                --disable-egl \
                --disable-gl \
                --disable-gles1 --disable-gles2 \
                --disable-goom \
                --disable-projectm
