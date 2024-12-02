#!/bin/sh

OPTIONS="
      --enable-lua
      --enable-flac
      --enable-theora
      --enable-avcodec --enable-merge-ffmpeg
      --enable-libass
      --enable-schroedinger
      --enable-shout
      --enable-goom
      --enable-sse
      --enable-libcddb
      --enable-zvbi --disable-telx"

sh "$(dirname $0)"/../../../configure ${OPTIONS}  "$@"
