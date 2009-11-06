#***************************************************************************
# src/Makefile : Dearchive and compile all files necessary
# ***************************************************************************
# Copyright (C) 2003 - 2009 the VideoLAN team
# $Id$
#
# Authors: Christophe Massiot <massiot@via.ecp.fr>
#          Derk-Jan Hartman <hartman at videolan dot org>
#          Eric Petit <titer@m0k.org>
#          Felix Paul Kühne <fkuehne at videolan dot org>
#          Christophe Mutricy <xtophe AT xtelevision.com>
#          Gildas Bazin <gbazin at videolan dot org>
#          Damien Fouilleul <damienf at videolan dot org>
#          Jean-Baptiste Kempf <jb at videolan dot org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
# ***************************************************************************

include ../config.mak
include ./packages.mak

# ***************************************************************************
# Set a clean environment
# ***************************************************************************

export PATH := $(PREFIX)/bin:$(EXTRA_PATH):$(PATH)
export PKG_CONFIG_PATH
export PKG_CONFIG_LIBDIR = $(PREFIX)/lib/pkgconfig
export MACOSX_DEPLOYMENT_TARGET = $(SDK_TARGET)
export LIBRARY_PATH := $(PREFIX)/lib:$(LIBRARY_PATH)
export CFLAGS = -I$(PREFIX)/include $(EXTRA_CFLAGS) $(EXTRA_CPPFLAGS)
export CPPFLAGS = -I$(PREFIX)/include $(EXTRA_CFLAGS) $(EXTRA_CPPFLAGS)
export CXXFLAGS = -I$(PREFIX)/include $(EXTRA_CFLAGS) $(EXTRA_CPPFLAGS)
export LDFLAGS = -L$(PREFIX)/lib $(EXTRA_LDFLAGS)
ifdef HAVE_DARWIN_OS
export CC
export CXX
export LD
endif

# ***************************************************************************
# Cross compilation variables
# We'll usually use --host=<platform>, except for a few libraries which
# don't handle it (gotta set CC/CXX/etc), and obviously FFmpeg has its own
# way of doing it... ;)
# ***************************************************************************

ifneq ($(CC),)
HOSTCC+= CC="$(CC)"
endif
ifneq ($(CXX),)
HOSTCC+= CXX="$(CXX)"
endif
ifneq ($(LD),)
HOSTCC+= LD="$(LD)"
endif
ifneq ($(RANLIB),)
HOSTCC+= RANLIB="$(RANLIB)"
endif
ifneq ($(AR),)
HOSTCC+= AR="$(AR)"
endif
ifneq ($(STRIP),)
HOSTCC+= STRIP="$(STRIP)"
endif


# Define ranlib on non-cross compilation setups
ifeq ($(RANLIB),)
RANLIB=ranlib
endif

# For libebml/libmatroska. Grrr.
ifneq ($(AR),)
HOSTCC2=$(HOSTCC) AR="$(AR) rcvu"
else
HOSTCC2=$(HOSTCC)
endif

FFMPEGCONF=

# cross compiling
#This should be inside the if block but some config scripts are buggy
HOSTCONF=--target=$(HOST) --host=$(HOST) --build=$(BUILD) --program-prefix=""
ifneq ($(BUILD),$(HOST))
    #
    # Compiling for MinGW under Cygwin could be deemed as cross compiling
    # unfortunately there isn't a complete separate GCC toolchain for MinGW under Cygwin
    #
    ifndef HAVE_CYGWIN
        # We are REALLY cross compiling
        FFMPEGCONF+=--cross-prefix=$(HOST)- --enable-cross-compile
        X264CONF=--host=$(HOST)
        PTHREADSCONF="CROSS=$(HOST)-"
    else
        # We are compiling for MinGW on Cygwin
        PTHREADSCONF=$(HOSTCC)
    endif
endif

#
# Special target-dependant options
#
ifdef HAVE_WIN32
HOSTCONF+= --without-pic --disable-shared --enable-msw --disable-dependency-tracking
FFMPEGCONF+= --target-os=mingw32 --enable-memalign-hack
ifdef HAVE_WIN64
FFMPEGCONF+= --cpu=athlon64 --arch=x86_64
else
FFMPEGCONF+= --cpu=i686
endif
endif

ifdef HAVE_WINCE
HOSTCONF+= --without-pic --disable-shared
FFMPEGCONF+= --enable-mingwce --cpu=armv4l --disable-encoders --disable-muxers --disable-mpegaudio-hp --disable-decoder=snow --disable-decoder=vc9 --disable-decoder=wmv3 --disable-decoder=vorbis --disable-decoder=dvdsub --disable-decoder=dvbsub --disable-protocols
endif

ifdef HAVE_UCLIBC
ifdef HAVE_BIGENDIAN
FFMPEGCONF+= --arch=armeb --enable-armv5te --enable-iwmmxt
else
FFMPEGCONF+= --arch=armv4l
endif
FFMPEGCONF+= --enable-small --disable-mpegaudio-hp
FFMPEG_CFLAGS += -DHAVE_LRINTF --std=c99
else
FFMPEGCONF+= --enable-libmp3lame --enable-libgsm
endif

ifdef HAVE_DARWIN_OS_ON_INTEL
FFMPEGCONF += --enable-memalign-hack
endif

ifdef HAVE_DARWIN_OS
X264CONF=--host=$(HOST)
X264CONF += --enable-pic
FFMPEGCONF += --cc=$(CC)
FFMPEGCONF += --arch=$(ARCH)
ifdef HAVE_DARWIN_64
FFMPEGCONF += --cpu=core2
X264CONF+=--host=x86_64-apple-darwin10
endif
ifdef HAVE_DARWIN_OS_ON_INTEL
FFMPEG_CFLAGS += -DHAVE_LRINTF
endif
endif

ifdef HAVE_AMR
FFMPEGCONF+= --enable-libamr-nb --enable-libamr-wb --enable-nonfree
endif

ifdef HAVE_LINUX
ifdef HAVE_MAEMO
ifneq ($(filter -m%=cortex-a8, $(EXTRA_CFLAGS)),)
FFMPEGCONF += --disable-runtime-cpudetect --enable-neon --cpu=cortex-a8
endif
# Really, this could be done on all Linux platforms, not just Maemo.
# Installing statically-linked VLC plugins is so much simpler.
HOSTCONF += --with-pic --disable-shared
endif
FFMPEGCONF += --enable-pic
X264CONF += --enable-pic
endif

ifdef HAVE_ISA_THUMB
NOTHUMB ?= -mno-thumb
endif

DATE=`date +%Y-%m-%d`

# ***************************************************************************
# Standard rules
# ***************************************************************************
# Generated by ./bootstrap from default configuration in src/Distributions
#
include ../distro.mak

FORCE:

# ***************************************************************************
# Useful macros
# ***************************************************************************

define EXTRACT_GZ
	rm -rf $@ || true
	gunzip -c $< | tar xf - --exclude='[*?:<>\|]'
	mv $(patsubst %.tar.gz,%,$(patsubst %.tgz,%,$(notdir $<))) $@ || true
	touch $@
endef

define EXTRACT_BZ2
	rm -rf $@ || true
	bunzip2 -c $< | tar xf - --exclude='[*?:<>\|]'
	mv $(patsubst %.tar.bz2,%,$(notdir $<)) $@ || true
	touch $@
endef

define EXTRACT_ZIP
	rm -rf $@ || true
	unzip $<
	mv $(patsubst %.zip,%,$(notdir $<)) $@ || true
	touch $@
endef

### Darwin-specific ###
# These macros prepare the dynamic libraries for inclusion in the Mac OS X
# bundle. For instance if you're building a library named libtoto.dylib,
# which depends on the contrib library libtata.dylib, you should have the
# following entry :
# .toto: toto_directory .tata
#	cd $< ; ./configure --prefix=$(PREFIX)
#	$(MAKE) -C $<
#	$(MAKE) -C $< install
#	$(INSTALL_NAME)
#	touch $@

# ***************************************************************************
# autoconf
# ***************************************************************************

autoconf-$(AUTOCONF_VERSION).tar.bz2:
	$(WGET) $(AUTOCONF_URL)

autoconf: autoconf-$(AUTOCONF_VERSION).tar.bz2
	$(EXTRACT_BZ2)

.autoconf: autoconf
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
	touch $@

CLEAN_FILE += .autoconf
CLEAN_PKG += autoconf
DISTCLEAN_PKG += autoconf-$(AUTOCONF_VERSION).tar.bz2

# ***************************************************************************
# gnumake
# ***************************************************************************

make-$(GNUMAKE_VERSION).tar.bz2:
	$(WGET) $(GNUMAKE_URL)

gnumake: make-$(GNUMAKE_VERSION).tar.bz2
	$(EXTRACT_BZ2)

.gnumake: gnumake
ifdef HAVE_DARWIN_OS
ifndef HAVE_DARWIN_9
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
endif
endif
	touch $@

CLEAN_FILE += .gnumake
CLEAN_PKG += gnumake
DISTCLEAN_PKG += make-$(GNUMAKE_VERSION).tar.bz2

# ***************************************************************************
# CMake
# ***************************************************************************
cmake-$(CMAKE_VERSION).tar.gz:
	$(WGET) $(CMAKE_URL)

cmake: cmake-$(CMAKE_VERSION).tar.gz
	$(EXTRACT_GZ)

#cmake:
#	cvs -d :pserver:anonymous:cmake@www.cmake.org:/cvsroot/CMake co -d cmake CMake

.cmake: cmake
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
	touch $@

CLEAN_FILE += .cmake
CLEAN_PKG += cmake

# ***************************************************************************
# libtool
# ***************************************************************************

ifdef HAVE_DARWIN_OS
libtool-$(LIBTOOL_VERSION).tar.gz:
	$(WGET) $(LIBTOOL_URL)

libtool: libtool-$(LIBTOOL_VERSION).tar.gz
	$(EXTRACT_GZ)

.libtool: libtool
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
	ln -sf libtool $(PREFIX)/bin/glibtool
	ln -sf libtoolize $(PREFIX)/bin/glibtoolize
	touch $@

CLEAN_PKG += libtool
DISTCLEAN_PKG += libtool-$(LIBTOOL_VERSION).tar.gz
CLEAN_FILE += .libtool

endif

# ***************************************************************************
# automake
# ***************************************************************************

automake-$(AUTOMAKE_VERSION).tar.gz:
	$(WGET) $(AUTOMAKE_URL)

automake: automake-$(AUTOMAKE_VERSION).tar.gz
	$(EXTRACT_GZ)

.automake: automake
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
	touch $@

CLEAN_FILE += .automake
CLEAN_PKG += automake
DISTCLEAN_PKG += automake-$(AUTOMAKE_VERSION).tar.gz

# ***************************************************************************
# pkgconfig
# ***************************************************************************

pkg-config-$(PKGCFG_VERSION).tar.gz:
	$(WGET) $(PKGCFG_URL)

