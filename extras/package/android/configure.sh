#!/bin/sh

if [ -z "$ANDROID_NDK" ]; then
    echo "Please set the ANDROID_NDK environment variable with its path."
    exit 1
fi

ANDROID_API=android-8

ANDROID_BIN=$ANDROID_NDK/toolchains/arm-linux-androideabi-4.4.3/prebuilt/linux-x86/bin/
ANDROID_INCLUDE=$ANDROID_NDK/platforms/$ANDROID_API/arch-arm/usr/include
ANDROID_LIB=$ANDROID_NDK/platforms/$ANDROID_API/arch-arm/usr/lib
GCC_PREFIX=${ANDROID_BIN}/arm-linux-androideabi-

VLC_SOURCEDIR="`dirname $0`/../../.."

# needed for old ndk: change all the arm-linux-androideabi to arm-eabi
# the --host is kept on purpose because otherwise libtool complains..

PATH="$ANDROID_BIN":$PATH \
CPPFLAGS="-I$ANDROID_INCLUDE" \
LDFLAGS="-Wl,-rpath-link=$ANDROID_LIB,-Bdynamic,-dynamic-linker=/system/bin/linker -Wl,--no-undefined -Wl,-shared -L$ANDROID_LIB" \
CFLAGS="-nostdlib" \
CXXFLAGS="-nostdlib" \
LIBS="-lc -ldl -lgcc" \
CC="${GCC_PREFIX}gcc" \
CXX="${GCC_PREFIX}g++" \
NM="${GCC_PREFIX}nm" \
STRIP="${GCC_PREFIX}strip" \
PKG_CONFIG_LIBDIR="$VLC_SOURCEDIR/extras/contrib/hosts/arm-eabi/lib/pkgconfig" \
sh ../configure --host=arm-eabi-linux --build=x86_64-unknown-linux \
                --enable-static-modules \
                --enable-debug \
                --disable-qt4 \
                --disable-skins2 \
                --disable-mad \
                --disable-mkv \
                --disable-live555 \
                --disable-libgcrypt \
                --disable-lua \
                --enable-swscale \
                --enable-avcodec \
                --enable-avformat \
                --disable-xcb \
                --disable-dbus \
                --disable-vcd \
                --disable-v4l2 \
                --disable-atmo \
                --disable-vlc
