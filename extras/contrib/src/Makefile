# ***************************************************************************
# src/Makefile : Dearchive and compile all files necessary
# ***************************************************************************
# Copyright (C) 2003, 2004, 2005 VideoLAN
# $Id$
#
# Authors: Christophe Massiot <massiot@via.ecp.fr>
#          Derk-Jan Hartman <hartman at videolan dot org>
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
# ***************************************************************************

include ../config.mak
include ./packages.mak

# ***************************************************************************
# Set a clean environment
# ***************************************************************************

ifeq ($(HOST),ppc-darwin)
export PATH = $(PREFIX)/bin:/bin:/usr/bin
export DYLD_LIBRARY_PATH = $(PREFIX)/lib
endif

ifeq ($(HOST),i586-pc-beos)
ifeq ($(HOST),$(BUILD))
# Compiling on BeOS
export PATH = $(PREFIX)/bin:.:/boot/home/config/bin:/bin:/boot/apps:/boot/preferences:/boot/beos/apps:/boot/beos/preferences:/boot/develop/tools/gnupro/bin
export LIBRARY_PATH = $(PREFIX)/lib:%A/lib:/boot/home/config/lib:/boot/beos/system/lib
else
# Cross compiling from Linux
export PATH = $(PREFIX)/bin:/opt/cross-tools/bin:/usr/bin:/bin
endif
endif

export CPPFLAGS = -I$(PREFIX)/include $(EXTRA_CPPFLAGS)
export CFLAGS = -I$(PREFIX)/include $(EXTRA_CPPFLAGS)
export CXXFLAGS = -I$(PREFIX)/include $(EXTRA_CPPFLAGS)
export LDFLAGS = -L$(PREFIX)/lib $(EXTRA_LDFLAGS)

# ***************************************************************************
# Cross compilation variables
# We'll usually use --host=<platform>, except for a few libraries which
# don't handle it (gotta set CC/CXX/etc), and obviously FFmpeg has its own
# way of doing it... ;)
# ***************************************************************************

ifneq ($(BUILD),$(HOST))
# We are cross compiling
HOSTCONF=--host=$(HOST)
HOSTCC=CC=$(HOST)-gcc CXX=$(HOST)-g++ AR=$(HOST)-ar RANLIB=$(HOST)-ranlib
# For libebml/libmatroska. Grrr.
HOSTCC2=CC=$(HOST)-gcc CXX=$(HOST)-g++ "AR=$(HOST)-ar rcvu" RANLIB=$(HOST)-ranlib
FFMPEGCONF=--cross-prefix=$(HOST)-
RANLIB=$(HOST)-ranlib
else
# Building natively
RANLIB=ranlib
endif

# ***************************************************************************
# Standard rules
# ***************************************************************************

ifeq ($(HOST),ppc-darwin)
all: .autoconf .libtool .automake .iconv .intl .pkgcfg .freetype .fribidi \
	.a52 .mpeg2 .id3tag .mad .ogg .vorbis .vorbisenc .theora \
	.FLAC .speex .faad .faac .lame .ebml .matroska .ffmpeg .openslp \
	.dvdcss .dvdread .dvdnav .dvbpsi .live .goom2k4 .caca .mod \
	.png .gpgerror .gcrypt .gnutls .daap .cddb .cdio .vcdimager .aclocal
# mozilla support? add before .aclocal:  .glib .libidl .mozilla
endif

ifeq ($(HOST),i586-pc-beos)
ifeq ($(HOST),$(BUILD))
# Native build: we need perl, autoconf, etc first
all: .perl .autoconf .libtool .automake .iconv .intl .freetype .fribidi \
	.a52 .mpeg2 .id3tag .mad .ogg .vorbis .vorbisenc .theora \
	.FLAC .speex .faad .faac .lame .ebml .matroska .ffmpeg .dvdcss \
	.dvdread .dvdnav .dvbpsi .aclocal
