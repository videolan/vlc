#!/usr/bin/env bash
sed -i -e "s,@GIT_HASH@,$(git rev-parse HEAD),g" extras/package/flatpak/vlc-appdata-manifest-url.patch
cd extras/tools
./bootstrap
make fetch
cd ../..
tar cf extras/package/flatpak/tools.tar extras/tools
# Prefetch all contrib tarballs and create a contrib source archive
mkdir -p contrib/linux
cd contrib/linux
../bootstrap --enable-ffmpeg --enable-dav1d --enable-fluidlite
make fetch
cd ../..
tar cf extras/package/flatpak/contrib.tar contrib
rm -rf contrib/tarballs
# Create VLC source archive
./bootstrap
BUILDCC=/usr/bin/gcc ./configure --disable-lua --disable-avcodec
make dist
mv vlc-*.tar.xz extras/package/flatpak/vlc.tar.xz
