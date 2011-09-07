[ -z $CONTRIB_DIR ] && export CONTRIB_DIR=/usr/win32

  $CONFIGURE \
      --with-contrib="$CONTRIB_DIR" \
      --enable-update-check \
      --enable-lua \
      --enable-faad \
      --enable-flac \
      --enable-theora \
      --enable-twolame \
      --enable-quicktime \
      --enable-real \
      --enable-avcodec --enable-merge-ffmpeg \
      --enable-dca \
      --enable-mpc \
      --enable-libass \
      --enable-x264 \
      --enable-schroedinger \
      --enable-realrtsp \
      --enable-live555 \
      --enable-dvdread \
      --enable-shout \
      --enable-goom \
      --enable-caca \
      --disable-portaudio \
      --disable-sdl \
      --enable-qt4 \
      --enable-skins2 \
      --enable-sse --enable-mmx \
      --enable-libcddb \
      --enable-zvbi --disable-telx \
      --enable-sqlite \
      --disable-dirac \
      --with-peflags \
      $CONFIGOPTS
