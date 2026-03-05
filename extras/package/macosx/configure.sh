#!/bin/sh

CFLAGS=${CFLAGS}
LDFLAGS=${LDFLAGS}

OPTIONS="
        --prefix=`pwd`/vlc_install_dir
        --enable-macosx
        --enable-merge-ffmpeg
        --enable-osx-notifications
        --enable-faad
        --enable-flac
        --enable-theora
        --enable-shout
        --enable-ncurses
        --enable-twolame
        --enable-realrtsp
        --enable-libass
        --enable-macosx-avfoundation
        --disable-skins2
        --disable-xcb
        --disable-caca
        --disable-pulse
        --disable-sdl-image
        --disable-vnc
        --with-macosx-version-min=10.7
"

export CFLAGS
export LDFLAGS

sh "$(dirname $0)"/../../../configure ${OPTIONS} "$@"
