if [ -z $CONTRIBS ]
then
	CONTRIBS=/usr/win32
fi

      $CONFIG \
      --without-contrib \
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
      --enable-live555 --with-live555-tree=$CONTRIBS/live.com \
      --enable-dvdread --with-dvdnav-config-path=$CONTRIBS/bin \
      --enable-shout \
      --enable-goom \
      --enable-caca \
      --enable-portaudio \
      --enable-sdl --with-sdl-config-path=$CONTRIBS/bin \
      --enable-qt4 \
      --enable-mozilla --with-mozilla-sdk-path=$CONTRIBS/gecko-sdk \
      --enable-activex \
      --enable-sse --enable-mmx \
      --enable-libcddb \
      --enable-zvbi --disable-telx \
      --disable-dvb \
      --disable-sqlite \
      --enable-peflags
