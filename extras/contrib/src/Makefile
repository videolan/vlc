#***************************************************************************
# src/Makefile : Dearchive and compile all files necessary
# ***************************************************************************
# Copyright (C) 2003 - 2007 the VideoLAN team
# $Id$
#
# Authors: Christophe Massiot <massiot@via.ecp.fr>
#          Derk-Jan Hartman <hartman at videolan dot org>
#          Eric Petit <titer@m0k.org>
#          Felix Kühne <fkuehne@users.sourceforge.net>
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
export DYLD_LIBRARY_PATH = $(PREFIX)/lib
# following is required for configure scripts that check dyld libraries
# by building and running a test app 
export DYLD_LIBRARY_PATH := $(DYLD_LIBRARY_PATH):$(PREFIX)/vlc-lib
export MACOSX_DEPLOYMENT_TARGET = $(SDK_TARGET)
export LIBRARY_PATH := $(PREFIX)/lib:$(LIBRARY_PATH)
export CFLAGS = -I$(PREFIX)/include $(EXTRA_CFLAGS) $(EXTRA_CPPFLAGS)
export CPPFLAGS = -I$(PREFIX)/include $(EXTRA_CFLAGS) $(EXTRA_CPPFLAGS)
export CXXFLAGS = -I$(PREFIX)/include $(EXTRA_CFLAGS) $(EXTRA_CPPFLAGS)
export LDFLAGS = -L$(PREFIX)/lib $(EXTRA_LDFLAGS)

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

# For ffmpeg if not cross compiling
ifneq ($(CC),)
  FFMPEGCONF=--cc="$(CC)"
endif

#
# cross compiling
#
#This should be inside the if block but some config scripts are buggy
HOSTCONF=--target=$(HOST) --host=$(HOST) --build=$(BUILD) --program-prefix=""
ifneq ($(BUILD),$(HOST))
    #
    # Compiling for MinGW under Cygwin could be deemed as cross compiling
    # unfortunately there isn't a complete separate GCC toolchain for MinGW under Cygwin
    #
    ifndef HAVE_CYGWIN
        # We are REALLY cross compiling
        FFMPEGCONF=--cross-prefix=$(HOST)- --cross-compile
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
HOSTCONF+= --without-pic --disable-shared --enable-msw --with-included-opencdk --with-included-libtasn1 --disable-dependency-tracking
FFMPEGCONF+= --target-os=mingw32 --enable-memalign-hack --cpu=i686 --disable-debug
endif

ifdef HAVE_WINCE
HOSTCONF+= --without-pic --disable-shared
FFMPEGCONF+= --enable-mingwce --cpu=armv4l --disable-debug --disable-encoders --disable-muxers --disable-mpegaudio-hp --disable-decoder=snow --disable-decoder=vc9 --disable-decoder=wmv3 --disable-decoder=vorbis --disable-decoder=dvdsub --disable-decoder=dvbsub --disable-protocols
endif

ifdef HAVE_UCLIBC
ifdef HAVE_BIGENDIAN
FFMPEGCONF+= --arch=armeb
else
FFMPEGCONF+= --arch=armv4l
endif
FFMPEGCONF+= --enable-small --disable-debug --disable-encoders --disable-muxers --disable-demuxer=mpegts --disable-v4l --disable-v4l2 --disable-bktr --disable-dv1394 --disable-mpegaudio-hp --disable-decoder=snow --disable-decoder=vc9 --disable-decoder=wmv3 --disable-decoder=vorbis --disable-decoder=dvdsub --disable-decoder=dvbsub --disable-protocols --disable-encoders
FFMPEG_CFLAGS += -DHAVE_LRINTF --std=c99
else
FFMPEGCONF+= --enable-libmp3lame --enable-libfaac --enable-liba52 --enable-swscaler
endif

ifdef HAVE_BEOS
FFMPEGCONF+= --cpu=x86 --disable-debug --disable-audio-beos
endif

ifdef HAVE_DARWIN_OS_ON_INTEL
FFMPEGCONF += --enable-memalign-hack --cpu=pentium-m
endif

ifdef HAVE_DARWIN_OS
X264CONF += --enable-pic
ifdef HAVE_DARWIN_OS_ON_INTEL
FFMPEG_CFLAGS += -DHAVE_LRINTF
endif
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

ifdef HAVE_DARWIN_OS
# the original libtoto.dylib is copied from contrib/lib to vlc-lib as
# vlc_libtoto.dylib. a soft link to this library is created in contrib/lib
# using the orginal dylib name. if libtoto.la also exists, the library named
# in dlname is also changed to a soft-link to vlc-lib in order to support
# libtool. to satisfy VLC.app dyld search, vlc_libtoto.dylib dyld id is changed
# to @executable_path/lib/vlc_libtoto.dylib 

define INSTALL_NAME
	if which install_name_tool >/dev/null; then \
		mkdir -p $(PREFIX)/vlc-lib ; \
		libname='lib$(patsubst .%,%,$@)' ; \
		dlname="$$libname.dylib" ; \
		cp $(PREFIX)/lib/$$dlname \
			$(PREFIX)/vlc-lib/vlc_$$dlname ; \
		rm $(PREFIX)/lib/$$dlname ; \
		ln -s $(PREFIX)/vlc-lib/vlc_$$dlname \
			$(PREFIX)/lib/$$dlname ; \
		if [ -f $(PREFIX)/lib/$$libname.la ]; then \
			eval ltname=`sed -n 's/dlname=//p' $(PREFIX)/lib/$$libname.la`; \
			rm $(PREFIX)/lib/$$ltname ; \
			ln -s $(PREFIX)/vlc-lib/vlc_$$dlname \
				$(PREFIX)/lib/$$ltname ; \
		fi ; \
		install_name_tool \
			-id @executable_path/lib/vlc_$$dlname \
			$(PREFIX)/vlc-lib/vlc_$$dlname ; \
	fi
endef

endif