pkgconfig: pkg-config-$(PKGCFG_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -d pkgconfig/glib-1.2.8 -p 1 < Patches/glib.patch
	patch -d pkgconfig -p0 < Patches/pkgcfg.patch
	(cd $@; autoconf)

.pkgcfg: pkgconfig
	(cd pkgconfig; ./configure --prefix=$(PREFIX) --disable-shared --enable-static && make && make install)
	touch $@

CLEAN_FILE += .pkgcfg
CLEAN_PKG += pkgconfig
DISTCLEAN_PKG += pkg-config-$(PKGCFG_VERSION).tar.gz

# ***************************************************************************
# gettext
# ***************************************************************************

gettext-$(GETTEXT_VERSION).tar.gz:
	$(WGET) $(GETTEXT_URL)

gettext: gettext-$(GETTEXT_VERSION).tar.gz
	$(EXTRACT_GZ)
ifdef HAVE_WIN32
	patch -p0 < Patches/gettext-win32.patch
endif
ifdef HAVE_DARWIN_OS
	patch -p0 < Patches/gettext-macosx.patch
endif

.intl: gettext .iconv
ifdef HAVE_WIN32
	( cd $< && $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-relocatable --with-iconv-prefix=$(PREFIX) --disable-java --disable-native-java)
else
	( cd $< && $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-java --disable-native-java --without-emacs)
endif
ifneq ($(HOST),$(BUILD))
  ifndef HAVE_CYGWIN
    # We'll use the installed gettext and only need to cross-compile libintl, also build autopoint and gettextsize tools need for VLC bootstrap
	( cd $< && make -C gettext-runtime/intl && patch -p1 < ../Patches/libintl.patch && make -C gettext-runtime/intl install && make -C gettext-tools/misc install )
  else
    # We are compiling for MinGW on Cygwin -- build the full current gettext
	( cd $< && make && patch -p1 < ../Patches/libintl.patch && make install )
  endif
else
# Build and install the whole gettext
	( cd $< && make && patch -p1 < ../Patches/libintl.patch && make install )
endif
# Work around another non-sense of autoconf.
ifdef HAVE_WIN32
	(cd ../include; sed -i.orig '311 c #if 0' libintl.h)
endif
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .intl
CLEAN_PKG += gettext
DISTCLEAN_PKG += gettext-$(GETTEXT_VERSION).tar.gz

# ***************************************************************************
# libiconv
# ***************************************************************************

libiconv-$(LIBICONV_VERSION).tar.gz:
	$(WGET) $(LIBICONV_URL)

libiconv: libiconv-$(LIBICONV_VERSION).tar.gz
	$(EXTRACT_GZ)
ifdef HAVE_WIN64
	patch -p0 < Patches/libiconv-win64.patch
endif

libiconv-snowleopard.tar.bz2:
	$(WGET) $(LIBICONVMAC_URL)

libiconv-snowleopard: libiconv-snowleopard.tar.bz2
	$(EXTRACT_BZ2)

ifdef HAVE_DARWIN_OS
ifdef HAVE_DARWIN_10
.iconv: libiconv-snowleopard
	(cd libiconv-snowleopard && cp libiconv.* ../../lib/)
	touch $@
else
.iconv:
	touch $@
endif
else
.iconv: libiconv
	(cd libiconv; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-nls && make && make install)
	$(INSTALL_NAME)
	touch $@
endif

CLEAN_FILE += .iconv
CLEAN_PKG += libiconv
DISTCLEAN_PKG += libiconv-$(LIBICONV_VERSION).tar.gz

# ***************************************************************************
# fontconfig
# ***************************************************************************

fontconfig-$(FONTCONFIG_VERSION).tar.gz:
	$(WGET) $(FONTCONFIG_URL)

fontconfig: fontconfig-$(FONTCONFIG_VERSION).tar.gz Patches/fontconfig.patch
	$(EXTRACT_GZ)
	patch -p0 < Patches/fontconfig-nodocs.patch
ifdef HAVE_WIN32
	patch -p0 < Patches/fontconfig.patch
	patch -p0 < Patches/fontconfig-noxml2.patch
endif
	(cd $@; autoreconf -ivf)

.fontconfig: fontconfig .xml .freetype
ifdef HAVE_WIN32
  ifdef HAVE_CYGWIN
	(cd $<;  ./configure --target=$(HOST) --disable-pic --disable-shared --disable-docs --with-arch=i686 --prefix=$(PREFIX) --with-freetype-config=$(PREFIX)/bin/freetype-config --enable-libxml2 && make && make install)
  else
	(cd $<; $(HOSTCC)  ./configure $(HOSTCONF) --with-arch=i686 --prefix=$(PREFIX) --with-freetype-config=$(PREFIX)/bin/freetype-config --disable-libxml2 --disable-docs && make && make install)
  endif
else
  ifdef HAVE_DARWIN_OS
	(cd $<; $(HOSTCC) LIBXML2_CFLAGS=`xml2-config --cflags` LIBXML2_LIBS=`xml2-config --libs` ./configure $(HOSTCONF) --with-cache-dir=/usr/X11/var/cache/fontconfig --with-confdir=/usr/X11/lib/X11/fonts --with-default-fonts=/System/Library/Fonts --with-add-fonts=/Library/Fonts,~/Library/Fonts --prefix=$(PREFIX) --with-freetype-config=$(PREFIX)/bin/freetype-config --with-arch=$(ARCH) --enable-libxml2 --disable-docs && make && make install-exec && (cd fontconfig ; make install-data) && cp fontconfig.pc $(PKG_CONFIG_LIBDIR))
  else
	(cd $<; $(HOSTCC) LIBXML2_CFLAGS=`$(PREFIX)/bin/xml2-config --cflags` ./configure $(HOSTCONF) --prefix=$(PREFIX) --with-freetype-config=$(PREFIX)/bin/freetype-config --enable-libxml2 --disable-docs && make && make install)
  endif
endif
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .fontconfig
CLEAN_PKG += fontconfig
DISTCLEAN_PKG += fontconfig-$(FONTCONFIG_VERSION).tar.gz

# ***************************************************************************
# freetype2
# ***************************************************************************

freetype-$(FREETYPE2_VERSION).tar.gz:
	$(WGET) $(FREETYPE2_URL)

freetype2: freetype-$(FREETYPE2_VERSION).tar.gz
	$(EXTRACT_GZ)

.freetype: freetype2
ifdef HAVE_CYGWIN
	(cd $<; ./configure --target=$(HOST) --disable-pic --disable-shared --prefix=$(PREFIX) && make && make install)
else
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
endif
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .freetype
CLEAN_PKG += freetype2
DISTCLEAN_PKG += freetype-$(FREETYPE2_VERSION).tar.gz

# ***************************************************************************
# fribidi
# ***************************************************************************

fribidi-$(FRIBIDI_VERSION).tar.gz:
	$(WGET) $(FRIBIDI_URL)

fribidi: fribidi-$(FRIBIDI_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p0 < Patches/fribidi.patch
	( cd $@; rm -f configure; ./bootstrap)

.fribidi: fribidi .iconv
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX)  && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .fribidi
CLEAN_PKG += fribidi
DISTCLEAN_PKG += fribidi-$(FRIBIDI_VERSION).tar.gz

# ***************************************************************************
# liba52
# ***************************************************************************

a52dec-$(A52DEC_VERSION).tar.gz:
	$(WGET) $(A52DEC_URL)

a52dec: a52dec-$(A52DEC_VERSION).tar.gz
	$(EXTRACT_GZ)
ifeq ($(ARCH),armel)
	(cd $@ ; patch -p0 < ../Patches/liba52-fixed.diff)
endif

.a52: a52dec
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && cd liba52 && make && make install && cd ../include && make && make install)
	touch $@

CLEAN_FILE += .a52
CLEAN_PKG += a52dec
DISTCLEAN_PKG += a52dec-$(A52DEC_VERSION).tar.gz

# ***************************************************************************
# mpeg2dec
# ***************************************************************************

libmpeg2-$(LIBMPEG2_VERSION).tar.gz:
	$(WGET) $(LIBMPEG2_URL)

libmpeg2: libmpeg2-$(LIBMPEG2_VERSION).tar.gz
	$(EXTRACT_GZ)
	cd libmpeg2 && patch -p0 < ../Patches/libmpeg2-mc-neon.patch
	cd libmpeg2 && ./bootstrap

.mpeg2: libmpeg2
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --without-x --disable-sdl && cd libmpeg2 && make && make install && cd ../include && make && make install)
	touch $@

CLEAN_FILE += .mpeg2
CLEAN_PKG += libmpeg2
DISTCLEAN_PKG += libmpeg2-$(LIBMPEG2_VERSION).tar.gz

# ***************************************************************************
# libid3tag
# ***************************************************************************

libid3tag-$(LIBID3TAG_VERSION).tar.gz:
	$(WGET) $(LIBID3TAG_URL)

libid3tag: libid3tag-$(LIBID3TAG_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -d libid3tag -p0 < Patches/libid3tag-0.15.1b-fix-CVE-2008-2109.patch
ifdef HAVE_DARWIN_OS
	( cd $@; sed -e 's%-dynamiclib%-dynamiclib -arch $(ARCH)%' -i.orig  configure )
endif

.id3tag: libid3tag
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .id3tag
CLEAN_PKG += libid3tag
DISTCLEAN_PKG += libid3tag-$(LIBID3TAG_VERSION).tar.gz

# ***************************************************************************
# pcre
# ***************************************************************************

pcre-$(PCRE_VERSION).tar.bz2:
	$(WGET) $(PCRE_URL)

pcre: pcre-$(PCRE_VERSION).tar.bz2
	$(EXTRACT_BZ2)

.pcre: pcre
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-shared && make && make install )
	touch $@

CLEAN_FILE += .pcre
CLEAN_PKG += pcre
DISTCLEAN_PKG += pcre-$(PCRE_VERSION).tar.bz2

# ***************************************************************************
# lua
# ***************************************************************************

ifdef HAVE_WIN32
LUA_MAKEPLATEFORM=mingw
else
ifdef HAVE_DARWIN_OS
LUA_MAKEPLATEFORM=macosx
else
ifdef HAVE_LINUX
LUA_MAKEPLATEFORM=linux
else
ifdef HAVE_BSD
LUA_MAKEPLATEFORM=bsd
else
LUA_MAKEPLATEFORM=generic
endif
endif
endif
endif

lua-$(LUA_VERSION).tar.gz:
	$(WGET) $(LUA_URL)

lua: lua-$(LUA_VERSION).tar.gz
	$(EXTRACT_GZ)
ifdef HAVE_DARWIN_OS
	(cd $@; sed -e 's%gcc%$(CC)%' -e 's%LDFLAGS=%LDFLAGS=$(EXTRA_CFLAGS) $(EXTRA_LDFLAGS)%' -i.orig  src/Makefile)
endif

.lua: lua
ifdef HAVE_WIN32
	( cd $< && sed -i.orig 's/lua luac/lua.exe/' Makefile && cd src && sed -i.orig 's/CC=/#CC=/' Makefile && sed -i 's/strip/$(STRIP)/' Makefile && cd ../..)
	(cd $<&& $(HOSTCC) make $(LUA_MAKEPLATEFORM)&& cd src&& $(HOSTCC) make liblua.a&& cd ..&&$(HOSTCC) make install INSTALL_TOP=$(PREFIX)&& $(RANLIB) $(PREFIX)/lib/liblua.a)
	(cd $<&& sed -i.orig 's@prefix= /usr/local@prefix= $(PREFIX)@' etc/lua.pc&& mkdir -p $(PREFIX)/lib/pkgconfig&& cp etc/lua.pc $(PREFIX)/lib/pkgconfig)
else
	(cd $<&& patch -p1) < Patches/lua-noreadline.patch
	(cd $<&& $(HOSTCC) make $(LUA_MAKEPLATEFORM) && make install INSTALL_TOP=$(PREFIX))
endif
	touch $@

CLEAN_FILE += .lua
CLEAN_PKG += lua
DISTCLEAN_PKG += lua-$(LUA_VERSION).tar.gz

# ***************************************************************************
# libmad
# ***************************************************************************

libmad-$(LIBMAD_VERSION).tar.gz:
	$(WGET) $(LIBMAD_URL)

libmad: libmad-$(LIBMAD_VERSION).tar.gz
	$(EXTRACT_GZ)
ifdef HAVE_DARWIN_OS
	( cd $@; sed -e 's%-march=i486%$(EXTRA_CFLAGS) $(EXTRA_LDFLAGS)%' -e 's%-dynamiclib%-dynamiclib -arch $(ARCH)%' -i.orig  configure )
endif

.mad: libmad
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="-O3 $(NOTHUMB)" && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .mad
CLEAN_PKG += libmad
DISTCLEAN_PKG += libmad-$(LIBMAD_VERSION).tar.gz

# ***************************************************************************
# ogg
# ***************************************************************************

libogg-$(OGG_VERSION).tar.gz:
	$(WGET) $(OGG_URL)

