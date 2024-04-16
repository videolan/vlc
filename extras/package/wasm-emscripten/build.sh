#! /bin/sh
# Copyright (C) 2003-2021 the VideoLAN team
#
# This file is under the same license as the vlc package.

set -e

diagnostic()
{
    echo "### build logs ###: " "$@" 1>&2;
}

usage()
{
    echo "Usage: $0 [--mode=(default=1)]"
    echo "  --with-prebuilt-contribs | -c"
    echo "  --mode=1 build all "
    echo "  --mode=0 incremental build (do not bootstrap and configure) "
}

NM="emnm"
get_symbol()
{
    echo "$1" | grep vlc_entry_"$2" | cut -d " " -f 3
}

get_entryname()
{
    symbols=$($NM -g "$1")
    entryname=$(get_symbol "$symbols" _)
    echo "$entryname"
}

VLC_USE_SANITIZER=

while test -n "$1"
do
    case "$1" in
        --help|-h)
            usage
            exit 0
            ;;
        --mode=*)
            BUILD_MODE="${1#--mode=}"
            ;;
        --with-prebuilt-contribs)
            VLC_USE_PREBUILT_CONTRIBS=1
            ;;
        --with-sanitizer=*)
            VLC_USE_SANITIZER="${1#--with-sanitizer=}"
            ;;
        --enable-shared)
            VLC_COMPILE_SHARED=1
            ;;
        --enable-extra-checks)
            VLC_BUILD_EXTRA_CHECKS=1
            ;;
        --gen-contrib-archive|-c)
            GENERATE_ARCHIVE=1
            ;;
        *)
            echo "Unrecognized options $1"
            usage
            exit 1
            ;;
    esac
    shift
done

BUILD_MODE=${BUILD_MODE:=1}
VLC_PREBUILT_CONTRIBS_URL=${VLC_PREBUILT_CONTRIBS_URL:-""}
BUILDDIR_NAME="build-emscripten"
GENERATE_ARCHIVE=${GENERATE_ARCHIVE:=0}
VLC_USE_PREBUILT_CONTRIBS=${VLC_USE_PREBUILT_CONTRIBS:=0}
VLC_COMPILE_SHARED=${VLC_COMPILE_SHARED:=0}
VLC_BUILD_EXTRA_CHECKS=${VLC_BUILD_EXTRA_CHECKS:=0}

diagnostic "setting MAKEFLAGS"
if [ -z "$MAKEFLAGS" ]; then
    UNAMES=$(uname -s)
    MAKEFLAGS=
    if which nproc >/dev/null; then
        MAKEFLAGS=-j$(nproc)
    elif [ "$UNAMES" = "Darwin" ] && which sysctl >/dev/null; then
        MAKEFLAGS=-j$(sysctl -n machdep.cpu.thread_count)
    fi
    export MAKEFLAGS;
fi

diagnostic "setting up dir paths"

OLD_PWD=$(pwd)
VLC_SRCPATH="$(dirname "$0")/../../../"
cd "$VLC_SRCPATH"
VLC_SRCPATH=$(pwd)
cd "$OLD_PWD"

diagnostic "vlc sources path: "
echo "$VLC_SRCPATH";

diagnostic "vlc tools: bootstrap"
cd "$VLC_SRCPATH"/extras/tools

# Force libtool build when compiling with shared library
if [ "$VLC_COMPILE_SHARED" -eq "1" ] && [ ! -d "libtool" ]; then
    NEEDED="$NEEDED libtool" ./bootstrap
else
    ./bootstrap
fi

diagnostic "vlc tools: make"
make

# update the PATH
export PATH=$VLC_SRCPATH/extras/tools/bin:$PATH

diagnostic "sdk tests: checking if autoconf supports emscripten"
# https://code.videolan.org/-/snippets/1283
for file in /usr/share/automake-*
do
    # This will pick the latest automake version
    AUTOMAKE_VERSION="$(echo "$file" | cut -d- -f2)"
done

diagnostic "using automake version: /usr/share/automake-$AUTOMAKE_VERSION"
if [ -f /usr/share/automake-"$AUTOMAKE_VERSION"/config.sub ]; then
    /usr/share/automake-"$AUTOMAKE_VERSION"/config.sub wasm32-unknown-emscripten
fi

mkdir -p "$VLC_SRCPATH"/contrib/contrib-emscripten
cd "$VLC_SRCPATH"/contrib/contrib-emscripten

diagnostic "vlc contribs: bootstrap"
../bootstrap --disable-disc --disable-sout --disable-net \
            --disable-a52 --disable-aom --disable-faad2 --disable-chromaprint \
            --disable-mad --disable-libmpeg2 --disable-nvcodec \
            --disable-tremor --disable-vpx --disable-theora \
            --disable-postproc --disable-gmp --disable-gcrypt \
            --disable-gpgerror --disable-fontconfig \
            --disable-asdcplib --disable-caca --disable-gettext \
            --disable-goom \
            --disable-lua --disable-luac --disable-sqlite \
            --disable-medialibrary --disable-mpcdec --disable-schroedinger \
            --disable-orc --disable-protobuf --disable-sidplay2 \
            --disable-spatialaudio --disable-speex \
            --disable-speexdsp --disable-taglib --disable-zvbi \
            --disable-rnnoise --disable-libaribcaption \
            --disable-ogg --disable-vorbis --disable-kate --disable-flac \
            --host=wasm32-unknown-emscripten

