PATH="/usr/win32/bin:$PATH" \
PKG_CONFIG_LIBDIR=/usr/win32/lib/pkgconfig \
CPPFLAGS="-I/usr/win32/include -I/usr/win32/include/ebml" \
LDFLAGS="-L/usr/win32/lib" \
CC=i586-mingw32msvc-gcc CXX=i586-mingw32msvc-g++ \
CONFIG="./configure --host=i586-mingw32msvc --build=i386-linux --enable-mkv --enable-debug" \
sh extras/package/win32/configure-common.sh