ifeq ($(HOST),i586-pc-beos)
# We test if the file actually exists, as sometimes (but not always)
# libtool won't accept to build the shared library
define INSTALL_NAME
	if test -f $(PREFIX)/lib/lib$(patsubst .%,%,$@).so ; then \
		mkdir -p $(PREFIX)/vlc-lib ; \
		cp $(PREFIX)/lib/lib$(patsubst .%,%,$@).so \
		  $(PREFIX)/vlc-lib/ ; \
	fi
endef
endif

# ***************************************************************************
# perl
# ***************************************************************************

ifeq ($(BUILD),i586-pc-beos)
perl-$(PERL_VERSION).tar.gz:
	$(WGET) $(PERL_URL)

perl: perl-$(PERL_VERSION).tar.gz
	$(EXTRACT_GZ)
	sed -i.orig 's%prefix="/boot/home/config"%prefix="$(PREFIX)"%'  perl/hints/beos.sh

.perl: perl
	(cd $<; ./Configure -d -e && make && make install && ln -sf perl5/$(PERL_VERSION)/BePC-beos/CORE/libperl.so $(PREFIX)/lib/libperl.so)
	touch $@

CLEAN_FILE += .perl
CLEAN_PKG += perl
DISTCLEAN_PKG += perl-$(PERL_VERSION).tar.gz
endif

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

ifeq ($(BUILD),i586-pc-beos)
libtool-$(LIBTOOL_VERSION).tar.gz:
	$(WGET) $(LIBTOOL_URL)

libtool: libtool-$(LIBTOOL_VERSION).tar.gz
	$(EXTRACT_GZ)

.libtool: libtool
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
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

.pkgcfg: pkgconfig
	(cd pkgconfig; autoconf && ./configure --prefix=$(PREFIX) --disable-shared --enable-static && make && make install)
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
ifdef HAVE_DARWIN_OS
	patch -p0 < Patches/gettext-macosx.patch 
endif
ifdef HAVE_WIN32
	patch -p0 < Patches/gettext-win32.patch
endif

.intl: gettext .iconv
ifdef HAVE_WIN32
	( cd $< && $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-relocatable --with-iconv-prefix=$(PREFIX) --disable-java --disable-native-java)
else
	( cd $< && $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-java --disable-native-java --without-emacs)
endif
ifeq ($(BUILD),i586-pc-beos)
# The ugliest kludge ever - so libtool correctly links shared libraries
# on BeOS. Sorry, I just can't figure out how libtool works
	( cd $< && for f in `find . -name libtool`; do  sed -e 's/ -shared / -nostart /' -e 's/^predep_objects.*/predep_objects=""/' -i  $$f; chmod +x $$f; done )
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
ifdef HAVE_DARWIN_OS
	patch -p0 < Patches/libiconv.patch
endif
	patch -p0 < Patches/libiconv-autoconf-uclibc.patch

ifdef HAVE_DARWIN_OS
.iconv:
	touch $@
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

fontconfig: fontconfig-$(FONTCONFIG_VERSION).tar.gz Makefile Patches/fontconfig.patch
	$(EXTRACT_GZ)
	patch -p0 < Patches/fontconfig.patch

.fontconfig: fontconfig
ifdef HAVE_WIN32
  ifdef HAVE_CYGWIN
	(cd $<; LIBXML2_CFLAGS=`$(PREFIX)/bin/xml2-config --cflags` ./configure --target=$(HOST) --disable-pic --disable-shared --with-cache-dir=WINDOWSTEMPDIR --with-arch=i686 --prefix=$(PREFIX) --with-freetype-config=$(PREFIX)/bin/freetype-config --enable-libxml2 && make && make install)
  else
	(cd $<; $(HOSTCC) LIBXML2_CFLAGS=`$(PREFIX)/bin/xml2-config --cflags` ./configure $(HOSTCONF) --with-cache-dir=WINDOWSTEMPDIR --with-arch=i686 --prefix=$(PREFIX) --with-freetype-config=$(PREFIX)/bin/freetype-config --enable-libxml2 && make && make install)
  endif
else
  ifdef HAVE_DARWIN_OS
	(cd $<; $(HOSTCC) LIBXML2_CFLAGS=`xml2-config --cflags` LIBXML2_LIBS=`xml2-config --libs` ./configure $(HOSTCONF) --with-cache-dir=/var/cache/fontconfig --with-confdir=/etc/fonts --prefix=$(PREFIX) --with-freetype-config=$(PREFIX)/bin/freetype-config --enable-libxml2 && make && make install-exec && (cd fontconfig ; make install-data))
  else
	(cd $<; $(HOSTCC) LIBXML2_CFLAGS=`$(PREFIX)/bin/xml2-config --cflags` ./configure $(HOSTCONF) --prefix=$(PREFIX) --with-freetype-config=$(PREFIX)/bin/freetype-config --enable-libxml2 && make && make install)
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
	patch -p0 < Patches/freetype-cross.patch

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

fribidi-$(FRIBIDI_VERSION).tar.bz2:
	$(WGET) $(FRIBIDI_URL)

fribidi: fribidi-$(FRIBIDI_VERSION).tar.bz2
	$(EXTRACT_BZ2)
	patch -p0 < Patches/fribidi.patch.real
ifdef HAVE_WIN32
	patch -p0 <Patches/fribidi-win32.patch
endif
ifdef HAVE_UCLIBC
	patch -p0 < Patches/fribidi-uclibc.patch
endif

.fribidi: fribidi .iconv
	(cd $<; rm -f configure; ./bootstrap && $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX)  && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .fribidi
CLEAN_PKG += fribidi
DISTCLEAN_PKG += fribidi-$(FRIBIDI_VERSION).tar.bz2

# ***************************************************************************
# liba52
# ***************************************************************************

a52dec-$(A52DEC_VERSION).tar.gz:
	$(WGET) $(A52DEC_URL)

a52dec: a52dec-$(A52DEC_VERSION).tar.gz
	$(EXTRACT_GZ)

.a52: a52dec
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && cd liba52 && make && make install && cd ../include && make && make install)
	touch $@

