#!/bin/sh

SCRIPTDIR=$(dirname "$0")
. "$SCRIPTDIR/env.build.sh" "none"

CFLAGS=${CFLAGS}
LDFLAGS=${LDFLAGS}

OPTIONS="
        --prefix=/
        --enable-macosx
        --enable-merge-ffmpeg
        --enable-osx-notifications
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
        --disable-vnc
        --with-macosx-version-min=$MINIMAL_OSX_VERSION
        --without-x
"

export CFLAGS
export LDFLAGS

vlcSetSymbolEnvironment

sh "$(dirname $0)"/../../../configure ${OPTIONS} "$@"
