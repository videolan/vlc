[ -z $CONTRIB_DIR ] && export CONTRIB_DIR=/usr/win32

  $CONFIGURE \
      --with-contrib \
      --enable-update-check \
      --enable-lua \
      --enable-faad \
      --enable-flac \
      --enable-theora \
      --enable-twolame \
      --enable-quicktime \
      --enable-real \
      --enable-avcodec  --enable-merge-ffmpeg \
      --enable-dca \
      --enable-mpc \
      --enable-libass \
      --enable-x264 \
      --enable-schroedinger \
      --enable-realrtsp \
      --enable-live555 \
      --enable-dvdread --with-dvdnav-config-path=$CONTRIB_DIR/bin \
      --enable-shout \
      --enable-goom \
      --enable-caca \
      --enable-portaudio \
      --enable-sdl \
      --enable-qt4 \
      --enable-sse --enable-mmx \
      --enable-libcddb \
      --enable-zvbi --disable-telx \
      --enable-sqlite \
      --enable-media-library \
      --disable-dvb \
      --disable-dirac \
      --enable-peflags \
      $CONFIGOPTS