CLEAN_FILE += .a52
CLEAN_PKG += a52dec
DISTCLEAN_PKG += a52dec-$(A52DEC_VERSION).tar.gz

# ***************************************************************************
# mpeg2dec
# ***************************************************************************

#mpeg2dec:
#	@echo "*** Please press return here : ***"
#	cvs -d $(MPEG2DEC_CVSROOT) login
#	cvs -d $(MPEG2DEC_CVSROOT) co mpeg2dec
#	(cd $@; ./bootstrap)

mpeg2dec-$(MPEG2DEC_VERSION).tar.gz:
	$(WGET) $(MPEG2DEC_URL)

# It is easier to use bootstrapped versions...
#mpeg2dec-$(MPEG2DEC_VERSION).tar.gz:
#	$(WGET) $(MPEG2DEC_SNAPSHOT)
#	mv mpeg2dec-$(MPEG2DEC_DATE).tar.gz $@

mpeg2dec: mpeg2dec-$(MPEG2DEC_VERSION).tar.gz
	$(EXTRACT_GZ)

.mpeg2: mpeg2dec
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --without-x --disable-sdl && cd libmpeg2 && make && make install && cd ../include && make && make install)
	touch $@

CLEAN_FILE += .mpeg2
CLEAN_PKG += mpeg2dec
#DISTCLEAN_PKG += mpeg2dec-$(MPEG2DEC_VERSION)-cvs.tar.gz
DISTCLEAN_PKG += mpeg2dec-$(MPEG2DEC_VERSION).tar.gz

# ***************************************************************************
# libid3tag
# ***************************************************************************

libid3tag-$(LIBID3TAG_VERSION).tar.gz:
	$(WGET) $(LIBID3TAG_URL)

libid3tag: libid3tag-$(LIBID3TAG_VERSION).tar.gz
	$(EXTRACT_GZ)

.id3tag: libid3tag
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .id3tag
CLEAN_PKG += libid3tag
DISTCLEAN_PKG += libid3tag-$(LIBID3TAG_VERSION).tar.gz

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

.lua: lua
ifdef HAVE_WIN32
	( cd $<; sed -i.orig 's/lua luac/lua.exe/' Makefile ; cd src; sed -i.orig 's/CC=/#CC=/' Makefile; cd ../..;)
	(cd $<; $(HOSTCC) make $(LUA_MAKEPLATEFORM); cd src; $(HOSTCC) make liblua.a; cd ..;$(HOSTCC) make install INSTALL_TOP=$(PREFIX); $(RANLIB) $(PREFIX)/lib/liblua.a)
	(cd $<; sed -i.orig 's@prefix= /usr/local@prefix= $(PREFIX)@' etc/lua.pc; mkdir -p $(PREFIX)/lib/pkgconfig; cp etc/lua.pc $(PREFIX)/lib/pkgconfig)
else 
	(cd $<; $(HOSTCC) make $(LUA_MAKEPLATEFORM) && make install INSTALL_TOP=$(PREFIX))
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

.mad: libmad
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="-O3" && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .mad
CLEAN_PKG += libmad
DISTCLEAN_PKG += libmad-$(LIBMAD_VERSION).tar.gz

# ***************************************************************************
# ogg
# ***************************************************************************

#ogg:
#	@echo "*** Please type \"anoncvs\" here : ***"
#	cvs -d $(OGG_CVSROOT) login
#	cvs -d $(OGG_CVSROOT) co ogg
#	(cd $@; ./autogen.sh)

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

#vorbis:
#	@echo "*** Please type \"anoncvs\" here : ***"
#	cvs -d $(OGG_CVSROOT) login
#	cvs -d $(OGG_CVSROOT) co vorbis
#	(cd $@; ./autogen.sh)
#	patch -p0 < Patches/vorbis.patch

libvorbis-$(VORBIS_VERSION).tar.gz:
	$(WGET) $(VORBIS_URL)

libvorbis: libvorbis-$(VORBIS_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p0 < Patches/libvorbis.patch
	(cd $@; autoconf)

.vorbis: libvorbis .ogg
# FIXME: shared libs disabled for now because of linking errors
#   -- titer 2005/09/16
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-shared && make && make install)
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
	$(SVN) export http://svn.xiph.org/trunk/Tremor tremor
	(cd $@ && patch -p0 < ../Patches/tremor.patch)
	(cd $@; rm -f ogg.h && echo "#include <ogg/ogg.h>" > ogg.h && rm -f os_types.h && echo "#include <ogg/os_types.h>" > os_types.h && ./autogen.sh)
else
tremor-$(TREMOR_VERSION).tar.bz2:
	echo "tremor snapshot does not exist, you MUST use subversion !"
	exit -1
	$(WGET) $(TREMOR_URL)

tremor: tremor-$(TREMOR_VERSION).tar.bz2
	$(EXTRACT_BZ2)
endif

.tremor: tremor .ogg
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-shared && make && make install)
	$(INSTALL_NAME)
	touch $@

ifdef SVN
tremor-source: tremor
	tar cfvz tremor-$(DATE).tar.gz tremor

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
	patch -p0 < Patches/theora.patch
	(cd $@; autoconf)

.theora: libtheora .ogg
ifdef HAVE_BEOS
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-sdltest --disable-oggtest --disable-vorbistest --disable-asm --disable-examples && make && make install)
else
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-sdltest --disable-oggtest --disable-vorbistest --disable-examples && make && make install)

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
	(cd $@; autoreconf && automake)
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
ifdef HAVE_BEOS
	patch -p0 < Patches/speex-BeOS.patch
endif

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

#faad2:
#	@echo "*** Please type return here : ***"
#	cvs -d $(FAAD2_CVSROOT) login
#	cvs -d $(FAAD2_CVSROOT) co faad2
#	(cd $@; patch -p1 < ../Patches/faad2.patch)
#	(cd $@; sh ./bootstrap)

faad2-$(FAAD2_VERSION).tar.bz2:
	$(WGET) $(FAAD2_URL)