libogg: libogg-$(OGG_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p0 < Patches/ogg.patch
	patch -p0 < Patches/libogg-1.1.patch
ifdef HAVE_WINCE
	patch -p0 < Patches/libogg-wince.patch
endif
	(cd $@; autoconf)

.ogg: libogg
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .ogg
CLEAN_PKG += libogg
DISTCLEAN_PKG += libogg-$(OGG_VERSION).tar.gz

# ***************************************************************************
# vorbis
# ***************************************************************************

libvorbis-$(VORBIS_VERSION).tar.gz:
	$(WGET) $(VORBIS_URL)

libvorbis: libvorbis-$(VORBIS_VERSION).tar.gz
	$(EXTRACT_GZ)

.vorbis: libvorbis .ogg
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
#	$(INSTALL_NAME)
	touch $@

.vorbisenc: .vorbis .ogg
#	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .vorbis .vorbisenc
CLEAN_PKG += libvorbis
DISTCLEAN_PKG += libvorbis-$(VORBIS_VERSION).tar.gz

# ***************************************************************************
# tremor
# ***************************************************************************

ifdef SVN
tremor:
	$(SVN) co http://svn.xiph.org/trunk/Tremor tremor
	(cd $@ && patch -p0 < ../Patches/tremor.patch)
	(cd $@; rm -f ogg.h && echo "#include <ogg/ogg.h>" > ogg.h && rm -f os_types.h && echo "#include <ogg/os_types.h>" > os_types.h)
else
tremor-$(TREMOR_VERSION).tar.bz2:
	echo "tremor snapshot does not exist, you MUST use subversion !"
	exit -1
	$(WGET) $(TREMOR_URL)

tremor: tremor-$(TREMOR_VERSION).tar.bz2
	$(EXTRACT_BZ2)
endif

.tremor: tremor .ogg
	(cd $<; $(HOSTCC) ./autogen.sh $(HOSTCONF) --prefix=$(PREFIX) --disable-shared CFLAGS="$(NOTHUMB)" && make && make install)
	$(INSTALL_NAME)
	touch $@

ifdef SVN
tremor-source: tremor
	tar cv --exclude=.svn tremor | bzip2 > tremor-$(DATE).tar.bz2

SOURCE += tremor-source
endif

CLEAN_FILE += .tremor
CLEAN_PKG += tremor
#DISTCLEAN_PKG += tremor-$(TREMOR_VERSION).tar.bz2 #no tremor snapshot

# ***************************************************************************
# theora
# ***************************************************************************

libtheora-$(THEORA_VERSION).tar.bz2:
	$(WGET) $(THEORA_URL)

libtheora: libtheora-$(THEORA_VERSION).tar.bz2
	$(EXTRACT_BZ2)
	patch -p0 < Patches/libtheora-includes.patch

THEORACONF = --disable-sdltest --disable-oggtest --disable-vorbistest --disable-examples

ifdef HAVE_DARWIN_64
THEORACONF += --disable-asm
endif
ifdef HAVE_WIN64
THEORACONF += --disable-asm
endif

.theora: libtheora .ogg
ifdef HAVE_DARWIN_OS
	cd $<; ($(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) $(THEORACONF) && make && make install)
else
ifdef HAVE_WIN32
	cd $<; $(HOSTCC) ./autogen.sh $(HOSTCONF) --prefix=$(PREFIX) $(THEORACONF)
endif
	if test ! -f $</config.status; then \
		cd $< ; \
		$(HOSTCC) ./configure $(HOSTCONF) \
			--prefix=$(PREFIX) $(THEORACONF) ; \
	fi
	cd $< && make && make install
endif
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .theora
CLEAN_PKG += libtheora
DISTCLEAN_PKG += libtheora-$(THEORA_VERSION).tar.bz2

# ***************************************************************************
# shout
# ***************************************************************************

libshout-$(SHOUT_VERSION).tar.gz:
	$(WGET) $(SHOUT_URL)

libshout: libshout-$(SHOUT_VERSION).tar.gz
	$(EXTRACT_GZ)
ifdef HAVE_WIN32
	patch -p0 < Patches/libshout-win32.patch
endif

.shout: libshout .theora .ogg .speex .vorbis
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) &&  make && make install )
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .shout
CLEAN_PKG += libshout
DISTCLEAN_PKG += libshout-$(SHOUT_VERSION).tar.gz

# ***************************************************************************
# flac
# ***************************************************************************

flac-$(FLAC_VERSION).tar.gz:
	$(WGET) $(FLAC_URL)

flac: flac-$(FLAC_VERSION).tar.gz
	$(EXTRACT_GZ)
ifdef HAVE_WIN32
	patch -p0 < Patches/flac-win32.patch
endif
ifdef HAVE_DARWIN_OS
	( cd $@; sed -e 's%-dynamiclib%-dynamiclib -arch $(ARCH)%' -i.orig  configure )
endif

FLAC_DISABLE_FLAGS = --disable-oggtest --disable-xmms-plugin --disable-cpplibs

.flac: flac .ogg
ifdef HAVE_WIN32
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) $(FLAC_DISABLE_FLAGS) && cd src&& make -C libFLAC&& make -C libFLAC install && cd .. && make -C include install)
else
ifdef HAVE_DARWIN_OS
ifdef HAVE_DARWIN_OS_ON_INTEL
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-asm-optimizations && cd src && make -C libFLAC && make -C libFLAC install  && cd .. && make -C include install)
else
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) $(FLAC_DISABLE_FLAGS) && cd src && make -C libFLAC && make -C libFLAC install  && cd .. && make -C include install)
endif
else
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX)  $(FLAC_DISABLE_FLAGS) && cd src && make -C libFLAC && make -C libFLAC install  && cd .. && make -C include install)
endif
endif
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .flac
CLEAN_PKG += flac
DISTCLEAN_PKG += flac-$(FLAC_VERSION).tar.gz

# ***************************************************************************
# speex
# ***************************************************************************

speex-$(SPEEX_VERSION).tar.gz:
	$(WGET) $(SPEEX_URL)

speex: speex-$(SPEEX_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p0 < Patches/speex.patch

.speex: speex
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --enable-ogg=no && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .speex
CLEAN_PKG += speex
DISTCLEAN_PKG += speex-$(SPEEX_VERSION).tar.gz

# ***************************************************************************
# faad2
# ***************************************************************************

faad2-$(FAAD2_VERSION).tar.gz:
	$(WGET) $(FAAD2_URL)

faad2: faad2-$(FAAD2_VERSION).tar.gz
	$(EXTRACT_GZ)
	(cd $@; echo|$(CC) -iquote . -E - || sed -i 's/-iquote /-I/' libfaad/Makefile.am; sh ./bootstrap)
	patch -p0 < Patches/faad-arm-fixed.patch

.faad: faad2
	(cd $< && $(HOSTCC) ./configure $(HOSTCONF) --disable-shared --prefix=$(PREFIX) CFLAGS="-O3 $(NOTHUMB)" && sed -i.orig "s/shrext_cmds/shrext/g" libtool && make -C libfaad && make -C libfaad install)
	touch $@

CLEAN_FILE += .faad
CLEAN_PKG += faad2
DISTCLEAN_PKG += faad2-$(FAAD2_VERSION).tar.gz

# ***************************************************************************
# lame
# ***************************************************************************

lame-$(LAME_VERSION).tar.gz:
	$(WGET) $(LAME_URL)

lame: lame-$(LAME_VERSION).tar.gz
	$(EXTRACT_GZ)

.lame: lame
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-analyser-hooks --disable-decoder --disable-shared --disable-gtktest --disable-frontend && make && make install)
	touch $@

CLEAN_FILE += .lame
CLEAN_PKG += lame
DISTCLEAN_PKG += lame-$(LAME_VERSION).tar.gz

# ***************************************************************************
# libebml
# ***************************************************************************

libebml-$(LIBEBML_VERSION).tar.bz2:
	$(WGET) $(LIBEBML_URL)

libebml: libebml-$(LIBEBML_VERSION).tar.bz2
	$(EXTRACT_BZ2)
ifdef HAVE_WIN32
	(cd $@; patch -p0 < ../Patches/libebml-svn-win32.patch)
endif
ifdef HAVE_WIN64
	patch -p0 < Patches/libebml-win64.patch
endif

.ebml: libebml
ifdef HAVE_WIN32
	(cd $<; make -C make/mingw32 prefix=$(PREFIX) $(HOSTCC2) CFLAGS="$(CFLAGS) -DWIN32" SHARED=no && make -C make/linux install_staticlib install_headers prefix=$(PREFIX) $(HOSTCC2))
else
	(cd $<; make -C make/linux prefix=$(PREFIX) $(HOSTCC2) staticlib && make -C make/linux install_staticlib install_headers prefix=$(PREFIX))
	$(RANLIB) $(PREFIX)/lib/libebml.a
endif
	touch $@

CLEAN_FILE += .ebml
CLEAN_PKG += libebml
DISTCLEAN_PKG += libebml-$(LIBEBML_VERSION).tar.bz2

# ***************************************************************************
# libmatroska
# ***************************************************************************

libmatroska-$(LIBMATROSKA_VERSION).tar.bz2:
	$(WGET) $(LIBMATROSKA_URL)

libmatroska: libmatroska-$(LIBMATROSKA_VERSION).tar.bz2
	$(EXTRACT_BZ2)
	patch -p0 < ./Patches/libmatroska-kaxblock.patch
ifdef HAVE_WIN32
	(cd $@; patch -p0 < ../Patches/libmatroska-win32.patch)
endif

.matroska: libmatroska .ebml
ifdef HAVE_WIN32
	(cd $<; make -C make/mingw32 prefix=$(PREFIX) $(HOSTCC2) SHARED=no EBML_DLL=no libmatroska.a && make -C make/linux install_staticlib install_headers prefix=$(PREFIX) $(HOSTCC2))
else
	(cd $<; make -C make/linux prefix=$(PREFIX) $(HOSTCC2) staticlib && make -C make/linux install_staticlib install_headers prefix=$(PREFIX))
	$(RANLIB) $(PREFIX)/lib/libmatroska.a
endif
	touch $@


CLEAN_FILE += .matroska
CLEAN_PKG += libmatroska
DISTCLEAN_PKG += libmatroska-$(LIBMATROSKA_VERSION).tar.bz2

# ***************************************************************************
# libamrnb
# ***************************************************************************

amrnb-$(LIBAMR_NB_VERSION).tar.bz2:
	$(WGET) $(LIBAMR_NB)

libamrnb: amrnb-$(LIBAMR_NB_VERSION).tar.bz2
	$(EXTRACT_BZ2)

.libamrnb: libamrnb
	(cd $<; $(HOSTCC) ./configure --prefix=$(PREFIX) --enable-shared && make && make install)
	touch $@

CLEAN_FILE += .libamrnb
CLEAN_PKG += libamrnb
DISTCLEAN_PKG += amrnb-$(LIBAMR_NB_VERSION).tar.bz2

# ***************************************************************************
# libamrwb
# ***************************************************************************

amrwb-$(LIBAMR_WB_VERSION).tar.bz2:
	$(WGET) $(LIBAMR_WB)

libamrwb: amrwb-$(LIBAMR_WB_VERSION).tar.bz2
	$(EXTRACT_BZ2)

.libamrwb: libamrwb
	(cd $<; $(HOSTCC) ./configure --prefix=$(PREFIX) --enable-shared && make && make install)
	touch $@


CLEAN_FILE += .libamrwb
CLEAN_PKG += libamrwb
DISTCLEAN_PKG += amrwb-$(LIBAMR_WB_VERSION).tar.bz2

# ***************************************************************************

# ffmpeg
# ***************************************************************************

ifdef SVN
ffmpeg:
	$(SVN) co $(FFMPEG_SVN) ffmpeg -r 19836
ifdef HAVE_ISA_THUMB
	patch -p0 < Patches/ffmpeg-avcodec-no-thumb.patch
endif
ifdef HAVE_WINCE
	patch -p1 < Patches/ffmpeg-svn-wince.patch
