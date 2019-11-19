#!/bin/sh

OPTIONS="
      --enable-shared
      --enable-run-as-root
      --enable-lua
      --enable-live555
      --enable-dvdread
      --enable-dvdnav
      --enable-sftp
      --enable-vcd
      --enable-libcddb
      --enable-dvbpsi
      --enable-ogg
      --enable-mad
      --enable-merge-ffmpeg
      --enable-avcodec
      --enable-avformat
      --enable-swscale
      --enable-postproc
      --enable-a52
      --enable-flac
      --enable-libmpeg2
      --enable-vorbis
      --enable-png
      --enable-x264
      --enable-libass
      --disable-xcb
      --enable-freetype
      --enable-fribidi
      --enable-fontconfig
      --enable-kva
      --enable-kai
      --enable-qt
      --enable-skins2
      --enable-libxml2
      --enable-libgcrypt
      --enable-gnutls
      --enable-vlc
"

export ARCHFLAGS=${ARCHFLAGS-"-march=i686"}
export CFLAGS="${CFLAGS} ${ARCHFLAGS} -std=gnu11"
export CXXFLAGS="${CXXFLAGS} ${ARCHFLAGS} -std=gnu++11"
export BUILDCC="gcc -std=gnu11"

sh "$(dirname $0)"/../../../configure ${OPTIONS} "$@"
