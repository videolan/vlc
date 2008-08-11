PATH="/usr/win32/bin:$PATH" \
PKG_CONFIG_LIBDIR=/usr/win32/lib/pkgconfig \
CPPFLAGS="-I/usr/win32/include -I/usr/win32/include/ebml" \
LDFLAGS="-L/usr/win32/lib" \
CC="gcc -mno-cygwin" CXX="g++ -mno-cygwin" \
CONFIG="./configure --host=i686-pc-mingw32 --disable-taglib --disable-mkv" \
sh extras/package/win32/configure-common.sh
