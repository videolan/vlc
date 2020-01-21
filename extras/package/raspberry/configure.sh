#!/bin/sh

OPTIONS="
      --enable-mmal
      --enable-lua
      --enable-faad
      --enable-flac
      --enable-theora
      --enable-avcodec --enable-merge-ffmpeg
      --enable-dca
      --enable-libass
      --enable-schroedinger
      --enable-live555
      --enable-dvdread
      --enable-shout
      --enable-goom
      --enable-libcddb
      --enable-zvbi --disable-telx
      --enable-nls
"

sh "$(dirname $0)"/../../../configure ${OPTIONS}  "$@"
