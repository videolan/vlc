lib_LTLIBRARIES = libvlcpp.la

libvlcpp_la_SOURCES =   \
    exception.cpp       \
    exception.hpp       \
    libvlc.cpp          \
    libvlc.hpp          \
    media.cpp           \
    media.hpp           \
    media_player.cpp    \
    media_player.hpp

libvlcpp_la_CXXFLAGS = @libvlc_CFLAGS@
libvlcpp_la_LDFLAGS = @libvlc_LIBS@
library_includedir=$(includedir)/libvlcpp
library_include_HEADERS = exception.hpp libvlc.hpp media.hpp media_player.hpp

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libvlcpp.pc
