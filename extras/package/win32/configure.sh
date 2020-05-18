#!/bin/sh

OPTIONS="
      --enable-update-check
      --enable-lua
      --enable-flac
      --enable-theora
      --enable-avcodec --enable-merge-ffmpeg
      --enable-dca
      --enable-libass
      --enable-schroedinger
      --enable-live555
      --enable-shout
      --enable-goom
      --enable-sse --enable-mmx
      --enable-libcddb
      --enable-zvbi --disable-telx
      --enable-nls"

sh "$(dirname $0)"/../../../configure ${OPTIONS}  "$@"