endif
ifdef HAVE_UCLIBC
	patch -p0 < Patches/ffmpeg-svn-uclibc.patch
	patch -p0 < Patches/ffmpeg-svn-internal-define.patch
	patch -p0 < Patches/ffmpeg-svn-libavformat.patch
endif
ifdef HAVE_WIN32
	(cd $@; patch -p0 < ../Patches/ffmpeg-svn-win32.patch)
endif
ifdef NO_TEXT_RELOCATION
	echo "Broken LD. Fix your tools."
	exit -1
endif
	(cd $@; patch -p1 < ../Patches/eac3_spectral.patch)
else
ffmpeg-$(FFMPEG_VERSION).tar.gz:
	echo "ffmpeg snapshot is too old, you MUST use subversion !"
	exit -1
	$(WGET) $(FFMPEG_URL)

ffmpeg: ffmpeg-$(FFMPEG_VERSION).tar.gz
	$(EXTRACT_GZ)
endif

FFMPEGCONF += \
	--disable-debug \
	--enable-gpl \
	--enable-postproc \
	--disable-ffserver \
	--disable-ffmpeg \
	--disable-ffplay \
	--disable-devices \
	--disable-protocols \
	--disable-network
ifdef HAVE_WIN32
FFMPEGCONF += --disable-bzlib --disable-decoder=dca --disable-encoder=ac3 --enable-libmp3lame
endif

ifdef HAVE_WINCE
.ffmpeg: ffmpeg .zlib
else
ifdef HAVE_UCLIBC
.ffmpeg: ffmpeg
else
.ffmpeg: ffmpeg .lame .gsm
endif
endif
	(cd $<; $(HOSTCC) ./configure --prefix=$(PREFIX) --extra-cflags="$(CFLAGS) $(FFMPEG_CFLAGS)" --extra-ldflags="$(LDFLAGS)" $(FFMPEGCONF) --disable-shared --enable-static && make && make install-libs install-headers)
	touch $@

ifdef SVN
ffmpeg-source: ffmpeg
	tar cv --exclude=.svn ffmpeg | bzip2 > ffmpeg-$(DATE).tar.bz2

SOURCE += ffmpeg-source
endif

CLEAN_FILE += .ffmpeg
CLEAN_PKG += ffmpeg
DISTCLEAN_PKG += ffmpeg-$(FFMPEG_VERSION).tar.gz

# ***************************************************************************
# libdvdcss
# ***************************************************************************

libdvdcss-$(LIBDVDCSS_VERSION).tar.gz:
	$(WGET) $(LIBDVDCSS_URL)

ifdef SVN
libdvdcss:
	$(SVN) co svn://svn.videolan.org/libdvdcss/trunk libdvdcss
	cd $@ && sh bootstrap
else
libdvdcss: libdvdcss-$(LIBDVDCSS_VERSION).tar.gz
	$(EXTRACT_GZ)
endif

.dvdcss: libdvdcss
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --disable-doc --prefix=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

ifdef SVN
libdvdcss-source: libdvdcss
	tar cv --exclude=.svn libdvdcss | bzip2 > libdvdcss-$(DATE).tar.bz2

SOURCE += libdvdcss-source
endif

CLEAN_FILE += .dvdcss
CLEAN_PKG += libdvdcss
DISTCLEAN_PKG += libdvdcss-$(LIBDVDCSS_VERSION).tar.gz

# ***************************************************************************
# dummy dvdread to trigger the INSTALL_NAME script
# ***************************************************************************
# .dvdread: .dvdnav
# 	$(INSTALL_NAME)
# 	touch $@
#
# CLEAN_FILE += .dvdread

# ***************************************************************************
# libdvdread: We use dvdnav's dvdread
# ***************************************************************************


libdvdread:
	$(SVN) co $(LIBDVDREAD_SVN)  libdvdread
	(cd $@; patch  -p 0 < ../Patches/libdvdread.patch)
ifdef HAVE_WIN32
	(cd $@; patch  -p 0 < ../Patches/libdvdread-win32.patch)
endif
	(cd $@; sh autogen.sh noconfig)

.libdvdread: libdvdread .dvdcss
	(cd libdvdread; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --with-libdvdcss=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .libdvdread
CLEAN_PKG += libdvdread
#DISTCLEAN_PKG += libdvdread-$(LIBDVDREAD_VERSION).tar.gz

# ***************************************************************************
# libdvdnav
# ***************************************************************************

ifdef SVN
libdvdnav:
	$(SVN) co $(LIBDVDNAV_SVN)  libdvdnav
	patch -d libdvdnav -p0 < Patches/libdvdnav.patch
	(cd $@; ./autogen.sh noconfig)
else
libdvdnav-$(LIBDVDNAV_VERSION).tar.gz:
	$(WGET) $(LIBDVDNAV_URL)

libdvdnav: libdvdnav-$(LIBDVDNAV_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p0 < Patches/libdvdnav.patch
ifdef HAVE_WIN32
	patch -p0 < Patches/libdvdnav-win32.patch
endif
	(cd $@; ./autogen.sh noconfig)
endif

.dvdnav: libdvdnav .libdvdread
ifdef HAVE_WIN32
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --enable-static --prefix=$(PREFIX) && make && make install)
else
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --enable-static --prefix=$(PREFIX) CFLAGS="$(CFLAGS) -Dlseek64=lseek -Doff64_t=off_t" && make && make install)
endif
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .dvdnav
CLEAN_PKG += libdvdnav
DISTCLEAN_PKG += libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2

# ***************************************************************************
# libdvbpsi
# ***************************************************************************

libdvbpsi5-$(LIBDVBPSI_VERSION).tar.gz:
	$(WGET) $(LIBDVBPSI_URL)

libdvbpsi: libdvbpsi5-$(LIBDVBPSI_VERSION).tar.gz
	$(EXTRACT_GZ)

.dvbpsi: libdvbpsi
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && cd src && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .dvbpsi
CLEAN_PKG += libdvbpsi
DISTCLEAN_PKG += libdvbpsi5-$(LIBDVBPSI_VERSION).tar.gz

# ***************************************************************************
# live
# ***************************************************************************

live555-$(LIVEDOTCOM_VERSION).tar.gz:
	$(WGET) $(LIVEDOTCOM_URL)