faad: faad2-$(FAAD2_VERSION).tar.bz2
	$(EXTRACT_BZ2)
	(cd $@; patch -p1 < ../Patches/faad2.patch && ./bootstrap)

.faad: faad
	(cd $< && $(HOSTCC) ./configure $(HOSTCONF) --disable-shared --prefix=$(PREFIX) CFLAGS="-O3" && sed -i.orig "s/shrext_cmds/shrext/g" libtool && make -C libfaad && make -C libfaad install)
	touch $@

CLEAN_FILE += .faad
CLEAN_PKG += faad
DISTCLEAN_PKG += faad2-$(FAAD2_VERSION).tar.bz2

# ***************************************************************************
# faac
# ***************************************************************************

faac-$(FAAC_VERSION).tar.gz:
	$(WGET) $(FAAC_URL)

faac: faac-$(FAAC_VERSION).tar.gz
	$(EXTRACT_GZ)

.faac: faac
	(cd $<; ./bootstrap && $(HOSTCC) ./configure $(HOSTCONF) --without-mp4v2 --prefix=$(PREFIX) --disable-shared && make && make install)
	touch $@

CLEAN_FILE += .faac
CLEAN_PKG += faac
DISTCLEAN_PKG += faac-$(FAAC_VERSION).tar.gz

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

## No reasons to use SVN anymore
#ifdef SVN
#libebml:
#	$(SVN) export http://svn.matroska.org/svn/matroska/trunk/libebml libebml
#else
libebml: libebml-$(LIBEBML_VERSION).tar.bz2
	$(EXTRACT_BZ2)
#endif

ifdef HAVE_WIN32
	(cd $@; patch -p0 < ../Patches/libebml-svn-win32.patch)
endif

.ebml: libebml
ifdef HAVE_WIN32
	(cd $<; make -C make/mingw32 prefix=$(PREFIX) $(HOSTCC2) CFLAGS="$(CFLAGS) -DWIN32" SHARED=no && make -C make/linux install_staticlib install_headers prefix=$(PREFIX) $(HOSTCC2))
else
	(cd $<; make -C make/linux prefix=$(PREFIX) $(HOSTCC2) staticlib && make -C make/linux install_staticlib install_headers prefix=$(PREFIX))
	$(RANLIB) $(PREFIX)/lib/libebml.a
endif	
	touch $@
## No reasons to use SVN anymore
#ifdef SVN
#libebml-source: libebml
#	tar cfvz  libebml-$(DATE).tar.gz libebml
#SOURCE += libebml-source	
#endif

CLEAN_FILE += .ebml
CLEAN_PKG += libebml
DISTCLEAN_PKG += libebml-$(LIBEBML_VERSION).tar.bz2

# ***************************************************************************
# libmatroska
# ***************************************************************************

libmatroska-$(LIBMATROSKA_VERSION).tar.bz2:
	$(WGET) $(LIBMATROSKA_URL)

## No reasons to use SVN anymore
#if SVN
#libmatroska:
#	$(SVN) export http://svn.matroska.org/svn/matroska/trunk/libmatroska libmatroska
#else
libmatroska: libmatroska-$(LIBMATROSKA_VERSION).tar.bz2
	$(EXTRACT_BZ2)
#endif 

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

## No reasons to use SVN anymore
#ifdef SVN
#libmatroska-source: libmatroska
#	tar cfvz libmatroska-$(DATE).tar.gz libmatroska
#
#SOURCE += libmatroska-source
#endif

CLEAN_FILE += .matroska
CLEAN_PKG += libmatroska
DISTCLEAN_PKG += libmatroska-$(LIBMATROSKA_VERSION).tar.bz2

# ***************************************************************************
# ffmpeg
# ***************************************************************************

ifdef SVN
ffmpeg:
	$(SVN) export $(FFMPEG_SVN)  ffmpeg
	(cd "$@" && patch -p0 < ../Patches/ffmpeg-svn-swscale-pc.patch)
ifeq ($(HOST),i586-pc-beos)
	(cd $@; patch -p0 < ../Patches/ffmpeg-svn-beos.patch)
endif
ifdef HAVE_WINCE
	patch -p1 < Patches/ffmpeg-svn-wince.patch
endif
ifdef HAVE_BEOS
	(cd $@; patch -p0 < ../Patches/ffmpeg-alignment.patch)
endif
ifdef HAVE_DARWIN_OS
	patch -p0 < Patches/ffmpeg-macosx-intel-mmx.patch
endif
ifdef HAVE_UCLIBC
	patch -p0 < Patches/ffmpeg-svn-uclibc.patch
	patch -p0 < Patches/ffmpeg-svn-internal-define.patch
	patch -p0 < Patches/ffmpeg-svn-grab.patch
endif

else
ffmpeg-$(FFMPEG_VERSION).tar.gz:
	echo "ffmpeg snapshot is too old, you MUST use subversion !"
	exit -1
	$(WGET) $(FFMPEG_URL)

ffmpeg: ffmpeg-$(FFMPEG_VERSION).tar.gz
	$(EXTRACT_GZ)
endif

ifdef HAVE_WINCE
.ffmpeg: ffmpeg .zlib
else
ifdef HAVE_UCLIBC
.ffmpeg: ffmpeg
else
.ffmpeg: ffmpeg .faac .lame
endif
endif
	(cd $<; $(HOSTCC) ./configure $(FFMPEGCONF) --prefix=$(PREFIX) --extra-cflags="$(CFLAGS) $(FFMPEG_CFLAGS) -DRUNTIME_CPUDETECT" --extra-ldflags="$(LDFLAGS)" --enable-gpl --enable-pp --disable-vhook --disable-ffserver --disable-ffmpeg --disable-ffplay && make && make install-libs install-headers)
	touch $@

ifdef SVN
ffmpeg-source: ffmpeg
	tar cfvz ffmpeg-$(DATE).tar.gz ffmpeg

SOURCE += ffmpeg-source
endif