diagnostic "vlc contribs: make"
if [ "$VLC_USE_PREBUILT_CONTRIBS" -ne "0" ]; then
    diagnostic "vlc contribs: using prebuilt contribs"
    emmake make prebuilt PREBUILT_URL="$VLC_PREBUILT_CONTRIBS_URL" || PREBUILT_FAILED=yes
else
    PREBUILT_FAILED=yes
fi
emmake make list
if [ -n "$PREBUILT_FAILED" ]; then
    emmake make
    if [ "$GENERATE_ARCHIVE" -eq "1" ]; then
        diagnostic "vlc contribs: generating package"
        emmake make package
    fi
else
    emmake make tools
fi

diagnostic "starting libvlc build"
BUILD_PATH=$VLC_SRCPATH/$BUILDDIR_NAME
mkdir -p "$BUILD_PATH"

diagnostic "vlc build dir: "
echo "$BUILD_PATH";

if [ "$VLC_COMPILE_SHARED" -eq "1" ]; then
#enable shared, while disabling some of the incompatible dependencies
    SHARED_CONFIGURE_FLAGS="--enable-shared"
else
    SHARED_CONFIGURE_FLAGS="--disable-shared"
fi

if [ "$VLC_BUILD_EXTRA_CHECKS" -eq "1" ]; then
    EXTRA_CONFIGURE_FLAGS="--enable-extra-checks"
fi

cd "$BUILD_PATH"
if [ $BUILD_MODE -eq 1 ]; then
    diagnostic "libvlc build: bootstrap"
    ../bootstrap

    diagnostic "libvlc build: configure"
    # if_nameindex is not supported in emscripten
    # ie: not exposed from musl to src/library.js
    # the test in configure.ac fails because htons is not
    # in tools/deps_info.py

    # shm.h is a blacklisted module
    SANITIZER_OPTIONS=
    if [ ! -z "${VLC_USE_SANITIZER}" ]; then
        SANITIZER_OPTIONS="--with-sanitizer=${VLC_USE_SANITIZER}"
    fi
    emconfigure "$VLC_SRCPATH"/configure --host=wasm32-unknown-emscripten --enable-debug \
                        $SHARED_CONFIGURE_FLAGS $EXTRA_CONFIGURE_FLAGS --disable-vlc \
                        --enable-avcodec --enable-avformat --enable-swscale --enable-postproc \
                        --disable-sout --disable-vlm --disable-a52 --disable-xcb --disable-lua \
                        --disable-addonmanagermodules --disable-ssp --disable-nls \
                        --enable-gles2 \
                        --with-contrib="$VLC_SRCPATH"/contrib/wasm32-unknown-emscripten \
                        "${SANITIZER_OPTIONS}"
fi

if [ "$VLC_COMPILE_SHARED" -eq "1" ]; then
    sed -i "s|^postdeps_CXX=.*$|postdeps_CXX='-L/$(dirname $(which emcc))/cache/sysroot/lib/wasm32-emscripten -lal -lhtml5 -lbulkmemory -lstubs -lsockets-mt'|" config.status
fi


diagnostic "libvlc build: make"
emmake make

diagnostic "libvlc build: generate static modules entry points"
# start by deleting the previous version so that it's not overwritten
rm -f "$BUILD_PATH"/vlc-modules.c "$BUILD_PATH"/vlc-modules.bc

# create module list
echo "creating module list"
FUN_PROTOS=""
FUN_LIST=""

if [ "$VLC_COMPILE_SHARED" -ne "1" ]; then
for file in "$BUILD_PATH"/modules/.libs/*plugin.a
do
    ENTRY=$(get_entryname "$file")
    FUN_PROTOS="$FUN_PROTOS""VLC_ENTRY_FUNC($ENTRY);\n"
    FUN_LIST="$FUN_LIST""$ENTRY,\n"
done;
fi

printf "// This file is autogenerated
#include <stddef.h>
#include <vlc_plugin.h>
%b\n
const vlc_plugin_cb vlc_static_modules[] = {
%bNULL
};\n" "$FUN_PROTOS" "$FUN_LIST" \
       > "$BUILD_PATH"/vlc-modules.c

diagnostic "vlc static modules: compiling static modules entry points"
# compile vlc-modules.c
SANITIZERS=
if echo "${VLC_USE_SANITIZER}" | grep address > /dev/null; then
SANITIZERS="$SANITIZERS -fsanitize=address -fsanitize-address-use-after-scope -fno-omit-frame-pointer"
fi

emcc $SANITIZERS -pthread -c -emit-llvm "$BUILD_PATH"/vlc-modules.c -I"$VLC_SRCPATH"/include -I"$BUILD_PATH"  -o "$BUILD_PATH"/vlc-modules.bc

echo "VLC for wasm32-unknown-emscripten built!"