live: live555-$(LIVEDOTCOM_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p0 < Patches/live-noapps.patch
	patch -p0 < Patches/live-uselocale.patch
ifdef HAVE_WIN64
	patch -p0 < Patches/live-win64.patch
endif

.live: live
ifdef HAVE_WIN32
	(cd $<;./genMakefiles mingw && make $(HOSTCC))
else
ifdef HAVE_DARWIN_OS
	(cd $<; sed -e 's%-DBSD=1%-DBSD=1\ $(EXTRA_CFLAGS)\ $(EXTRA_LDFLAGS)%' -e 's%cc%$(CC)%'  -e 's%c++%$(CXX)%' -i.orig  config.macosx)
	(cd $<; ./genMakefiles macosx && make)
else
	(cd $<; sed -e 's/=/= EXTRA_CPPFLAGS/' -e 's%EXTRA_CPPFLAGS%-I/include%' -i.orig groupsock/Makefile.head)
ifdef HAVE_UCLIBC
ifdef HAVE_BIGENDIAN
	(cd $<; ./genMakefiles armeb-uclibc && make $(HOSTCC))
endif
else
	(cd $<; sed -e 's%-D_FILE_OFFSET_BITS=64%-D_FILE_OFFSET_BITS=64\ -fPIC\ -DPIC%' -i.orig config.linux)
	(cd $<; ./genMakefiles linux && make $(HOSTCC))
endif
endif
endif
	mkdir -p $(PREFIX)/lib $(PREFIX)/include
	cp $</groupsock/libgroupsock.a $(PREFIX)/lib
	cp $</liveMedia/libliveMedia.a $(PREFIX)/lib
	cp $</UsageEnvironment/libUsageEnvironment.a $(PREFIX)/lib
	cp $</BasicUsageEnvironment/libBasicUsageEnvironment.a $(PREFIX)/lib
	cp $</groupsock/include/*.hh $</groupsock/include/*.h $(PREFIX)/include
	cp $</liveMedia/include/*.hh $(PREFIX)/include
	cp $</UsageEnvironment/include/*.hh $(PREFIX)/include
	cp $</BasicUsageEnvironment/include/*.hh $(PREFIX)/include
	touch $@

CLEAN_FILE += .live
CLEAN_PKG += live
DISTCLEAN_PKG += live555-$(LIVEDOTCOM_VERSION).tar.gz

# ***************************************************************************
# goom2k4
# ***************************************************************************

goom$(GOOM2k4_VERSION).tar.gz:
	$(WGET) $(GOOM2k4_URL)
	mv goom-$(GOOM2k4_VERSION)-src.tar.gz goom$(GOOM2k4_VERSION).tar.gz

goom: goom$(GOOM2k4_VERSION).tar.gz
	$(EXTRACT_GZ)
ifdef HAVE_WIN32
	(cd $@; dos2unix configure.in)
	patch -p0 < Patches/goom2k4-0-win32.patch
else
	patch -p0 < Patches/goom2k4-0-mmx.patch
endif
	patch -p0 < Patches/goom2k4-0-memleaks.patch
	patch -p0 < Patches/goom2k4-autotools.patch
	(cd $@; rm -f configure; ACLOCAL="aclocal -I m4/" autoreconf -ivf)

.goom2k4: goom
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-shared --enable-static --disable-glibtest --disable-gtktest && make && make install)
	touch $@

CLEAN_FILE += .goom2k4
CLEAN_PKG += goom
DISTCLEAN_PKG += goom$(GOOM2k4_VERSION).tar.gz

# ***************************************************************************
# libcaca
# ***************************************************************************

libcaca-$(LIBCACA_VERSION).tar.gz:
	$(WGET) $(LIBCACA_URL)

libcaca: libcaca-$(LIBCACA_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p0 < Patches/libcaca-static.patch
ifdef HAVE_DARWIN_OS
	patch -p0 < Patches/libcaca-osx-sdkofourchoice.patch
	(cd $@; sed -e 's%/Developer/SDKs/MacOSX10.4u.sdk%$(MACOSX_SDK)%' -i.orig  configure)
endif

.caca: libcaca
ifdef HAVE_WIN32
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --enable-win32 --disable-x11 --disable-imlib2 --disable-doc --disable-ruby --disable-csharp --disable-cxx && make && make install)
else
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-imlib2 --disable-doc --disable-ruby --disable-csharp --disable-cxx && make && make install)
endif
	touch $@

CLEAN_FILE += .caca
CLEAN_PKG += libcaca
DISTCLEAN_PKG += libcaca-$(LIBCACA_VERSION).tar.gz

# ***************************************************************************
# libdca
# ***************************************************************************

libdca-$(LIBDCA_VERSION).tar.bz2:
	$(WGET) $(LIBDCA_URL)

libdca: libdca-$(LIBDCA_VERSION).tar.bz2
	$(EXTRACT_BZ2)
ifdef HAVE_DARWIN_9
	( cd $@; patch -p0 < ../Patches/libdca-llvm-gcc.patch )
endif

.dca: libdca
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .dca
CLEAN_PKG += libdca
DISTCLEAN_PKG += libdca-$(LIBDCA_VERSION).tar.bz2

# ***************************************************************************
# libx264
# ***************************************************************************

x264-$(X264_VERSION).tar.gz:
	$(WGET) $(X264_URL)

ifdef GIT
x264:
	$(GIT) clone git://git.videolan.org/x264.git
	patch -p0 < Patches/x264-svn-info-threads.patch
ifdef HAVE_WIN32
	(cd x264; patch -p0 < ../Patches/x264-svn-win32.patch )
endif

else

x264: x264-$(X264_VERSION).tar.gz
	$(EXTRACT_GZ)
ifdef HAVE_WIN32
  ifndef HAVE_CYGWIN
	(cd x264;patch -p0 <../Patches/x264-20050624-win32.patch)
  endif
else
	patch -p0 < Patches/x264.patch
	rm x264/x264.c
endif
endif

ifdef HAVE_WIN32
.x264: x264 .pthreads
  ifdef HAVE_CYGWIN
	(cd $<; $(HOSTCC) RANLIB="ranlib" AR="ar" STRIP="strip" ./configure $(X264CONF) --prefix="$(PREFIX)" --extra-cflags="-I$(PREFIX)/include" --extra-ldflags="-L$(PREFIX)/lib" && make && make install)
  else
	(cd $<; $(HOSTCC) ./configure $(X264CONF) --prefix="$(PREFIX)" --enable-pthread && make && make install)
  endif
else
ifdef HAVE_DARWIN_OS_ON_INTEL
  .x264: x264 .yasm
	(cd $<; $(HOSTCC) ./configure $(X264CONF) --prefix="$(PREFIX)" && make && make install)
else
  .x264: x264
	(cd $<; $(HOSTCC) ./configure $(X264CONF) --prefix="$(PREFIX)" && make && make install)
endif
endif
	touch $@

ifdef GIT
x264-source: x264
	cd x264 && \
	git archive --format=tar HEAD | bzip2 > ../x264-$(DATE).tar.bz2

SOURCE += x264-source
endif

CLEAN_FILE += .x264
CLEAN_PKG += x264
DISTCLEAN_PKG += x264-$(X264_VERSION).tar.gz

# ***************************************************************************
# libmodplug
# ***************************************************************************

libmodplug-$(MODPLUG_VERSION).tar.gz:
	$(WGET) $(MODPLUG_URL)

libmodplug: libmodplug-$(MODPLUG_VERSION).tar.gz
	$(EXTRACT_GZ)
ifdef HAVE_WIN32
	patch -p0 < Patches/libmodplug-win32.patch
	(cd $@; autoreconf -ivf )
endif

.mod: libmodplug
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-shared --enable-static && make && make install)
	touch $@

CLEAN_FILE += .mod
CLEAN_PKG += libmodplug
DISTCLEAN_PKG += libmodplug-$(MODPLUG_VERSION).tar.gz

# ***************************************************************************
# libcddb
# ***************************************************************************

libcddb-$(CDDB_VERSION).tar.bz2:
	$(WGET) $(CDDB_URL)

libcddb: libcddb-$(CDDB_VERSION).tar.bz2
	$(EXTRACT_BZ2)
	(cd $@; patch -p0 < ../Patches/libcddb-cross.patch )
ifdef HAVE_WIN32
	(cd $@; patch -p0 < ../Patches/libcddb-win32.patch )
endif
	(cd $@; autoreconf -fisv)

ifdef HAVE_WIN32
.cddb: libcddb .regex
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-shared --enable-static --without-iconv CFLAGS="$(CFLAGS) -D_BSD_SOCKLEN_T_=int" && make && make install)
else
.cddb: libcddb
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-shared --enable-static CFLAGS="$(CFLAGS) -D_BSD_SOCKLEN_T_=int" LDFLAGS="$(LDFLAGS) -liconv" && make && make install)
endif
	touch $@

CLEAN_FILE += .cddb
CLEAN_PKG += libcddb
DISTCLEAN_PKG += libcddb-$(CDDB_VERSION).tar.bz2

# ***************************************************************************
# vcdimager
# ***************************************************************************

vcdimager-$(VCDIMAGER_VERSION).tar.gz:
	$(WGET) $(VCDIMAGER_URL)

vcdimager: vcdimager-$(VCDIMAGER_VERSION).tar.gz
	$(EXTRACT_GZ)

ifdef HAVE_DARWIN_OS
.vcdimager: vcdimager
	(cd $<; ./configure --prefix=$(PREFIX) --disable-shared --enable-static LDFLAGS="$(LDFLAGS) -framework CoreFoundation -framework IOKit" && make && make install)
	touch $@
else
.vcdimager: vcdimager
	(cd $<; ./configure --prefix=$(PREFIX) --disable-shared --enable-static && make && make install)
	touch $@
endif

CLEAN_FILE += .vcdimager
CLEAN_PKG += vcdimager
DISTCLEAN_PKG += vcdimager-$(VCDIMAGER_VERSION).tar.gz

# ***************************************************************************
# libcdio
# ***************************************************************************

libcdio-$(CDIO_VERSION).tar.gz:
	$(WGET) $(CDIO_URL)

libcdio: libcdio-$(CDIO_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p0 < Patches/libcdio-install-cdparanoia-pc.patch
ifdef HAVE_DARWIN_OS
	patch -p0 < Patches/libcdio-modernOSX.patch
endif

.cdio: libcdio
	(cd $<; sed -e 's%@ENABLE_CPP_TRUE@SUBDIRS = C++%@ENABLE_CPP_TRUE@SUBDIRS = %' -i.orig example/Makefile.in && autoreconf -fisv && ./configure --prefix=$(PREFIX) --without-vcdinfo --disable-shared && make && make install)
	touch $@

CLEAN_FILE += .cdio
CLEAN_PKG += libcdio
DISTCLEAN_PKG += libcdio-$(CDIO_VERSION).tar.gz

# ***************************************************************************
# qt4 (win32 binary)
# ***************************************************************************

qt4-$(QT4_VERSION)-win32-bin.tar.bz2:
	$(WGET) $(QT4_URL)

qt4_win32: qt4-$(QT4_VERSION)-win32-bin.tar.bz2
	$(EXTRACT_BZ2)
	chmod -R 755 qt4_win32

.qt4_win32: qt4_win32
	(cd qt4_win32;mkdir -p $(PREFIX)/bin; mkdir -p $(PREFIX)/include;mkdir -p $(PREFIX)/lib/pkgconfig;rm -f $(PREFIX)/lib/pkgconfig/Qt*; sed 's,@@PREFIX@@,$(PREFIX),' lib/pkgconfig/QtCore.pc.in > $(PREFIX)/lib/pkgconfig/QtCore.pc;sed 's,@@PREFIX@@,$(PREFIX),' lib/pkgconfig/QtGui.pc.in > $(PREFIX)/lib/pkgconfig/QtGui.pc;cp -r include/* $(PREFIX)/include;cp lib/*a $(PREFIX)/lib)
ifeq ($(BUILD),i686-pc-cygwin)
	(cd qt4_win32;cp bin/*.exe $(PREFIX)/bin)
else
	(cd qt4_win32;cp bin/* $(PREFIX)/bin)
endif
	touch $@

CLEAN_FILE += .qt4_win32
CLEAN_PKG += qt4_win32
DISTCLEAN_PKG += qt4-$(QT4_VERSION)-win32-bin.tar.bz2

# ***************************************************************************
# qt4 (trolltech binaries)
# ***************************************************************************

qt-win-opensource-$(QT4T_VERSION)-mingw.exe:
	wget $(QT4T_URL)

Qt_win32: qt-win-opensource-$(QT4T_VERSION)-mingw.exe
	mkdir Qt
	7z -oQt x qt-win-opensource-$(QT4T_VERSION)-mingw.exe \$$OUTDIR/bin\ /bin \$$OUTDIR/bin\ /lib \$$OUTDIR/bin\ /include/QtCore \$$OUTDIR/bin\ /include/QtGui \$$OUTDIR/bin\ /src/gui \$$OUTDIR/bin\ /src/corelib \$$OUTDIR/bin\ /translations
	mv Qt/\$$OUTDIR/bin\ /* Qt/ && rmdir Qt/\$$OUTDIR/bin\  Qt/\$$OUTDIR
	find Qt -name '* ' -exec sh -c "mv \"{}\" `echo {}`" \;
	find Qt/src -name '*.cpp' -exec rm {} \;
	find Qt/translations -type f -a ! -name 'qt_*.qm' -exec rm {} \;
	find Qt/include -name '*.h' -exec sh -c "mv {} {}.tmp; sed 's,..\/..\/src,..\/src,' {}.tmp > {}; rm -f {}.tmp" \;
	mkdir Qt/lib/pkgconfig
	sed -e s,@@VERSION@@,$(QT4T_VERSION), -e s,@@PREFIX@@,$(PREFIX), Patches/QtCore.pc.in > Qt/lib/pkgconfig/QtCore.pc
	sed -e s,@@VERSION@@,$(QT4T_VERSION), -e s,@@PREFIX@@,$(PREFIX), Patches/QtGui.pc.in > Qt/lib/pkgconfig/QtGui.pc

.Qt_win32: Qt_win32
	mkdir -p $(PREFIX)/bin $(PREFIX)/include/qt4/src $(PREFIX)/lib/pkgconfig $(PREFIX)/share/qt4/translations
	cp Qt/bin/moc.exe Qt/bin/rcc.exe Qt/bin/uic.exe $(PREFIX)/bin
	cp -r Qt/include/QtCore Qt/include/QtGui $(PREFIX)/include/qt4
	cp -r Qt/src/corelib Qt/src/gui $(PREFIX)/include/qt4/src
	cp Qt/lib/libQtCore4.a Qt/lib/libQtGui4.a $(PREFIX)/lib
	cp Qt/lib/pkgconfig/* $(PREFIX)/lib/pkgconfig
	cp Qt/translations/* $(PREFIX)/share/qt4/translations
	touch $@

CLEAN_FILE += .Qt_win32
CLEAN_PKG += Qt_win32
DISTCLEAN_PKG += qt-win-opensource-$(QT4T_VERSION)-mingw.exe

# ***************************************************************************
# qt4 (source-code compilation for Mac)
# ***************************************************************************

qt-mac-opensource-src-$(QT4_MAC_VERSION).tar.gz:
	$(WGET) $(QT4_MAC_URL)

qt4_mac: qt-mac-opensource-src-$(QT4_MAC_VERSION).tar.gz
	$(EXTRACT_GZ)

.qt4_mac: qt4_mac
	(cd qt4_mac; ./configure -prefix $(PREFIX) -release -fast -no-qt3support -nomake "examples demos" -sdk $(MACOSX_SDK) -no-framework -arch $(ARCH) && make && make install)
	touch $@

CLEAN_FILE += .qt4_mac
CLEAN_PKG += qt4_mac
DISTCLEAN_PKG += qt-mac-opensource-src-$(QT4_MAC_VERSION).tar.gz

# ***************************************************************************
# zlib
# ***************************************************************************

zlib-$(ZLIB_VERSION).tar.gz:
	$(WGET) $(ZLIB_URL)

zlib: zlib-$(ZLIB_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p0 < Patches/zlib.patch

.zlib: zlib
	(cd zlib; $(HOSTCC2) ./configure --prefix=$(PREFIX) && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .zlib
CLEAN_PKG += zlib
DISTCLEAN_PKG += zlib-$(ZLIB_VERSION).tar.gz

# ***************************************************************************
# PortAudio
# ***************************************************************************

pa_snapshot_v$(PORTAUDIO_VERSION).tar.gz:
	$(WGET) $(PORTAUDIO_URL)

portaudio: pa_snapshot_v$(PORTAUDIO_VERSION).tar.gz
	$(EXTRACT_GZ)
ifneq ($(HOST),$(BUILD))
	(patch -p0 < Patches/portaudio-cross.patch;cd $@;  autoconf)
endif

.portaudio: portaudio
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make  install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .portaudio
CLEAN_PKG += portaudio
DISTCLEAN_PKG += pa_snapshot_v$(PORTAUDIO_VERSION).tar.gz

# ***************************************************************************
# xml
# ***************************************************************************

libxml2-$(XML_VERSION).tar.gz:
	$(WGET) $(XML_URL)

xml: libxml2-$(XML_VERSION).tar.gz
	$(EXTRACT_GZ)
ifdef HAVE_UCLIBC
	patch -p0 < Patches/xml2-uclibc.patch
endif

XMLCONF = --with-minimal --with-catalog --with-reader --with-tree --with-push --with-xptr --with-valid --with-xpath --with-xinclude --with-sax1 --without-zlib --without-iconv --without-http --without-ftp  --without-debug --without-docbook --without-regexps --without-python

.xml: xml
  ifdef HAVE_CYGWIN
	(cd xml; ac_cv_header_pthread_h="no" CFLAGS="-DLIBXML_STATIC" $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) $(XMLCONF) && make && make install)
  else
	(cd xml; CFLAGS="-DLIBXML_STATIC" $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) $(XMLCONF) && make && make install)
  endif
ifndef HAVE_DARWIN_OS
	$(INSTALL_NAME)
endif
	touch $@

CLEAN_FILE += .xml
CLEAN_PKG += xml
DISTCLEAN_PKG += libxml2-$(XML_VERSION).tar.gz

# ***************************************************************************
# twolame
# ***************************************************************************

twolame-$(TWOLAME_VERSION).tar.gz:
	$(WGET) $(TWOLAME_URL)

twolame: twolame-$(TWOLAME_VERSION).tar.gz
	$(EXTRACT_GZ)
ifdef HAVE_WIN32
	(cd twolame/win32; $(WGET) "http://twolame.svn.sourceforge.net/viewvc/*checkout*/twolame/trunk/win32/winutil.h")
endif

.twolame: twolame
	(cd twolame; $(HOSTCC) CFLAGS="${CFLAGS}  -DLIBTWOLAME_STATIC" ./configure $(HOSTCONF) --prefix=$(PREFIX) &&  make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .twolame
CLEAN_PKG += twolame
DISTCLEAN_PKG += twolame-$(TWOLAME_VERSION).tar.gz

# ***************************************************************************
# libpng
# ***************************************************************************

libpng-$(PNG_VERSION).tar.bz2:
	$(WGET) $(PNG_URL)

libpng: libpng-$(PNG_VERSION).tar.bz2
	$(EXTRACT_BZ2)
ifdef HAVE_WIN32
	(cd $@; cat ../Patches/libpng-win32.patch | sed s,??PREFIX??,$(PREFIX), | patch -p0)
else
	(cat Patches/libpng-osx.patch | sed -e 's%??PREFIX??%$(PREFIX)%' -e 's%??EXTRA_CFLAGS??%$(EXTRA_CFLAGS)%' -e 's%??EXTRA_LDFLAGS??%$(EXTRA_LDFLAGS)%' | patch -p0)
endif
	(patch -p0 < Patches/libpng-makefile.patch)
ifdef HAVE_DARWIN_OS_ON_INTEL
	(patch -p0 < Patches/libpng-darwin-mactel.patch)
endif

.png: libpng .zlib
ifdef HAVE_DARWIN_OS
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
else
ifdef HAVE_WIN32
	(cd $<; rm -f INSTALL; cp scripts/makefile.cygwin Makefile && make $(HOSTCC) && make install-static)
else
ifeq ($(PNG_VERSION),1.2.8)
	(cd $<; PREFIX=$(PREFIX) DESTDIR=$(PREFIX) cp scripts/makefile.linux Makefile && make && make install)
else
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
endif
endif
endif
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .png
CLEAN_PKG += libpng
DISTCLEAN_PKG += libpng-$(PNG_VERSION).tar.bz2

# ***************************************************************************
# libzvbi
# ***************************************************************************

zvbi-$(ZVBI_VERSION).tar.bz2:
	$(WGET) $(ZVBI_URL)

zvbi: zvbi-$(ZVBI_VERSION).tar.bz2
	$(EXTRACT_BZ2)
ifdef HAVE_WIN32
	(cd $@; patch -p1 < ../Patches/zvbi-win32.patch; patch -p1 < ../Patches/zvbi-makefile.patch; autoreconf -ivf)
endif

ifdef HAVE_WIN32
.zvbi: zvbi .pthreads
else
.zvbi: zvbi
endif
ifdef HAVE_DARWIN_OS
	(cd $<; ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="$(CFLAGS) -fnested-functions" && make && make install)
else
ifdef HAVE_WIN32
	(cd $<; ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="$(CFLAGS) -DPTW32_STATIC_LIB --std=gnu99"  && make && make install)
else
	(cd $<; ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="$(CFLAGS)" && make -C src && make -C src install)
endif
endif
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .zvbi
CLEAN_PKG += zvbi
DISTCLEAN_PKG += zvbi-$(ZVBI_VERSION).tar.bz2

# ***************************************************************************
# libraw1394
# ***************************************************************************

libraw1394-$(LIBRAW1394_VERSION).tar.gz:
	$(WGET) $(LIBRAW1394_URL)

libraw1394: libraw1394-$(LIBRAW1394_VERSION).tar.gz
	$(EXTRACT_GZ)

.raw1394: libraw1394
	(cd $<; ./configure --prefix=$(PREFIX) && make && make DESTDIR=$(PREFIX) install)
#	sed 's/^typedef u_int8_t  byte_t;/\/* typedef u_int8_t  byte_t;\*\//'
	touch $@

CLEAN_FILE += .raw1394
CLEAN_PKG += libraw1394
DISTCLEAN_PKG += libraw1394-$(LIBRAW1394_VERSION).tar.gz

# ***************************************************************************
# libdc1394
# ***************************************************************************

libdc1394-$(LIBDC1394_VERSION).tar.gz:
	$(WGET) $(LIBDC1394_URL)

libdc1394: libdc1394-$(LIBDC1394_VERSION).tar.gz
	$(EXTRACT_GZ)

.dc1394: libdc1394
	(cd $<; ./configure --prefix=$(PREFIX) && \
	 patch -p1 < ../Patches/libdc1394-noexamples.patch && \
	 make && make DESTDIR=$(PREFIX) install)
	touch $@

CLEAN_FILE += .dc1394
CLEAN_PKG += libdc1394
DISTCLEAN_PKG += libdc1394-$(LIBDC1394_VERSION).tar.gz

# ***************************************************************************
# gpg-error
# ***************************************************************************

libgpg-error-$(GPGERROR_VERSION).tar.bz2:
	$(WGET) $(GPGERROR_URL)

libgpg-error: libgpg-error-$(GPGERROR_VERSION).tar.bz2
	$(EXTRACT_BZ2)

.gpg-error: libgpg-error
ifdef HAVE_DARWIN_OS_ON_INTEL
	(cd $<; ./autogen.sh)
endif
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-nls --disable-shared --disable-languages && make && make install)
#	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .gpg-error
CLEAN_PKG += libgpg-error
DISTCLEAN_PKG += libgpg-error-$(GPGERROR_VERSION).tar.bz2

# ***************************************************************************
# libgcrypt
# ***************************************************************************

libgcrypt-$(GCRYPT_VERSION).tar.bz2:
	$(WGET) $(GCRYPT_URL)

libgcrypt: libgcrypt-$(GCRYPT_VERSION).tar.bz2
	$(EXTRACT_BZ2)
	patch -p0 < Patches/gcrypt.patch

CIPHDIG= --enable-ciphers=aes,des,rfc2268,arcfour --enable-digests=sha1,md5,rmd160 --enable-publickey-digests=dsa

.gcrypt: libgcrypt .gpg-error
ifdef HAVE_WIN32
	(cd $<; ./autogen.sh && $(HOSTCC) ./configure $(HOSTCONF) --target=i586-mingw32msvc --prefix=$(PREFIX) CFLAGS="$(CFLAGS)" $(CIPHDIG) --disable-shared --enable-static --disable-nls && make && make install)
else
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS) -lgpg-error" $(CIPHDIG) && make && make install)
endif
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .gcrypt
CLEAN_PKG += libgcrypt
DISTCLEAN_PKG += libgcrypt-$(GCRYPT_VERSION).tar.bz2