CLEAN_FILE += .ffmpeg
CLEAN_PKG += ffmpeg ffmpeg.cvs
DISTCLEAN_PKG += ffmpeg-$(FFMPEG_VERSION).tar.gz

# ***************************************************************************
# libdvdcss
# ***************************************************************************

libdvdcss-$(LIBDVDCSS_VERSION).tar.gz:
	$(WGET) $(LIBDVDCSS_URL)

ifdef SVN
libdvdcss: 
	$(SVN) export svn://svn.videolan.org/libdvdcss/trunk libdvdcss
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
	tar cfvz libdvdcss-$(DATE).tar.gz libdvdcss

SOURCE += libdvdcss-source
endif

CLEAN_FILE += .dvdcss
CLEAN_PKG += libdvdcss
DISTCLEAN_PKG += libdvdcss-$(LIBDVDCSS_VERSION).tar.gz

# ***************************************************************************
# dummy dvdread to trigger the INSTALL_NAME script
# ***************************************************************************
.dvdread: .dvdnav
	$(INSTALL_NAME)
	(install_name_tool -change $(PREFIX)/lib/libdvdread.0.dylib @executable_path/lib/vlc_libdvdread.dylib $(PREFIX)/vlc-lib/vlc_libdvdnav.dylib)
	touch $@

CLEAN_FILE += .dvdread

# ***************************************************************************
# libdvdnav
# ***************************************************************************

libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2:
	$(WGET) $(LIBDVDNAV_URL)

libdvdnav: libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2
	$(EXTRACT_BZ2)
	patch -p0 < Patches/dvdnav.patch
	patch -p0 < Patches/libdvdnav-mactel.patch
	(cd $@; ./autogen.sh noconfig)

.dvdnav: libdvdnav .dvdcss
ifdef HAVE_WIN32	
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --enable-static --prefix=$(PREFIX) --with-libdvdcss=$(PREFIX) CFLAGS="$(CFLAGS)" CPPFLAGS="$(CPPFLAGS) -Dssize_t=long" && make && make install)
else
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --enable-static --prefix=$(PREFIX) --with-libdvdcss=$(PREFIX) CFLAGS="$(CFLAGS) -Dlseek64=lseek -Doff64_t=off_t" && make && make install)
endif
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .dvdnav
CLEAN_PKG += libdvdnav
DISTCLEAN_PKG += libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2

# ***************************************************************************
# libdvbpsi
# ***************************************************************************

libdvbpsi4-$(LIBDVBPSI_VERSION).tar.gz:
	$(WGET) $(LIBDVBPSI_URL)

libdvbpsi: libdvbpsi4-$(LIBDVBPSI_VERSION).tar.gz
	$(EXTRACT_GZ)

.dvbpsi: libdvbpsi
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && cd src && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .dvbpsi
CLEAN_PKG += libdvbpsi
DISTCLEAN_PKG += libdvbpsi4-$(LIBDVBPSI_VERSION).tar.gz

# ***************************************************************************
# live
# ***************************************************************************

live555-$(LIVEDOTCOM_VERSION).tar.gz:
	$(WGET) $(LIVEDOTCOM_URL)

live: live555-$(LIVEDOTCOM_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p0 < Patches/live.patch
ifdef HAVE_WIN32 
	patch -p0 < Patches/livedotcom-win32.patch
endif
ifdef HAVE_DARWIN_OS
	patch -p0 < Patches/live-osx.patch
endif
	patch -p0 < Patches/live-starttime.patch
	(cd live/liveMedia/include; \
		mv liveMedia_version.hh liveMedia_version.hh.bak; \
	sed -e 's/.*LIVEMEDIA_LIBRARY_VERSION_INT.*/#define LIVEMEDIA_LIBRARY_VERSION_INT         9999999999/' liveMedia_version.hh.bak > liveMedia_version.hh )
	patch -Np0 < Patches/live-config.patch
ifdef HAVE_UCLIBC
	patch -p0 < Patches/live-noapps.patch
endif

.live: live
ifdef HAVE_WIN32 
	(cd $<;./genMakefiles mingw && make $(HOSTCC))
else
ifdef HAVE_DARWIN_OS
	(cd $<; sed -e 's%EXTRA_CFLAGS%$(EXTRA_CFLAGS)%' -e 's%EXTRA_LDFLAGS%$(EXTRA_LDFLAGS)%' -i.orig  config.macosx)
	(cd $<; ./genMakefiles macosx && make)
else
	(cd $<; sed -e 's/=/= EXTRA_CPPFLAGS/' -e 's%EXTRA_CPPFLAGS%-I/include%' -i.orig groupsock/Makefile.head)
ifdef HAVE_UCLIBC
ifdef HAVE_BIGENDIAN
	(cd $<; ./genMakefiles armeb-uclibc && make $(HOSTCC))
endif
else
	(cd $<; ./genMakefiles linux && make $(HOSTCC))
endif
endif
endif
	touch $@

CLEAN_FILE += .live
CLEAN_PKG += live
DISTCLEAN_PKG += live555-$(LIVEDOTCOM_VERSION).tar.gz

# ***************************************************************************
# goom
# ***************************************************************************

#goom-macosx-altivec-bin.tar.gz:
#	$(WGET) $(GOOM_URL)

#goom: goom-macosx-altivec-bin.tar.gz
#	$(EXTRACT_GZ)

#.goom: goom
#	(cd $<;)
#	touch $@

#CLEAN_FILE += .goom
#CLEAN_PKG += goom
#DISTCLEAN_PKG += goom-macosx-altivec-bin.tar.gz

# ***************************************************************************
# goom2k4
# ***************************************************************************

goom$(GOOM2k4_VERSION).tar.gz:
	$(WGET) $(GOOM2k4_URL)
	mv goom-$(GOOM2k4_VERSION)-src.tar.gz goom$(GOOM2k4_VERSION).tar.gz

goom: goom$(GOOM2k4_VERSION).tar.gz
	$(EXTRACT_GZ)
ifdef HAVE_DARWIN_OS
	patch -p0 < Patches/goom.patch
endif
ifdef HAVE_WIN32
	(cd $@; dos2unix configure.in)
	patch -p0 < Patches/goom2k4-0-win32.patch
else
	patch -p0 < Patches/goom2k4-0-mmx.patch
endif

.goom2k4: goom
ifdef HAVE_DARWIN_OS
	(cd $<; ./configure --prefix=$(PREFIX) --disable-shared --enable-static && make && make install)
else
	(cd $< && rm -f configure; aclocal -I m4/ && automake --add-missing && autoconf configure.in > configure && $(HOSTCC) sh ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-shared --enable-static --disable-glibtest --disable-gtktest && make && make install)
endif
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

.caca: libcaca
ifdef HAVE_WIN32
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --enable-win32 --disable-x11 --disable-imlib2 --disable-doc && make && make install)
else
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-imlib2 --disable-doc && make && make install)
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

