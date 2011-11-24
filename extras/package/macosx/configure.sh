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
        --enable-vcdx
        --enable-caca
        --enable-ncurses
        --enable-twolame
        --enable-realrtsp
        --enable-libass
        --disable-skins2
        --disable-xcb
"

sh "$(dirname $0)"/../../../configure ${OPTIONS} $*
