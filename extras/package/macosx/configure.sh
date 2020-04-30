#!/bin/sh

SCRIPTDIR=$(dirname "$0")
. "$SCRIPTDIR/env.build.sh" "none"

CFLAGS=${CFLAGS}
LDFLAGS=${LDFLAGS}

case "${ARCH}" in
    x86_64*)
        CFLAGS="${CFLAGS} -m64 -march=core2 -mtune=core2"
        LDFLAGS="${LDFLAGS} -m64"
        ;;
    *x86*)
        CFLAGS="${CFLAGS} -m32 -march=prescott -mtune=generic"
        LDFLAGS="${LDFLAGS} -m32"
        ;;
esac

OPTIONS="
        --prefix=/
        --enable-macosx
        --enable-merge-ffmpeg
        --enable-osx-notifications
        --enable-faad
        --enable-flac
        --enable-theora
        --enable-shout
        --enable-ncurses
        --enable-twolame
        --enable-libass
        --enable-macosx-avfoundation
        --disable-skins2
        --disable-xcb
        --disable-caca
        --disable-pulse
        --disable-sdl-image
        --disable-vnc
        --with-macosx-version-min=10.11
"

export CFLAGS
export LDFLAGS

vlcSetSymbolEnvironment

sh "$(dirname $0)"/../../../configure ${OPTIONS} "$@"