.dca: libdca
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
	touch $@

CLEAN_FILE += .dca
CLEAN_PKG += libdca
DISTCLEAN_PKG += libdca-$(LIBDCA_VERSION).tar.bz2

# ***************************************************************************
# libx264
# ***************************************************************************

x264-$(X264_VERSION).tar.gz:
	$(WGET) $(X264_URL)

ifdef SVN
x264: 
	$(SVN) export svn://svn.videolan.org/x264/trunk/ x264
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
	(cd $<; $(HOSTCC) ./configure $(X264CONF) --prefix="$(PREFIX)" && make && make install)
  endif
else
.x264: x264
	(cd $<; $(HOSTCC) ./configure $(X264CONF) --prefix="$(PREFIX)" && make && make install)
endif
	touch $@

ifdef SVN
x264-source: x264
	tar cfvz x264-$(DATE).tar.gz x264

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

.cddb: libcddb
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-shared --enable-static CFLAGS="$(CFLAGS) -D_BSD_SOCKLEN_T_=int" LDFLAGS="$(LDFLAGS) -liconv" && make && make install)
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

.cdio: libcdio
	(cd $<; sed -e 's%@ENABLE_CPP_TRUE@SUBDIRS = C++%@ENABLE_CPP_TRUE@SUBDIRS = %' -i.orig example/Makefile.in && ./configure --prefix=$(PREFIX) --without-vcdinfo --disable-shared && make && make install)
	touch $@

CLEAN_FILE += .cdio
CLEAN_PKG += libcdio
DISTCLEAN_PKG += libcdio-$(CDIO_VERSION).tar.gz

# ***************************************************************************
# wxWidgets
# ***************************************************************************

wxWidgets-$(WXWIDGETS_VERSION).tar.gz:
	$(WGET) $(WXWIDGETS_URL)

wxwidgets: wxWidgets-$(WXWIDGETS_VERSION).tar.gz
	$(EXTRACT_GZ)
	(cd wxwidgets; patch -p0 < ../Patches/wxwidgets_uri.patch)
#	(cd wxwidgets; patch -p1 <../Patches/wxMSW-win32.patch)

