#!/bin/sh

OPTIONS="
      --enable-update-check
      --enable-lua
      --enable-faad
      --enable-flac
      --enable-theora
      --enable-twolame
      --enable-quicktime
      --enable-avcodec --enable-merge-ffmpeg
      --enable-dca
      --enable-mpc
      --enable-libass
      --enable-x264
      --enable-schroedinger
      --enable-realrtsp
      --enable-live555
      --enable-dvdread
      --enable-shout
      --enable-goom
      --enable-caca
      --disable-sdl
      --enable-qt
      --enable-skins2
      --enable-sse --enable-mmx
      --enable-libcddb
      --enable-zvbi --disable-telx
      --enable-nls
      --disable-dirac"

if gcc -v 2>/dev/null -a echo | gcc -mno-cygwin -E -2>/dev/null 2>&1
then
    echo Cygwin detected, adjusting options
    export CC="gcc -mno-cygwin"
    export CXX="g++ -mno-cygwin"
    OPTIONS="${OPTIONS} --disable-taglib --disable-mkv"
fi

sh "$(dirname $0)"/../../../configure ${OPTIONS} $*
