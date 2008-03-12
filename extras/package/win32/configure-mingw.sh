PATH="/usr/win32/bin:$PATH" \
PKG_CONFIG_LIBDIR=/usr/win32/lib/pkgconfig \
CPPFLAGS="-I/usr/win32/include -I/usr/win32/include/ebml" \
LDFLAGS="-L/usr/win32/lib" \
CC=i586-mingw32msvc-gcc CXX=i586-mingw32msvc-g++ \
      ./configure --host=i586-mingw32msvc --build=i386-linux \
      --enable-release \
      --without-contrib \
      --enable-nls \
      --enable-shared-libvlc \
      --enable-update-check \
      --enable-lua \
      --enable-faad \
      --enable-flac \
      --enable-theora \
      --enable-twolame \
      --enable-quicktime \
      --enable-real \
      --enable-dirac \
      --enable-realrtsp \
      --enable-ffmpeg --with-ffmpeg-mp3lame --with-ffmpeg-faac \
      --with-ffmpeg-config-path=/usr/win32/bin --with-ffmpeg-zlib \
      --enable-livedotcom --with-livedotcom-tree=/usr/win32/live.com \
      --enable-dca \
      --enable-mkv \
      --enable-x264 \
      --enable-dvdread --with-dvdnav-config-path=/usr/win32/bin \
      --enable-shout \
      --enable-goom \
      --enable-caca --with-caca-config-path=/usr/win32/bin \
      --enable-portaudio \
      --enable-sdl --with-sdl-config-path=/usr/win32/bin \
      --enable-qt4 \
      --enable-wxwidgets --with-wx-config-path=/usr/win32/lib/wx/config \
      --with-freetype-config-path=/usr/win32/bin \
      --with-fribidi-config-path=/usr/win32/bin \
      --with-xml2-config-path=/usr/win32/bin \
      --enable-mozilla --with-mozilla-sdk-path=/usr/win32/gecko-sdk \
      --enable-activex \
      --disable-gnomevfs --disable-hal --disable-gtk \
      --disable-cddax --disable-vcdx \