.wxwidgets: wxwidgets
	(cd wxwidgets; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --enable-optimise --disable-debug --enable-unicode --disable-threads --disable-gif --disable-pcx --disable-iff --disable-pnm --without-zlib  --without-libpng --without-libjpeg --without-libtiff --without-regex  --disable-zipstream --disable-fs_zip --enable-no_rtti  --enable-no_exceptions --disable-exceptions --disable-stl --disable-html --without-expat --disable-protocols --disable-protocol --disable-xrc --disable-sound --disable-dialupman --disable-mediactrl  --disable-joystick --disable-grid --disable-datepick --disable-postscript--disable-filesystem --disable-fileproto --disable-ipc --disable-sockets --disable-precomp-headers && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .wxwidgets
CLEAN_PKG += wxwidgets
DISTCLEAN_PKG += wxWidgets-$(WXWIDGETS_VERSION).tar.gz

# ***************************************************************************
# qt4 (win32 binary)
# ***************************************************************************

qt4-$(QT4_VERSION)-win32-bin.tar.gz:
	$(WGET) $(QT4_URL)

qt4: qt4-$(QT4_VERSION)-win32-bin.tar.gz
	$(EXTRACT_GZ)

.qt4: qt4
	(cd qt4;mkdir -p $(PREFIX)/bin; mkdir -p $(PREFIX)/include;mkdir -p $(PREFIX)/lib/pkgconfig;rm -f $(PREFIX)/lib/pkgconfig/Qt*; sed 's,@@PREFIX@@,$(PREFIX),' lib/pkgconfig/QtCore.pc.in > $(PREFIX)/lib/pkgconfig/QtCore.pc;sed 's,@@PREFIX@@,$(PREFIX),' lib/pkgconfig/QtGui.pc.in > $(PREFIX)/lib/pkgconfig/QtGui.pc;cp -r include/* $(PREFIX)/include;cp lib/*a $(PREFIX)/lib)
ifeq ($(BUILD),i686-pc-cygwin)
	(cd qt4;cp bin/*.exe $(PREFIX)/bin)
else
	(cd qt4;cp bin/* $(PREFIX)/bin)
endif
	touch $@

CLEAN_FILE += .qt4
CLEAN_PKG += qt4
DISTCLEAN_PKG += qt4-$(QT4_VERSION)-win32-bin.tar.gz

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
ifdef HAVE_WIN32
	patch -p0 < Patches/xml2-win32.patch
	rm -f xml/aclocal.m4
endif
ifdef HAVE_UCLIBC
	patch -p0 < Patches/xml2-uclibc.patch
endif

.xml: xml
ifdef HAVE_WIN32
	(cd xml; aclocal && automake && autoreconf)
endif
  ifdef HAVE_CYGWIN
	(cd xml; ac_cv_header_pthread_h="no" CFLAGS="-DLIBXML_STATIC" $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --with-minimal --with-catalog --with-reader --with-tree --with-push --with-xptr --with-valid --with-xpath --with-xinclude --with-sax1 --without-zlib --without-iconv --without-http --without-ftp && make && make install)
  else
	(cd xml; CFLAGS="-DLIBXML_STATIC" $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --with-minimal --with-catalog --with-reader --with-tree --with-push --with-xptr --with-valid --with-xpath --with-xinclude --with-sax1 --without-zlib --without-iconv --without-http --without-ftp && make && make install)
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
ifdef HAVE_BEOS
	patch -p0 < Patches/twolame-BeOS.patch
endif

.twolame: twolame
ifdef HAVE_WIN32
	(cd twolame/win32; $(WGET) "http://twolame.svn.sourceforge.net/viewvc/*checkout*/twolame/trunk/win32/winutil.h")
endif	
	(cd twolame; $(HOSTCC) CFLAGS="${CFLAGS}  -DLIBTWOLAME_STATIC" ./configure $(HOSTCONF) --prefix=$(PREFIX) && cd libtwolame &&  make && make install)
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

.png: libpng 
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
	$(INSTALL_NAME)
endif 
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
.zvbi: zvbi .pthreads
else
.zvbi: zvbi
endif
ifdef HAVE_DARWIN_OS
	(cd $<; ./configure --prefix=$(PREFIX) CFLAGS="$(CFLAGS) -fnested-functions" && make && make install)
else
	(cd $<; ./configure --prefix=$(PREFIX) CFLAGS="$(CFLAGS)" && make && make install)
endif
	touch $@

CLEAN_FILE += .zvbi
CLEAN_PKG += zvbi
DISTCLEAN_PKG += zvbi-$(ZVBI_VERSION).tar.bz2

# ***************************************************************************
# libraw1394
# ***************************************************************************

#ifdef SVN
#libraw1394:
#	$(SVN) export -r 325 $(LIBDC1394_SVN)/libraw1394/trunk/libraw1394  libraw1394
#        (cd $<; libtoolize --copy --force; \
#                autopoint -f; \
#                aclocal -I extras/contrib/share/aclocal; \
#                autoconf; \
#                autoheader; \
#                automake --add-missing --copy -Wall)
#
#else
libraw1394-$(LIBRAW1394_VERSION).tar.gz:
	$(WGET) $(LIBRAW1394_URL)

libraw1394: libraw1394-$(LIBRAW1394_VERSION).tar.gz
	$(EXTRACT_GZ)
#endif

.raw1394: libraw1394
	(cd $<; ./configure --prefix=$(PREFIX) && make && make DESTDIR=$(PRFIX) install)
#	sed 's/^typedef u_int8_t  byte_t;/\/* typedef u_int8_t  byte_t;\*\//'
	touch $@
	
CLEAN_FILE += .raw1394
CLEAN_PKG += libraw1394
DISTCLEAN_PKG += libraw1394-$(LIBRAW1394_VERSION).tar.gz

# ***************************************************************************
# libdc1394
# ***************************************************************************

#ifdef SVN
#libdc1394:
#	$(SVN) export -r 325 $(LIBDC1394_SVN)/libdc1394/trunk/libdc1394  libdc1394
#        (cd $<; libtoolize --copy --force; \
#                autopoint -f; \
#                aclocal -I extras/contrib/share/aclocal; \
#                autoconf; \
#                autoheader; \
#                automake --add-missing --copy -Wall)
#
#else
libdc1394-$(LIBDC1394_VERSION).tar.gz:
	$(WGET) $(LIBDC1394_URL)

libdc1394: libdc1394-$(LIBDC1394_VERSION).tar.gz
	$(EXTRACT_GZ)
#endif

.dc1394: libdc1394
	(cd $<; ./configure --prefix=$(PREFIX) && \
	 patch -p1 < ../Patches/libdc1394-noexamples.patch && \
	 make && make DESTDIR=$(PRFIX) install)
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
ifdef HAVE_WIN32
	patch -p0 < Patches/libgpg-error-win32.patch
endif

.gpg-error: libgpg-error
ifdef HAVE_DARWIN_OS_ON_INTEL
	(cd $<; ./autogen.sh)
endif
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-nls --disable-shared && make && make install)
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

.gcrypt: libgcrypt .gpg-error
ifdef HAVE_WIN32
	(cd $<; ./autogen.sh && $(HOSTCC) ./configure $(HOSTCONF) --target=i586-mingw32msvc --prefix=$(PREFIX) CFLAGS="$(CFLAGS)" && sed -i 46s@sys/times.h@sys/time.h@ cipher/random.c && make && make install)
else
ifdef HAVE_DARWIN_OS_ON_INTEL
	(cd $<; $(HOSTCC) ./configure --host=$(HOST) --build=$(BUILD) --prefix=$(PREFIX) --disable-asm CFLAGS="$(CFLAGS)" && make && make install)
endif
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="$(CFLAGS)" && make && make install)
endif
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .gcrypt
CLEAN_PKG += libgcrypt
DISTCLEAN_PKG += libgcrypt-$(GCRYPT_VERSION).tar.bz2

# ***************************************************************************
# gnutls
# ***************************************************************************

gnutls-$(GNUTLS_VERSION).tar.bz2:
	$(WGET) $(GNUTLS_URL)

gnutls: gnutls-$(GNUTLS_VERSION).tar.bz2
	$(EXTRACT_BZ2)
ifdef HAVE_WIN32
	patch -p0 < Patches/gnutls-win32.patch
	(cd $@; cd lgl; ln -sf alloca_.h alloca.h) 
endif

.gnutls: gnutls .gcrypt .gpg-error
ifdef HAVE_WIN32
	(cd $<; autoconf && $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="$(CFLAGS)" --target=i586-mingw32msvc && cd gl && make && cd ../lgl && make &&  cd ../lib &&make && make install && cd ../includes && make install)
else
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="$(CFLAGS)" --disable-cxx && cd gl && make && cd ../lgl && make &&  cd ../lib &&make && make install && cd ../includes && make install)
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

glib-$(GLIB_VERSION).tar.gz:
	$(WGET) $(GLIB_URL)

