lib_LTLIBRARIES = libvlcpp.la

libvlcpp_la_SOURCES =   \
    audio.cpp           \
    audio.hpp           \
    exception.cpp       \
    exception.hpp       \
    libvlc.cpp          \
    libvlc.hpp          \
    media.cpp           \
    media.hpp           \
    media_player.cpp    \
    media_player.hpp    \
    video.cpp           \
    video.hpp

libvlcpp_la_CXXFLAGS = @libvlc_CFLAGS@
libvlcpp_la_LDFLAGS = @libvlc_LIBS@ -version-info 1:0:0
library_includedir=$(includedir)/libvlcpp
library_include_HEADERS = exception.hpp libvlc.hpp media.hpp media_player.hpp video.hpp

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libvlcpp.pc