# ***************************************************************************
# opencdk (requires by gnutls 2.x)
# ***************************************************************************

opencdk-$(OPENCDK_VERSION).tar.bz2:
	$(WGET) $(OPENCDK_URL)

opencdk: opencdk-$(OPENCDK_VERSION).tar.bz2
	$(EXTRACT_BZ2)

.opencdk: opencdk
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="$(CFLAGS)" $(HOSTCC) && make && make install)
	touch $@

CLEAN_FILE += .opencdk
CLEAN_PKG += opencdk
DISTCLEAN_PKG += opencdk-$(OPENCDK_VERSION).tar.bz2

# ***************************************************************************
# gnutls
# ***************************************************************************

gnutls-$(GNUTLS_VERSION).tar.bz2:
	$(WGET) $(GNUTLS_URL)

gnutls: gnutls-$(GNUTLS_VERSION).tar.bz2
	$(EXTRACT_BZ2)
ifdef HAVE_WIN32
	patch -p0 < Patches/gnutls-win32.patch
	(cd $@; autoreconf)
endif

.gnutls: gnutls .gcrypt .gpg-error
ifdef HAVE_WIN32
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="$(CFLAGS)" --target=i586-mingw32msvc --disable-cxx -disable-shared --enable-static --disable-nls --with-included-opencdk --with-included-libtasn1 &&  cd gl && make && cd ../lib && make && make install )
else
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="$(CFLAGS)" --disable-cxx --with-included-opencdk --disable-guile && make && make install)
endif
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .gnutls
CLEAN_PKG += gnutls
DISTCLEAN_PKG += gnutls-$(GNUTLS_VERSION).tar.bz2

# ***************************************************************************
# libopendaap
# ***************************************************************************

libopendaap-$(DAAP_VERSION).tar.bz2:
	$(WGET) $(DAAP_URL)

libopendaap: libopendaap-$(DAAP_VERSION).tar.bz2
	$(EXTRACT_BZ2)
	patch -p0 < Patches/daap.patch

