#!/bin/sh

OPTIONS="
      --enable-update-check
      --enable-lua
      --enable-faad
      --enable-flac
      --enable-theora
      --enable-twolame
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
      --enable-qt
      --enable-skins2
      --enable-sse --enable-mmx
      --enable-libcddb
      --enable-zvbi --disable-telx
      --enable-nls"

sh "$(dirname $0)"/../../../configure ${OPTIONS}  "$@"