glib: glib-$(GLIB_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p0 < Patches/glib.patch

.glib: glib
	(cd $<; ./configure --prefix=$(PREFIX) --host=powerpc-apple-macos && make && make install)
	touch $@

CLEAN_FILE += .glibl
CLEAN_PKG += glib
DISTCLEAN_PKG += glib-$(GLIB_VERSION).tar.gz

# ***************************************************************************
# libIDL
# ***************************************************************************

libIDL-$(LIBIDL_VERSION).tar.gz:
	$(WGET) $(LIBIDL_URL)

libIDL: libIDL-$(LIBIDL_VERSION).tar.gz
	$(EXTRACT_GZ)

.IDL: libIDL
	(cd $<; ./configure --prefix=$(PREFIX) --host=powerpc-apple-macos && make && make install )
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
# mozilla
# ***************************************************************************

mozilla-source-$(MOZILLA_VERSION).tar.bz2:
	$(WGET) $(MOZILLA_URL)

mozilla: mozilla-source-$(MOZILLA_VERSION).tar.bz2
	$(EXTRACT_BZ2)

.mozilla: mozilla
	(cd $<; ./configure --prefix=$(PREFIX) --enable-default-toolkit=mac --enable-standalone-modules=xpcom,plugin && make && make install)
	touch $@

CLEAN_FILE += .mozilla
CLEAN_PKG += mozilla
DISTCLEAN_PKG += mozilla-source-$(MOZILLA_VERSION).tar.bz2

# ***************************************************************************
# libjpeg
# ***************************************************************************

jpeg-$(JPEG_VERSION).tar.gz:
	$(WGET) $(JPEG_URL)

jpeg: jpeg-$(JPEG_VERSION).tar.gz 
	$(EXTRACT_GZ)

.jpeg: jpeg
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="$(CFLAGS)" $(HOSTCC) && make && make install-lib)
	$(RANLIB) $(PREFIX)/lib/libjpeg.a
	touch $@

CLEAN_FILE += .jpeg
CLEAN_PKG += jpeg
DISTCLEAN_PKG += jpeg-$(JPEG_VERSION).tar.gz

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
# SDL
# ***************************************************************************

SDL-$(SDL_VERSION).tar.gz:
	$(WGET) $(SDL_URL)

SDL: SDL-$(SDL_VERSION).tar.gz 
	$(EXTRACT_GZ)

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
	(cd $@; autoreconf)

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
	patch -p0 <Patches/dirac-cross.patch
ifdef HAVE_CYGWIN
	# incorrect compile warning with cygwin compiler 3.4.4
	patch -p0 <Patches/dirac-cygwin.patch
endif
	(cd $@; aclocal && autoconf && automake)

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

.clinkcc: clinkcc
	(cd $<; ./bootstrap && ./configure --prefix=$(PREFIX) --enable-expat && make && chmod 755 config/install-sh && make install )
	touch $@

CLEAN_FILE += .clinkcc
CLEAN_PKG += clinkcc
DISTCLEAN_PKG += clinkcc$(CLINKCC_VERSION).tar.gz

# ***************************************************************************
# NASM assembler
# ***************************************************************************

nasm-$(NASM_VERSION).tar.bz2:
	$(WGET) $(NASM_URL)

nasm: nasm-$(NASM_VERSION).tar.bz2
	$(EXTRACT_BZ2)

#nasm:
#	echo $(NASM_CVSROOT) A > ./nasm.cvs
#	CVS_PASSFILE=./nasm.cvs cvs -z3 -d $(NASM_CVSROOT) co nasm
#	(cd $@ && autoconf && autoheader)

.nasm: nasm
	(cd $< && $(HOSTCC) ./configure --prefix=$(PREFIX) && make && make install)
	touch $@

CLEAN_FILE += .nasm
CLEAN_PKG += nasm nasm.cvs
DISTCLEAN_PKG += nasm-$(NASM_VERSION).tar.bz2

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
# UNICOWS (Unicode support for WINME/98/95)
# ***************************************************************************

libunicows-$(UNICOWS_VERSION).tar.gz:
	$(WGET) $(UNICOWS_URL)
	mv libunicows-$(UNICOWS_VERSION)-src.tar.gz $@ || true

libunicows: libunicows-$(UNICOWS_VERSION).tar.gz
	$(EXTRACT_GZ)

.unicows: libunicows
	(cd $</src; make -f makefile.mingw32 PATHSEP='/' $(HOSTCC) && cp -f ../lib/mingw32/libunicows.a $(PREFIX)/lib)
	touch $@

CLEAN_FILE += .unicows
CLEAN_PKG += libunicows
DISTCLEAN_PKG += libunicows-$(UNICOWS_VERSION).tar.gz

# ***************************************************************************
# TagLib read and editing of tags of popular audio formats
# ***************************************************************************

taglib-$(TAGLIB_VERSION).tar.gz:
	$(WGET) $(TAGLIB_URL)

taglib: taglib-$(TAGLIB_VERSION).tar.gz
	$(EXTRACT_GZ)
ifdef HAVE_CYGWIN
	patch -p0 <Patches/taglib-cygwin.patch
endif

.tag: taglib
	(cd $<; $(HOSTCC) ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
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

.pthreads: pthreads
	(cd $<; $(HOSTCC) make $(PTHREADSCONF) GC-static && cp -v pthread.h sched.h semaphore.h $(PREFIX)/include/ && cp -v *.a $(PREFIX)/lib/)
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
# Some cleaning
# ***************************************************************************

clean-dots: FORCE
	rm -f $(CLEAN_FILE)

clean: clean-dots
	rm -rf $(CLEAN_PKG)

clean-src: clean
	rm -rf $(DISTCLEAN_PKG)

clean-svn:
	rm -rf ffmpeg tremor x264 libdca

distclean: clean-src

# ***************************************************************************
# Download all the sources and package unversionned copies of subversion trees
# ***************************************************************************

source: $(SOURCE) $(DISTCLEAN_PKG)
