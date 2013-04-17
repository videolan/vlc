#!/bin/sh

CFLAGS=${CFLAGS}
LDFLAGS=${LDFLAGS}

case "${ARCH}" in
    x86_64*)
        CFLAGS="${CFLAGS} -m64 -march=core2 -mtune=core2"
        LDFLAGS="${LDFLAGS} -m64"
        ;;
    ppc)
        CFLAGS="${CFLAGS} -arch ppc -mtune=G4"
        LDFLAGS="${LDFLAGS} -arch ppc"
        ;;
    *x86*)
        CFLAGS="${CFLAGS} -m32 -march=prescott -mtune=generic"
        LDFLAGS="${LDFLAGS} -m32"
        ;;
esac

OPTIONS="
        --prefix=`pwd`/vlc_install_dir
        --enable-macosx
        --enable-merge-ffmpeg
        --enable-growl
        --enable-faad
        --enable-flac
        --enable-theora
        --enable-shout
        --enable-ncurses
        --enable-twolame
        --enable-realrtsp
        --enable-libass
        --enable-macosx-audio
        --enable-macosx-eyetv
        --enable-macosx-qtkit
        --enable-macosx-avfoundation
        --enable-macosx-vout
        --disable-skins2
        --disable-xcb
        --disable-caca
        --disable-sdl
        --disable-samplerate
        --disable-macosx-dialog-provider
"

export CFLAGS
export LDFLAGS

sh "$(dirname $0)"/../../../configure ${OPTIONS} $*
