#!/bin/sh

OPTIONS="
        --prefix=`pwd`/vlc_install_dir
        --enable-macosx
        --enable-merge-ffmpeg
        --enable-growl
        --enable-faad
        --enable-flac
        --enable-theora
        --enable-shout
        --enable-caca
        --enable-ncurses
        --enable-twolame
        --enable-realrtsp
        --enable-libass
        --enable-macosx-audio
        --enable-macosx-dialog-provider
        --enable-macosx-eyetv
        --enable-macosx-qtkit
        --enable-macosx-vout
        --disable-skins2
        --disable-xcb
        --disable-sdl
"

sh "$(dirname $0)"/../../../configure ${OPTIONS} $*