else
# Cross compiling: we already have the Linux tools, only build the
# libraries now
all: .iconv .intl .freetype .fribidi \
	.a52 .mpeg2 .id3tag .mad .ogg .vorbis .vorbisenc .theora \
	.FLAC .speex .faad .faac .lame .ebml .matroska .ffmpeg .dvdcss \
	.dvdread .dvdnav .dvbpsi .aclocal
endif
endif

FORCE:

# ***************************************************************************
# Useful macros
# ***************************************************************************

define EXTRACT_GZ
	tar xzf $<
	mv $(patsubst %.tar.gz,%,$(patsubst %.tgz,%,$(notdir $<))) $@ || true
	touch $@
endef

define EXTRACT_BZ2
	bunzip2 -c $< | tar xf -
	mv $(patsubst %.tar.bz2,%,$(notdir $<)) $@ || true
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

ifeq ($(HOST),ppc-darwin)
define INSTALL_NAME_LIB
	install_name_tool \
		-change $(PREFIX)/lib/$(shell cd $(PREFIX)/lib; ls | grep "lib$(INSTALL_LIB).[0-9]*.dylib") \
		@executable_path/lib/vlc_lib$(INSTALL_LIB).dylib \
		$(PREFIX)/vlc-lib/lib$(patsubst .%,%,$@).dylib ;
endef

define INSTALL_NAME
	if which install_name_tool >/dev/null; then \
		mkdir -p $(PREFIX)/vlc-lib ; \
		cp $(PREFIX)/lib/lib$(patsubst .%,%,$@).dylib \
			$(PREFIX)/vlc-lib/lib$(patsubst .%,%,$@).dylib ; \
		install_name_tool \
			-id @executable_path/lib/vlc_lib$(patsubst .%,%,$@).dylib \
			$(PREFIX)/vlc-lib/lib$(patsubst .%,%,$@).dylib ; \
		$(foreach INSTALL_LIB,$(patsubst .%,%,$(filter .%,$^)), \
			$(INSTALL_NAME_LIB)) \
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
	mv perl/hints/beos.sh perl/hints/beos.sh.orig
	sed 's%prefix="/boot/home/config"%prefix="$(PREFIX)"%' < perl/hints/beos.sh.orig > perl/hints/beos.sh

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

autoconf-$(AUTOCONF_VERSION).tar.gz:
	$(WGET) $(AUTOCONF_URL)

autoconf: autoconf-$(AUTOCONF_VERSION).tar.gz
	$(EXTRACT_GZ)

.autoconf: autoconf
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
	touch $@

CLEAN_FILE += .autoconf
CLEAN_PKG += autoconf
DISTCLEAN_PKG += autoconf-$(AUTOCONF_VERSION).tar.gz

# ***************************************************************************
# libtool
# ***************************************************************************

ifeq ($(BUILD),ppc-darwin)
.libtool:
	if test -x /usr/bin/glibtoolize; then \
		ln -sf /usr/bin/glibtoolize $(PREFIX)/bin/libtoolize ; \
	fi
	touch $@
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
endif

CLEAN_FILE += .libtool

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

pkgconfig-$(PKGCFG_VERSION).tar.gz:
	$(WGET) $(PKGCFG_URL)

pkgconfig: pkgconfig-$(PKGCFG_VERSION).tar.gz
	$(EXTRACT_GZ)

.pkgcfg: pkgconfig
	(cd pkgconfig; ./configure --prefix=$(PREFIX) --disable-shared --enable-static && make && make install)
	touch $@

CLEAN_FILE += .pkgcfg
CLEAN_PKG += pkgconfig
DISTCLEAN_PKG += pkgconfig-$(PKGCFG_VERSION).tar.gz

# ***************************************************************************
# gettext
# ***************************************************************************

gettext-$(GETTEXT_VERSION).tar.gz:
	$(WGET) $(GETTEXT_URL)