.opendaap: libopendaap
	(cd $<; ./configure --prefix=$(PREFIX) CFLAGS="$(CFLAGS) -D_BSD_SOCKLEN_T_=int" && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .opendaap
CLEAN_PKG += libopendaap
DISTCLEAN_PKG += libopendaap-$(DAAP_VERSION).tar.bz2

# ***************************************************************************
# glib
# ***************************************************************************

glib-$(GLIB_VERSION).tar.bz2:
	$(WGET) $(GLIB_URL)

glib: glib-$(GLIB_VERSION).tar.bz2
	$(EXTRACT_BZ2)
	(cd $@; patch -p0 < ../Patches/glib-remove-gtkdoc.patch)
ifdef HAVE_DARWIN_64
	(cd $@; patch -p0 < ../Patches/glib-Darwin64.patch)
endif

.glib: glib
	(cd $<; sh autogen.sh --prefix=$(PREFIX) --host=$(HOST) && make && make install)
	touch $@

CLEAN_FILE += .glib
CLEAN_PKG += glib
DISTCLEAN_PKG += glib-$(GLIB_VERSION).tar.gz

# ***************************************************************************
# libIDL
# ***************************************************************************

libIDL-$(LIBIDL_VERSION).tar.bz2:
	$(WGET) $(LIBIDL_URL)

libIDL: libIDL-$(LIBIDL_VERSION).tar.bz2
	$(EXTRACT_BZ2)

.IDL: libIDL
	(cd $<; ./configure --prefix=$(PREFIX) --host=$(HOST) && make && make install )
	touch $@

CLEAN_FILE += .IDL
CLEAN_PKG += libIDL
DISTCLEAN_PKG += libIDL-$(LIBIDL_VERSION).tar.gz

# ***************************************************************************
# Gecko SDK
# ***************************************************************************

gecko-sdk-ppc-macosx10.2-1.7.5.tar.gz:
	$(WGET) $(GECKO_SDK_MAC_URL)

gecko-sdk: gecko-sdk-ppc-macosx10.2-1.7.5.tar.gz
	$(EXTRACT_GZ)

.gecko: gecko-sdk
	rm -rf ../gecko-sdk
	mv gecko-sdk ..
	mkdir gecko-sdk #creating an empty dir is faster than copying the whole dir
	(cd $<)
	touch $@

CLEAN_FILE += .gecko
CLEAN_PKG += gecko-sdk
DISTCLEAN_PKG += gecko-sdk-ppc-macosx10.2-1.7.5.tar.gz

# ***************************************************************************
# Gecko SDK win32
# ***************************************************************************

gecko-sdk-i586-pc-msvc-1.8b1.zip:
	$(WGET) $(GECKO_SDK_WIN32_URL)

libIDL-0.6.3-win32-bin.zip:
	$(WGET) $(LIBIDL_WIN32_BIN_URL)

glib-19990228.zip:
	$(WGET) $(GLIB_WIN32_BIN_URL)

.gecko-win32: gecko-sdk-i586-pc-msvc-1.8b1.zip libIDL-0.6.3-win32-bin.zip glib-19990228.zip
	unzip gecko-sdk-i586-pc-msvc-1.8b1.zip
	rm -rf ../gecko-sdk
	mv gecko-sdk ..
	unzip -j libIDL-0.6.3-win32-bin.zip libIDL-0.6.3-win32-bin/libIDL-0.6.dll -d ../gecko-sdk/bin
	unzip glib-19990228.zip glib-1.2.dll -d ../gecko-sdk/bin
	chmod a+x ../gecko-sdk/bin/*
	touch $@

CLEAN_FILE += .gecko-win32
CLEAN_PKG += gecko-sdk-win32
DISTCLEAN_PKG += gecko-sdk-i586-pc-msvc-1.8b1.zip libIDL-0.6.3-win32-bin.zip glib-19990228.zip

# ***************************************************************************
# mozilla-macosx
#
# Note that this dependency needs to be compiled against the 10.4u SDK using
# using GCC-4.0. Using newer releases of one of these won't lead to a 
# successful compilation! Adapt the respective variables in ../config.mak!
# ***************************************************************************

firefox-$(MOZILLA_VERSION)-source.tar.bz2:
	$(WGET) $(MOZILLA_URL)

mozilla: firefox-$(MOZILLA_VERSION)-source.tar.bz2
	$(EXTRACT_BZ2)
	(cd $@; patch -p0 < ../Patches/firefox-darwin.patch)

.mozilla-macosx: mozilla .glib .IDL
	ifneq($(SDK_TARGET),"10.4")
	echo "Compiling the mozilla target requires the 10.4 SDK!"
	exit -1
	endif
	(cd $<; ./configure --prefix=$(PREFIX)/gecko-sdk-new --enable-default-toolkit=cairo-cocoa --enable-standalone-modules=xpcom,plugin  --enable-application=xulrunner && make && make install)
	touch $@

CLEAN_FILE += .mozilla-macosx
CLEAN_PKG += mozilla
DISTCLEAN_PKG += firefox-source-$(MOZILLA_VERSION).tar.bz2

# ***************************************************************************
# libjpeg
# ***************************************************************************

jpegsrc.v$(JPEG_VERSION).tar.gz:
	$(WGET) $(JPEG_URL)

jpeg-7: jpegsrc.v$(JPEG_VERSION).tar.gz
	$(EXTRACT_GZ)

.jpeg: jpeg-7
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="$(CFLAGS)" $(HOSTCC) && make && make install)
	$(RANLIB) $(PREFIX)/lib/libjpeg.a
	touch $@

CLEAN_FILE += .jpeg
CLEAN_PKG += jpeg-7
DISTCLEAN_PKG += jpegsrc.v$(JPEG_VERSION).tar.gz

# ***************************************************************************
# tiff
# ***************************************************************************

tiff-$(TIFF_VERSION).tar.gz:
	$(WGET) $(TIFF_URL)

tiff: tiff-$(TIFF_VERSION).tar.gz
	$(EXTRACT_GZ)

.tiff: tiff
ifdef HAVE_WIN32
	(cd $<; ./configure  --target=i586-mingw32msvc --with-CFLAGS="$(CFLAGS)" --with-JPEG=no --with-ZIP=no --prefix=$(PREFIX) --host=$(HOST) &&make -C port && make -C libtiff && make -C libtiff install)
else
	(cd $<; ./configure --with-CFLAGS="$(CFLAGS)" --with-JPEG=no --with-ZIP=no --prefix=$(PREFIX) && make -C port &&make -C libtiff && make -C libtiff install)
endif
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .tiff
CLEAN_PKG += tiff
DISTCLEAN_PKG += tiff-$(TIFF_VERSION).tar.gz

# ***************************************************************************
# LibSDL
# ***************************************************************************

ifndef HAVE_DARWIN_OS
SDL-$(SDL_VERSION).tar.gz:
	$(WGET) $(SDL_URL)

SDL: SDL-$(SDL_VERSION).tar.gz
	$(EXTRACT_GZ)
else
SDL:
	$(SVN) co http://svn.libsdl.org/trunk/SDL -r 4444 SDL
	(cd $@; sh autogen.sh)
endif

.SDL: SDL
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-audio --disable-video-x11 --disable-video-aalib --disable-video-dga --disable-video-fbcon --disable-video-directfb --disable-video-ggi --disable-video-svga --disable-directx --enable-joystick --disable-cdrom --disable-threads --disable-sdl-dlopen CFLAGS="$(CFLAGS)" && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .SDL
CLEAN_PKG += SDL
DISTCLEAN_PKG += SDL-$(SDL_VERSION).tar.gz

# ***************************************************************************
# SDL_image
# ***************************************************************************

SDL_image-$(SDL_IMAGE_VERSION).tar.gz:
	$(WGET) $(SDL_IMAGE_URL)

SDL_image: SDL_image-$(SDL_IMAGE_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p0 < Patches/SDL_image.patch

.SDL_image: SDL_image .SDL .png .jpeg .tiff
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="$(CFLAGS)" --enable-tif --disable-sdltest && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .SDL_image
CLEAN_PKG += SDL_image
DISTCLEAN_PKG += SDL_image-$(SDL_IMAGE_VERSION).tar.gz

# ***************************************************************************
# Musepack decoder library (libmpcdec)
# ***************************************************************************

libmpcdec-$(MUSE_VERSION).tar.bz2:
	$(WGET) $(MUSE_URL)

mpcdec: libmpcdec-$(MUSE_VERSION).tar.bz2
	$(EXTRACT_BZ2)
	patch -p0 < Patches/mpcdec.patch
	(cd $@; autoreconf -ivf)

.mpcdec: mpcdec
ifdef HAVE_WIN32
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS=-D_PTRDIFF_T=mpc_int32_t && make && make install)
else
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
endif
ifdef HAVE_DARWIN_OS
	$(INSTALL_NAME)
endif
	touch $@

CLEAN_FILE += .mpcdec
CLEAN_PKG += mpcdec
DISTCLEAN_PKG += libmpcdec-$(MUSE_VERSION).tar.bz2

# ***************************************************************************
# Dirac
# ***************************************************************************

dirac-$(DIRAC_VERSION).tar.gz:
	$(WGET) $(DIRAC_URL)

dirac: dirac-$(DIRAC_VERSION).tar.gz
	$(EXTRACT_GZ)
ifdef HAVE_CYGWIN
	# incorrect compile warning with cygwin compiler 3.4.4
	patch -p0 <Patches/dirac-cygwin.patch
endif
ifndef HAVE_DARWIN_OS
	(cd $@; aclocal -I m4 && autoconf && automake)
else
	(cd $@; ./bootstrap)
endif

DIRAC_SUBDIRS = libdirac_byteio libdirac_common libdirac_motionest libdirac_encoder libdirac_decoder

ifdef HAVE_WIN32
DIRAC_SUBDIRS += win32
endif

.dirac: dirac
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX)  CFLAGS="$(CFLAGS)" && DOC_DIR="" make $(DIRAC_SUBDIRS) && DOC_DIR="" make install)
	touch $@

# for MacOS X, dirac is split into two libraries, which needs be installed using two targets
.dirac_encoder: .dirac
	$(INSTALL_NAME)
	touch $@

.dirac_decoder: .dirac
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .dirac
CLEAN_PKG += dirac
DISTCLEAN_PKG += dirac-$(DIRAC_VERSION).tar.gz

# *************************************************************************
# DirectX headers
# *************************************************************************
win32-dx7headers.tgz:
	$(WGET) $(DX_HEADERS_URL)
.dx_headers: win32-dx7headers.tgz
	(cd ../include ; tar xzf ../src/$<)
	touch $@
CLEAN_FILE += .dx_headers
DISTCLEAN_PKG += win32-dx7headers.tgz

# *************************************************************************
# DirectShow headers
# *************************************************************************
dshow-headers.tgz:
	$(WGET) $(DSHOW_HEADERS_URL)
.dshow_headers: dshow-headers.tgz
	(cd ../include ; tar xzf ../src/$<)
	touch $@
CLEAN_FILE += .dshow_headers
DISTCLEAN_PKG += dshow-headers.tgz

# ***************************************************************************
# libexpat
# ***************************************************************************

expat-$(EXPAT_VERSION).tar.gz:
	$(WGET) $(EXPAT_URL)

expat: expat-$(EXPAT_VERSION).tar.gz
	$(EXTRACT_GZ)

.expat: expat
ifdef HAVE_DARWIN_OS
	(cd $< aclocal --force && libtoolize --force && autoconf --force && autoheader --force )
endif
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
	touch $@

CLEAN_FILE += .expat
CLEAN_PKG += expat
DISTCLEAN_PKG += expat-$(EXPAT_VERSION).tar.gz

# ***************************************************************************
# CyberLink C++ UPnP library
# ***************************************************************************

clinkcc$(CLINKCC_VERSION).tar.gz:
	$(WGET) $(CLINKCC_URL)

clinkcc: clinkcc$(CLINKCC_VERSION).tar.gz
	tar xzf $<
	mv $(patsubst %.tar.gz,Cyberlink,$(patsubst %.tgz,Cyberlink,$(notdir $<))) $@ || true
	touch $@
	patch -p0 < Patches/clinkcc.patch
ifdef HAVE_DARWIN_OS
	patch -p0 < Patches/clinkcc_osx.patch
endif
	(cd $@; ./bootstrap)

.clinkcc: clinkcc
	(cd $<;./configure --prefix=$(PREFIX) --enable-expat && make && chmod 755 config/install-sh && make install )
	touch $@

CLEAN_FILE += .clinkcc
CLEAN_PKG += clinkcc
DISTCLEAN_PKG += clinkcc$(CLINKCC_VERSION).tar.gz

# ***************************************************************************
# YASM assembler
# ***************************************************************************

yasm-$(YASM_VERSION).tar.gz:
	$(WGET) $(YASM_URL)

yasm: yasm-$(YASM_VERSION).tar.gz
	$(EXTRACT_GZ)

.yasm: yasm
	(cd $< && $(HOSTCC) ./configure --prefix=$(PREFIX) --host=$(HOST) && make && make install)
	touch $@

CLEAN_FILE += .yasm
CLEAN_PKG += yasm
DISTCLEAN_PKG += yasm-$(YASM_VERSION).tar.gz

# ***************************************************************************
# kate
# ***************************************************************************

libkate-$(KATE_VERSION).tar.gz:
	$(WGET) $(KATE_URL)

libkate: libkate-$(KATE_VERSION).tar.gz
	$(EXTRACT_GZ)

.kate: libkate .ogg
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-valgrind --disable-doc && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .kate
CLEAN_PKG += libkate
DISTCLEAN_PKG += libkate-$(KATE_VERSION).tar.gz

# ***************************************************************************
# tiger
# ***************************************************************************

libtiger-$(TIGER_VERSION).tar.gz:
	$(WGET) $(TIGER_URL)

libtiger: libtiger-$(TIGER_VERSION).tar.gz
	$(EXTRACT_GZ)

.tiger: libtiger .kate
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .tiger
CLEAN_PKG += libtiger
DISTCLEAN_PKG += libtiger-$(TIGER_VERSION).tar.gz

# ***************************************************************************
# Copy aclocal files
# This is necessary for --missing aclocal to succeed after a
# configure.ac/Makefile.am change in the vlc root dir
# **************************************************************************

.aclocal:
	cp -R $(PREFIX)/share/aclocal/* $(PREFIX)/../../m4/
	touch $@

CLEAN_FILE += .aclocal

# ***************************************************************************
# TagLib read and editing of tags of popular audio formats
# ***************************************************************************

taglib-$(TAGLIB_VERSION).tar.gz:
	$(WGET) $(TAGLIB_URL)

taglib: taglib-$(TAGLIB_VERSION).tar.gz
	$(EXTRACT_GZ)
ifdef HAVE_WIN32
	patch -p0 < Patches/taglib-static.patch
endif
ifdef HAVE_CYGWIN
	patch -p0 < Patches/taglib-cygwin.patch
endif

.tag: taglib
	(cd $<; $(HOSTCC)  ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .tag
CLEAN_PKG += taglib
DISTCLEAN_PKG += taglib-$(TAGLIB_VERSION).tar.gz

# ***************************************************************************
# pthreads for win32
# ***************************************************************************

pthreads-w32-$(PTHREADS_VERSION)-release.tar.gz:
	$(WGET) $(PTHREADS_URL)

pthreads: pthreads-w32-$(PTHREADS_VERSION)-release.tar.gz
	$(EXTRACT_GZ)
	(cd $@; patch -p0 < ../Patches/pthreads-detach.patch)

.pthreads: pthreads
	(cd $<; $(HOSTCC) make $(PTHREADSCONF) GC GC-static && mkdir -p $(PREFIX)/include && cp -v pthread.h sched.h semaphore.h $(PREFIX)/include/ && mkdir -p $(PREFIX)/lib && cp -v *.a *.dll $(PREFIX)/lib/)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .pthreads
CLEAN_PKG += pthreads
DISTCLEAN_PKG += pthreads-w32-$(PTHREADS_VERSION)-release.tar.gz

# ***************************************************************************
# ncurses library (with wide chars support)
# ***************************************************************************

ncurses-$(NCURSES_VERSION).tar.gz:
	$(WGET) $(NCURSES_URL)

ncurses: ncurses-$(NCURSES_VERSION).tar.gz
	$(EXTRACT_GZ)

.ncurses: ncurses
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --without-debug --enable-widec --without-develop --without-shared && make && make install)
	touch $@

CLEAN_FILE += .ncurses
CLEAN_PKG += ncurses
DISTCLEAN_PKG += ncurses-$(NCURSES_VERSION).tar.gz

# ***************************************************************************
# FluidSynth library (Midi)
# ***************************************************************************

fluidsynth-$(FLUID_VERSION).tar.gz:
	$(WGET) $(FLUID_URL)

fluidsynth: fluidsynth-$(FLUID_VERSION).tar.gz
	$(EXTRACT_GZ)
ifdef HAVE_WIN32
	patch -p0 < Patches/fluid-win32.patch
	cd $@; ./autogen.sh || true
endif

.fluid: fluidsynth
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
	touch $@

CLEAN_FILE += .fluid
CLEAN_PKG += fluidsynth
DISTCLEAN_PKG += fluidsynth-$(FLUID_VERSION).tar.gz

# ***************************************************************************
# liboil
# ***************************************************************************

liboil-$(OIL_VERSION).tar.gz:
	$(WGET) $(OIL_URL)

liboil: liboil-$(OIL_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -d liboil -p1 < Patches/liboil-noshared.patch
	patch -d liboil -p1 < Patches/liboil-win64.patch
	patch -d liboil -p1 < Patches/liboil-Xcompile.patch
	(cd $@; autoreconf)



.liboil: liboil
ifdef HAVE_DARWIN_OS
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="$(CFLAGS)")
else
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="$(CFLAGS) -O2")
endif
	(cd $<; make && make install)
	touch $@

CLEAN_FILE += .liboil
CLEAN_PKG += liboil
DISTCLEAN_PKG += liboil-$(OIL_VERSION).tar.gz

# ***************************************************************************
# Schroedinger library
# ***************************************************************************

schroedinger-$(SCHROED_VERSION).tar.gz:
	$(WGET) $(SCHROED_URL)

schroedinger: schroedinger-$(SCHROED_VERSION).tar.gz
	$(EXTRACT_GZ)

.schroedinger: schroedinger .liboil
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --with-thread=none --disable-gstreamer --prefix=$(PREFIX) CFLAGS="$(CFLAGS) -O3" && make -C schroedinger && make -C schroedinger install)
	touch $@

CLEAN_FILE += .schroedinger
CLEAN_PKG += schroedinger
DISTCLEAN_PKG += schroedinger-$(SCHROED_VERSION).tar.gz

# ***************************************************************************
# ASS library
# ***************************************************************************

libass-$(ASS_VERSION).tar.bz2:
	$(WGET) $(ASS_URL)

libass:
	$(GIT) clone $(ASS_GIT)
	(cd $@; autoreconf -ivf)

.libass: libass .freetype
	(cd $<; $(HOSTCC) ./configure --disable-png $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="$(CFLAGS) -O3" && make && make install)
	touch $@

CLEAN_FILE += .libass
CLEAN_PKG += libass
DISTCLEAN_PKG += libass-$(ASS_VERSION).tar.bz2

# ***************************************************************************
# Sparkle
# ***************************************************************************

Sparkle-$(SPARKLE_VERSION).zip:
	$(WGET) $(SPARKLE_URL)

.Sparkle: Sparkle-$(SPARKLE_VERSION).zip
	rm -rf $@ || true
	unzip $<
	rm -rf ../Sparkle
	mv Sparkle ..
	touch $@

CLEAN_FILE += .Sparkle
CLEAN_PKG += Sparkle
DISTCLEAN_PKG += Sparkle-$(SPARKLE_VERSION).zip

# ***************************************************************************
# UPNP library
# ***************************************************************************

libupnp-$(UPNP_VERSION).tar.bz2:
	$(WGET) $(UPNP_URL)

libupnp: libupnp-$(UPNP_VERSION).tar.bz2
	$(EXTRACT_BZ2)

.libupnp: libupnp
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-samples --without-documentation --disable-webserver CFLAGS="$(CFLAGS) -O3" && make && make install)
	touch $@

CLEAN_FILE += .libupnp
CLEAN_PKG += libupnp
DISTCLEAN_PKG += libupnp-$(UPNP_VERSION).tar.bz2

# *********
# GSM
# *********
gsm-$(GSM_VERSION).tar.gz:
	$(WGET) $(GSM_URL)

gsm: gsm-$(GSM_VERSION).tar.gz
	rm -rf $@ || true
	gunzip -c $< | tar xf - --exclude='[*?:<>\|]'
	mv gsm-1.0-* gsm || true
ifneq ($(HOST),$(BUILD))
	(patch -p0 < Patches/gsm-cross.patch)
endif
ifdef HAVE_DARWIN_OS
	(cd $@; sed -e 's%-O2%-O2\ $(EXTRA_CFLAGS)\ $(EXTRA_LDFLAGS)%' -e 's%# LDFLAGS 	=%LDFLAGS 	= $(EXTRA_LDFLAGS)%' -e 's%gcc%$(CC)%' -i.orig  Makefile)
endif

.gsm: gsm
	(cd $<; $(HOSTCC) make && cp inc/gsm.h $(PREFIX)/include && cp lib/libgsm.a  $(PREFIX)/lib)
	touch $@

CLEAN_FILE += .gsm
CLEAN_PKG += gsm
DISTCLEAN_PKG += gsm-$(GSM_VERSION).tar.gz

# *********
# projectM
# *********
ifdef SVN
libprojectM:
	$(SVN) co $(LIBPROJECTM_SVN) libprojectM
	patch -d libprojectM -p0 < Patches/libprojectM-fpic.patch
else
libprojectM-$(LIBPROJECTM_VERSION).tar.bz2:
	$(WGET) $(LIBPROJECTM_URL)

libprojectM: libprojectM-$(LIBPROJECTM_VERSION).tar.bz2
	$(EXTRACT_BZ2)
endif

.libprojectM: libprojectM
	(cd $</src; $(HOSTCC) CPPFLAGS="$(CPPFLAGS)" cmake . -DUSE_FBO:STRING=FALSE -DUSE_FBO:BOOL=OFF -DINCLUDE-PROJECTM-LIBVISUAL:BOOL=OFF -DINCLUDE-PROJECTM-PULSEAUDIO:BOOL=OFF -DINCLUDE-PROJECTM-QT:BOOL=OFF -DCMAKE_INSTALL_PREFIX=$(PREFIX) && make install)
	touch $@

CLEAN_FILE += .libprojectM
CLEAN_PKG += libprojectM
DISTCLEAN_PKG += libprojectM-$(LIBPROJECTM_VERSION).tar.bz2

# ***************************************************************************
# X11 C Bindings
# ***************************************************************************

libxcb-$(XCB_VERSION).tar.bz2:
	$(WGET) $(XCB_URL)/$@

libxcb: libxcb-$(XCB_VERSION).tar.bz2
	-rm -rf $@
	bzcat $< | tar xf -
	mv libxcb-$(XCB_VERSION) $@
	touch $@

.xcb: libxcb
	cd $< && $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) \
		--disable-composite \
		--disable-damage \
		--disable-dpms \
		--disable-glx \
		--enable-randr \
		--disable-record \
		--enable-render \
		--disable-resource \
		--disable-screensaver \
		--disable-shape \
		--enable-shm \
		--disable-sync \
		--disable-xevie \
		--disable-xfixes \
		--disable-xfree86-dri \
		--disable-xinerama \
		--disable-xinput \
		--disable-xprint \
		--disable-selinux \
		--disable-xtest \
		--enable-xv \
		--disable-xvmc \
		--disable-build-docs
	cd $< && make install
	touch $@

xcb-util-$(XCB_UTIL_VERSION).tar.bz2:
	$(WGET) $(XCB_UTIL_URL)/$@

xcb-util: xcb-util-$(XCB_UTIL_VERSION).tar.bz2
	-rm -rf $@
	bzcat $< | tar xf -
	mv xcb-util-$(XCB_UTIL_VERSION) $@
	touch $@

.xcb-util: xcb-util .xcb
	cd $< && $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX)
	cd $< && make install
	touch $@

CLEAN_FILES += .xcb-util .xcb
CLEAN_PKG += xcb-util xcb
DISTCLEAN_PKG += xcb-util-$(XCB_UTIL_VERSION).tar.bz2 libxcb-$(XCB_VERSION).tar.bz2

# ***************************************************************************
# Peflags utility (Windows only)
# ***************************************************************************

peflags/peflags.c:
	mkdir -p peflags
	cd peflags && $(WGET) $(PEFLAGS_URL)/peflags.c

.peflags: peflags/peflags.c
	cd peflags && gcc peflags.c -o peflags
	install -d $(PREFIX)/bin
	cd peflags && install ./peflags $(PREFIX)/bin
	touch $@

CLEAN_PKG += peflags
CLEAN_FILES += .peflags

# ************
# Regex
# ************

regex-$(REGEX_VERSION).tar.gz:
	$(WGET) $(REGEX_URL)

regex: regex-$(REGEX_VERSION).tar.gz
	$(EXTRACT_GZ)

.regex: regex
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && make subirs= && $(AR) ru libregex.a regex.o && $(RANLIB) libregex.a && cp -v regex.h $(PREFIX)/include && cp -v libregex.a $(PREFIX)/lib )

CLEAN_FILE += .regex
CLEAN_PKG += regex
DISTCLEAN_PKG += regex-$(REGEX_VERSION).tar.gz


# ***************************************************************************
# Some cleaning
# ***************************************************************************

clean-dots: FORCE
	rm -f $(CLEAN_FILE)

clean: clean-dots
	rm -rf $(CLEAN_PKG)

clean-src: clean
	rm -rf $(DISTCLEAN_PKG)

clean-svn:
	rm -rf ffmpeg tremor x264 libdca pa_snapshot_v$(PORTAUDIO_VERSION).tar.gz portaudio live555-$(LIVEDOTCOM_VERSION).tar.gz live libass

distclean: clean-src

# ***************************************************************************
# Download all the sources and package unversionned copies of subversion trees
# ***************************************************************************

source: $(SOURCE) $(DISTCLEAN_PKG)