gettext: gettext-$(GETTEXT_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p 0 < Patches/gettext.patch

.intl: gettext
	( cd $< && ./configure $(HOSTCONF) --prefix=$(PREFIX) )
ifeq ($(BUILD),i586-pc-beos)
# The ugliest kludge ever - so libtool correctly links shared libraries
# on BeOS. Sorry, I just can't figure out how libtool works
	( cd $< && for f in `find . -name libtool`; do mv $$f $$f.orig; sed -e 's/ -shared / -nostart /' -e 's/^predep_objects.*/predep_objects=""/' < $$f.orig > $$f; chmod +x $$f; rm $$f.orig; done )
endif
ifneq ($(HOST),$(BUILD))
# We'll use the installed gettext and only need to cross-compile libintl
	( cd $< && make -C gettext-runtime/intl && make -C gettext-runtime/intl install )
else
# Build and install the whole gettext
	( cd $< && make && make install )
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

.iconv: libiconv
	(cd libiconv; ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-nls && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .iconv
CLEAN_PKG += libiconv
DISTCLEAN_PKG += libiconv-$(LIBICONV_VERSION).tar.gz

# ***************************************************************************
# freetype2
# ***************************************************************************

freetype-$(FREETYPE2_VERSION).tar.gz:
	$(WGET) $(FREETYPE2_URL)

freetype2: freetype-$(FREETYPE2_VERSION).tar.gz
	$(EXTRACT_GZ)

.freetype: freetype2
	(cd $<; ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
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
	patch -p 0 < Patches/fribidi.patch.real

.fribidi: fribidi
	(cd $<; ./bootstrap && ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
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

.a52: a52dec
	(cd $<; ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
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

#mpeg2dec-$(MPEG2DEC_VERSION).tar.gz:
#	$(WGET) $(MPEG2DEC_URL)

# It is easier to use bootstrapped versions...
mpeg2dec-$(MPEG2DEC_VERSION).tar.gz:
	$(WGET) $(MPEG2DEC_SNAPSHOT)
	mv mpeg2dec-snapshot.tar.gz $@

mpeg2dec: mpeg2dec-$(MPEG2DEC_VERSION).tar.gz
	$(EXTRACT_GZ)
#	patch -p 0 < Patches/mpeg2dec.patch

.mpeg2: mpeg2dec
	(cd $<; ./configure $(HOSTCONF) --prefix=$(PREFIX) --without-x --disable-dsl && make && make install)
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
	(cd $<; ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .id3tag
CLEAN_PKG += libid3tag
DISTCLEAN_PKG += libid3tag-$(LIBID3TAG_VERSION).tar.gz

# ***************************************************************************
# libmad
# ***************************************************************************

libmad-$(LIBMAD_VERSION).tar.gz:
	$(WGET) $(LIBMAD_URL)

libmad: libmad-$(LIBMAD_VERSION).tar.gz
	$(EXTRACT_GZ)

.mad: libmad
	(cd $<; ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="-O3" && make && make install)
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

.ogg: libogg
	(cd $<; ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
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
#	patch -p 0 < Patches/vorbis.patch

libvorbis-$(VORBIS_VERSION).tar.gz:
	$(WGET) $(VORBIS_URL)

libvorbis: libvorbis-$(VORBIS_VERSION).tar.gz
	$(EXTRACT_GZ)

.vorbis: libvorbis .ogg
# libvorbis' configure doesn't handle --host
# FIXME: shared libs disabled for now because configure thinks we should
# link the .so to -lc when cross-compiling to BeOS (-> fails)
#   -- titer 2005/02/12
	(cd $<; $(HOSTCC) ./configure --prefix=$(PREFIX) --disable-shared && make && make install)
#	$(INSTALL_NAME)
	touch $@

.vorbisenc: .vorbis .ogg
#	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .vorbis .vorbisenc
CLEAN_PKG += libvorbis
DISTCLEAN_PKG += libvorbis-$(VORBIS_VERSION).tar.gz

# ***************************************************************************
# theora
# ***************************************************************************

libtheora-$(THEORA_VERSION).tar.bz2:
	$(WGET) $(THEORA_URL)

libtheora: libtheora-$(THEORA_VERSION).tar.bz2
	$(EXTRACT_BZ2)
	patch -p 0 < Patches/theora.patch

.theora: libtheora
	(cd $<; ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .theora
CLEAN_PKG += libtheora
DISTCLEAN_PKG += libtheora-$(THEORA_VERSION).tar.bz2

# ***************************************************************************
# flac
# ***************************************************************************

flac-$(FLAC_VERSION).tar.gz:
	$(WGET) $(FLAC_URL)

flac: flac-$(FLAC_VERSION).tar.gz
	$(EXTRACT_GZ)

.FLAC: flac
	(cd $<; ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
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
	patch -p 0 < Patches/speex.patch

.speex: speex
	(cd $<; ./configure $(HOSTCONF) --prefix=$(PREFIX) --enable-ogg=no && make && make install)
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
#	(cd $@; patch -p 0 < ../Patches/faad2.patch)
#	(cd $@; sh ./bootstrap)

faad2-$(FAAD2_VERSION).tar.bz2:
	$(WGET) $(FAAD2_URL)

faad2: faad2-$(FAAD2_VERSION).tar.bz2
	$(EXTRACT_BZ2)

.faad: faad2
	(cd $<; ./configure $(HOSTCONF) --prefix=$(PREFIX) CFLAGS="-O3" && make -C libfaad && make -C libfaad install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .faad
CLEAN_PKG += faad2
DISTCLEAN_PKG += faad2-$(FAAD2_VERSION).tar.bz2

# ***************************************************************************
# faac
# ***************************************************************************

faac-$(FAAC_VERSION).tar.bz2:
	$(WGET) $(FAAC_URL)

faac: faac-$(FAAC_VERSION).tar.bz2
	$(EXTRACT_BZ2)
	patch -p 0 < Patches/faac.patch

.faac: faac
	(cd $<; rm -f install-sh mkinstalldirs missing && ./bootstrap && ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-shared && make && make install)
	touch $@

CLEAN_FILE += .faac
CLEAN_PKG += faac
DISTCLEAN_PKG += faac-$(FAAC_VERSION).tar.bz2

# ***************************************************************************
# lame
# ***************************************************************************

lame-$(LAME_VERSION).tar.gz:
	$(WGET) $(LAME_URL)

lame: lame-$(LAME_VERSION).tar.gz
	$(EXTRACT_GZ)

.lame: lame
	(cd $<; ./configure $(HOSTCONF) --prefix=$(PREFIX) --disable-shared && make && make install)
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
	-rm -rf $@/CVS

.ebml: libebml
	(cd $<; make -C make/linux prefix=$(PREFIX) $(HOSTCC2) && make -C make/linux install prefix=$(PREFIX))
	$(RANLIB) $(PREFIX)/lib/libebml.a
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
	-rm -rf $@/CVS

.matroska: libmatroska
	(cd $<; make -C make/linux prefix=$(PREFIX) $(HOSTCC2) && make -C make/linux install prefix=$(PREFIX))
	$(RANLIB) $(PREFIX)/lib/libmatroska.a
	touch $@

CLEAN_FILE += .matroska
CLEAN_PKG += libmatroska
DISTCLEAN_PKG += libmatroska-$(LIBMATROSKA_VERSION).tar.bz2

# ***************************************************************************
# ffmpeg
# ***************************************************************************

ffmpeg:
	echo $(FFMPEG_CVSROOT) A > ./ffmpeg.cvs
	CVS_PASSFILE=./ffmpeg.cvs cvs -z3 -d $(FFMPEG_CVSROOT) co ffmpeg
	patch -p 0 < Patches/ffmpeg.patch.cvs
#	(cd $@; ./bootstrap)

ffmpeg-$(FFMPEG_VERSION).tar.gz:
	$(WGET) $(FFMPEG_URL)

#ffmpeg: ffmpeg-$(FFMPEG_VERSION).tar.gz
#	$(EXTRACT_GZ)
#	patch -p 0 < Patches/ffmpeg.patch

.ffmpeg: ffmpeg
	(cd $<; ./configure $(FFMPEGCONF) --prefix=$(PREFIX) --extra-cflags="$(CFLAGS) -DRUNTIME_CPUDETECT" --extra-ldflags="$(LDFLAGS)" --enable-gpl --enable-mp3lame --enable-faac --enable-pp --disable-vhook --disable-audio-beos --disable-ffserver --disable-vorbis --disable-faad --enable-mp3lame --enable-zlib --enable-a52 && make && make -C libavcodec/libpostproc install && make -C libavcodec installlib && make -C libavformat installlib)
	touch $@

CLEAN_FILE += .ffmpeg
CLEAN_PKG += ffmpeg
DISTCLEAN_PKG += ffmpeg-$(FFMPEG_VERSION).tar.gz

# ***************************************************************************
# openslp
# ***************************************************************************

openslp-$(OPENSLP_VERSION).tar.gz:
	$(WGET) $(OPENSLP_URL)

openslp: openslp-$(OPENSLP_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p 0 < Patches/openslp.patch

.openslp: openslp
	(cd $<; ./configure --prefix=$(PREFIX) && make SUBDIRS="common libslpattr slpd libslp slptool" && make install SUBDIRS="common libslpattr slpd libslp slptool")
	touch $@

CLEAN_FILE += .openslp
CLEAN_PKG += openslp
DISTCLEAN_PKG += openslp-$(OPENSLP_VERSION).tar.gz

# ***************************************************************************
# libdvdcss
# ***************************************************************************

libdvdcss-$(LIBDVDCSS_VERSION).tar.gz:
	$(WGET) $(LIBDVDCSS_URL)

libdvdcss: libdvdcss-$(LIBDVDCSS_VERSION).tar.gz
	$(EXTRACT_GZ)

.dvdcss: libdvdcss
	(cd $<; ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .dvdcss
CLEAN_PKG += libdvdcss
DISTCLEAN_PKG += libdvdcss-$(LIBDVDCSS_VERSION).tar.gz

# ***************************************************************************
# libdvdread
# ***************************************************************************

libdvdread-$(LIBDVDREAD_VERSION).tar.gz:
	$(WGET) $(LIBDVDREAD_URL)

libdvdread: libdvdread-$(LIBDVDREAD_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p 0 < Patches/libdvdread.patch

.dvdread: libdvdread .dvdcss
	(cd $<; ./configure $(HOSTCONF) --prefix=$(PREFIX) --with-libdvdcss=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .dvdread
CLEAN_PKG += libdvdread
DISTCLEAN_PKG += libdvdread-$(LIBDVDREAD_VERSION).tar.gz

# ***************************************************************************
# libdvdnav
# ***************************************************************************

libdvdnav-$(LIBDVDNAV_VERSION).tar.gz:
	$(WGET) $(LIBDVDNAV_URL)

libdvdnav: libdvdnav-$(LIBDVDNAV_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p 0 < Patches/dvdnav.patch
ifeq ($(BUILD),ppc-darwin)
	(cd $@; autoconf)
else
	(cd $@; ./autogen.sh noconfig)
endif

.dvdnav: libdvdnav .dvdread
	(cd $<; ./configure $(HOSTCONF) --enable-static --prefix=$(PREFIX) --with-libdvdcss=$(PREFIX) CFLAGS="$(CFLAGS) -Dlseek64=lseek -Doff64_t=off_t" && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .dvdnav
CLEAN_PKG += libdvdnav
DISTCLEAN_PKG += libdvdnav-$(LIBDVDNAV_VERSION).tar.gz

# ***************************************************************************
# libdvbpsi
# ***************************************************************************

libdvbpsi3-$(LIBDVBPSI_VERSION).tar.gz:
	$(WGET) $(LIBDVBPSI_URL)

libdvbpsi: libdvbpsi3-$(LIBDVBPSI_VERSION).tar.gz
	$(EXTRACT_GZ)

.dvbpsi: libdvbpsi
	(cd $<; ./configure $(HOSTCONF) --prefix=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .dvbpsi
CLEAN_PKG += libdvbpsi
DISTCLEAN_PKG += libdvbpsi-$(LIBDVBPSI_VERSION).tar.gz

# ***************************************************************************
# live
# ***************************************************************************

live.$(LIVEDOTCOM_VERSION).tar.gz:
	$(WGET) $(LIVEDOTCOM_URL)

live: live.$(LIVEDOTCOM_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p 0 < Patches/live.patch

.live: live
	(cd $<; ./genmakefiles macosx && make )
	touch $@

CLEAN_FILE += .live
CLEAN_PKG += live
DISTCLEAN_PKG += live.$(LIVEDOTCOM_VERSION).tar.gz

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
	patch -p 0 < Patches/goom.patch

.goom2k4: goom
	(cd $<; ./configure --prefix=$(PREFIX) --disable-shared --enable-static && make && make install )
	touch $@

CLEAN_FILE += .goom2k4
CLEAN_PKG += goom
DISTCLEAN_PKG += goom$(GOOM2k_VESION).tar.gz

# ***************************************************************************
# libcaca
# ***************************************************************************

libcaca-$(LIBCACA_VERSION).tar.gz:
	$(WGET) $(LIBCACA_URL)

libcaca: libcaca-$(LIBCACA_VERSION).tar.gz
	$(EXTRACT_GZ)

.caca: libcaca
	(cd $<; ./configure --prefix=$(PREFIX) --disable-imlib2 --disable-doc && make && make install)
	touch $@

CLEAN_FILE += .caca
CLEAN_PKG += libcaca
DISTCLEAN_PKG += libcaca-$(LIBCACA_VERSION).tar.gz

# ***************************************************************************
# libdts
# ***************************************************************************

libdts-$(LIBDTS_VERSION).tar.gz:
	$(WGET) $(LIBDTS_URL)

ifeq ($(HOST),i586-pc-beos)
# libdts 0.0.2 doesn't compile on BeOS
libdts:
	svn co svn://svn.videolan.org/libdts/trunk libdts
	(cd $@; ./bootstrap)
else
libdts: libdts-$(LIBDTS_VERSION).tar.gz
	$(EXTRACT_GZ)
endif

.dts: libdts
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
	touch $@

CLEAN_FILE += .dts
CLEAN_PKG += libdts
DISTCLEAN_PKG += libdts-$(LIBDTS_VERSION).tar.gz

# ***************************************************************************
# libmodplug
# ***************************************************************************

libmodplug-$(MODPLUG_VERSION).tar.gz:
	$(WGET) $(MODPLUG_URL)

libmodplug: libmodplug-$(MODPLUG_VERSION).tar.gz
	$(EXTRACT_GZ)

.mod: libmodplug
	(cd $<; ./configure --prefix=$(PREFIX) --disable-shared --enable-static && make && make install)
	touch $@

CLEAN_FILE += .mod
CLEAN_PKG += libmodplug
DISTCLEAN_PKG += libmodplug-$(MODPLUG_VERSION).tar.gz

# ***************************************************************************
# libcddb
# ***************************************************************************

libcddb-$(CDDB_VERSION).tar.gz:
	$(WGET) $(CDDB_URL)

libcddb: libcddb-$(CDDB_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p 0 < Patches/libcddb.patch

.cddb: libcddb
	(cd $<; ./configure --prefix=$(PREFIX) --disable-shared --enable-static CFLAGS="$(CFLAGS) -D_BSD_SOCKLEN_T_=int" LDFLAGS="$(LDFLAGS) -liconv" && make && make install)
	touch $@

CLEAN_FILE += .cddb
CLEAN_PKG += libcddb
DISTCLEAN_PKG += libcddb-$(CDDB_VERSION).tar.gz

# ***************************************************************************
# vcdimager
# ***************************************************************************

vcdimager-$(VCDIMAGER_VERSION).tar.gz:
	$(WGET) $(VCDIMAGER_URL)

vcdimager: vcdimager-$(VCDIMAGER_VERSION).tar.gz
	$(EXTRACT_GZ)

ifeq ($(HOST),ppc-darwin)
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
	(cd $<; ./configure --prefix=$(PREFIX) --without-vcdinfo && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .cdio
CLEAN_PKG += libcdio
DISTCLEAN_PKG += libcdio-$(CDIO_VERSION).tar.gz

# ***************************************************************************
# OpenMash
# ***************************************************************************

mash-src-$(MASH_VERSION).tar.gz:
	$(WGET) $(MASH_URL)

mash-code: mash-src-$(MASH_VERSION).tar.gz
	$(EXTRACT_GZ)

.mash: mash-code
	(cd $<; ./configure && make )
	touch $@

CLEAN_FILE += .mash
CLEAN_PKG += mash-code
DISTCLEAN_PKG += mash-src-$(MASH_VERSION).tar.gz

# ***************************************************************************
# toolame
# ***************************************************************************

toolame-$(TOOLAME_VERSION).tar.bz2:
	$(WGET) $(TOOLAME_URL)

toolame: toolame-$(TOOLAME_VERSION).tar.bz2
	$(EXTRACT_BZ2)

.toolame: toolame
	(cd toolame; ./configure --prefix=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .toolame
CLEAN_PKG += toolame
DISTCLEAN_PKG += toolame-$(TOOLAME_VERSION).tar.bz2

# ***************************************************************************
# libpng
# ***************************************************************************

libpng-$(PNG_VERSION).tar.bz2:
	$(WGET) $(PNG_URL)

libpng: libpng-$(PNG_VERSION).tar.bz2
	$(EXTRACT_BZ2)
	cat Patches/libpng.patch | sed s,PREFIX,$(PREFIX), | patch -p0

.png: libpng
	(cd $<; cp scripts/makefile.macosx Makefile && make shared && make install)
	touch $@

CLEAN_FILE += .png
CLEAN_PKG += libpng
DISCLEAN_PKG += libpng-$(PNG_VERSION).tar.bz2

# ***************************************************************************
# libzvbi
# ***************************************************************************

zvbi-$(ZVBI_VERSION).tar.bz2:
	$(WGET) $(ZVBI_URL)

zvbi: zvbi-$(ZVBI_VERSION).tar.bz2
	$(EXTRACT_BZ2)

.zvbi: zvbi
	(cd $<; ./configure --prefix=$(PREFIX) && make )
	touch $@

CLEAN_FILE += .zvbi
CLEAN_PKG += zvbi
DISCLEAN_PKG += zvbi-$(ZVBI_VERSION).tar.bz2

# ***************************************************************************
# gpg-error 
# ***************************************************************************

libgpg-error-$(GPGERROR_VERSION).tar.gz:
	$(WGET) $(GPGERROR_URL)

libgpg-error: libgpg-error-$(GPGERROR_VERSION).tar.gz
	$(EXTRACT_GZ)

.gpgerror: libgpg-error
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install )
	touch $@

CLEAN_FILE += .gpgerror
CLEAN_PKG += libgpg-error
DISCLEAN_PKG += libgpg-error-$(GPGERROR_VERSION).tar.gz

# ***************************************************************************
# libgcrypt
# ***************************************************************************

libgcrypt-$(GCRYPT_VERSION).tar.gz:
	$(WGET) $(GCRYPT_URL)

libgcrypt: libgcrypt-$(GCRYPT_VERSION).tar.gz
	$(EXTRACT_GZ)

.gcrypt: libgcrypt
	(cd $<; ./configure --prefix=$(PREFIX) CFLAGS="$(CFLAGS) -D_BSD_SOCKLEN_T_=int" && make && make install )
	touch $@

CLEAN_FILE += .gcrypt
CLEAN_PKG += libgcrypt
DISCLEAN_PKG += libgcrypt-$(GCRYPT_VERSION).tar.gz

# ***************************************************************************
# gnutls
# ***************************************************************************

gnutls-$(GNUTLS_VERSION).tar.bz2:
	$(WGET) $(GNUTLS_URL)

gnutls: gnutls-$(GNUTLS_VERSION).tar.bz2
	$(EXTRACT_BZ2)

.gnutls: gnutls
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install )
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .gnutls
CLEAN_PKG += gnutls
DISCLEAN_PKG += gnutls-$(GNUTLS_VERSION).tar.bz2

# ***************************************************************************
# libopendaap
# ***************************************************************************

libopendaap-$(DAAP_VERSION).tar.bz2:
	$(WGET) $(DAAP_URL)

libopendaap: libopendaap-$(DAAP_VERSION).tar.bz2
	$(EXTRACT_BZ2)
	patch -p 0 < Patches/daap.patch

.daap: libopendaap
	(cd $<; ./configure --prefix=$(PREFIX) CFLAGS="$(CFLAGS) -D_BSD_SOCKLEN_T_=int" && make install )
	touch $@

CLEAN_FILE += .daap
CLEAN_PKG += libopendaap
DISCLEAN_PKG += libopendaap-$(DAAP_VERSION).tar.bz2

# ***************************************************************************
# glib
# ***************************************************************************

glib-$(GLIB_VERSION).tar.gz:
	$(WGET) $(GLIB_URL)

glib: glib-$(GLIB_VERSION).tar.gz
	$(EXTRACT_GZ)

.glib: glib
	(cd $<; ./configure --prefix=$(PREFIX) --host=powerpc-apple-macos && make && make install )
	touch $@

CLEAN_FILE += .glibl
CLEAN_PKG += glib
DISCLEAN_PKG += glib-$(GLIB_VERSION).tar.gz

# ***************************************************************************
# libIDL
# ***************************************************************************

libIDL-$(LIBIDL_VERSION).tar.gz:
	$(WGET) $(LIBIDL_URL)

libIDL: libIDL-$(LIBIDL_VERSION).tar.gz
	$(EXTRACT_GZ)

.libidl: libIDL
	(cd $<; ./configure --prefix=$(PREFIX) --host=powerpc-apple-macos && make && make install )
	touch $@

CLEAN_FILE += .libidl
CLEAN_PKG += libIDL
DISCLEAN_PKG += libIDL-$(LIBIDL_VERSION).tar.gz

# ***************************************************************************
# mozilla
# ***************************************************************************

mozilla-source-$(MOZILLA_VERSION).tar.bz2:
	$(WGET) $(MOZILLA_URL)

mozilla: mozilla-source-$(MOZILLA_VERSION).tar.bz2
	$(EXTRACT_BZ2)

.mozilla: mozilla
	(cd $<; ./configure --prefix=$(PREFIX) --enable-default-toolkit=mac --enable-standalone-modules=xpcom,plugin && make && make install )
	touch $@

CLEAN_FILE += .mozilla
CLEAN_PKG += mozilla
DISCLEAN_PKG += mozilla-source-$(MOZILLA_VERSION).tar.bz2

# ***************************************************************************
# Copy aclocal files
# This is necessary for --missing aclocal to succeed after a
# configure.ac/Makefile.am change in the vlc root dir
# **************************************************************************

.aclocal:
	cp -R $(PREFIX)/share/aclocal/* $(PREFIX)/../../m4/
	touch $@


# ***************************************************************************
# Some cleansing
# ***************************************************************************

clean-dots: FORCE
	rm -f $(CLEAN_FILE)

clean: clean-dots
	rm -rf $(CLEAN_PKG)

clean-src: clean
	rm -rf $(DISTCLEAN_PKG)

distclean: clean-src
