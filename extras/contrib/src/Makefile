# ***************************************************************************
# src/Makefile : Dearchive and compile all files necessary
# ***************************************************************************
# Copyright (C) 2003 VideoLAN
# $Id: Makefile,v 1.4 2003/11/17 13:14:25 hartman Exp $
#
# Authors: Christophe Massiot <massiot@via.ecp.fr>
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

export PATH = $(PREFIX)/bin:/bin:/usr/bin
export DYLD_LIBRARY_PATH = $(PREFIX)/lib
export CPPFLAGS = -I$(PREFIX)/include $(EXTRA_CPPFLAGS)
export CFLAGS = -I$(PREFIX)/include $(EXTRA_CPPFLAGS)
export CXXFLAGS = -I$(PREFIX)/include $(EXTRA_CPPFLAGS)
export LDFLAGS = -L$(PREFIX)/lib

# ***************************************************************************
# Standard rules
# ***************************************************************************

all: .autoconf .libtool .automake .iconv .intl .freetype .fribidi \
	.a52 .mpeg2 .id3tag .mad .ogg .vorbis .vorbisenc .theora \
	.FLAC .speex .faad .lame .ebml .matroska .ffmpeg .openslp \
	.dvdcss .dvdread .dvdplay .dvbpsi
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

define INSTALL_NAME_LIB
	install_name_tool \
		-change $(PREFIX)/lib/$(shell cd $(PREFIX)/lib; ls | grep "lib$(INSTALL_LIB).[0-9]*.dylib") \
		@executable_path/lib/lib$(INSTALL_LIB).dylib \
		$(PREFIX)/vlc-lib/lib$(patsubst .%,%,$@).dylib ;
endef

define INSTALL_NAME
	if which install_name_tool >/dev/null; then \
		mkdir -p $(PREFIX)/vlc-lib ; \
		cp $(PREFIX)/lib/lib$(patsubst .%,%,$@).dylib \
			$(PREFIX)/vlc-lib/lib$(patsubst .%,%,$@).dylib ; \
		install_name_tool \
			-id @executable_path/lib/lib$(patsubst .%,%,$@).dylib \
			$(PREFIX)/vlc-lib/lib$(patsubst .%,%,$@).dylib ; \
		$(foreach INSTALL_LIB,$(patsubst .%,%,$(filter .%,$^)), \
			$(INSTALL_NAME_LIB)) \
	fi
endef

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

# This breaks under Darwin
#libtool_$(LIBTOOL_VERSION).orig.tar.gz:
#	$(WGET) $(LIBTOOL_URL)

#libtool: libtool_$(LIBTOOL_VERSION).orig.tar.gz
#	$(EXTRACT_GZ)
#	rm $@
#	mv libtool-$(LIBTOOL_VERSION) $@

#.libtool: libtool
#	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
#	touch $@

# Darwin-specific
.libtool:
	if test -x /usr/bin/glibtoolize; then \
		ln -sf /usr/bin/glibtoolize $(PREFIX)/bin/libtoolize ; \
	fi
	touch $@

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
# libiconv
# ***************************************************************************

libiconv-$(LIBICONV_VERSION).tar.gz:
	$(WGET) $(LIBICONV_URL)

libiconv: libiconv-$(LIBICONV_VERSION).tar.gz
	$(EXTRACT_GZ)

.iconv: libiconv
	(cd libiconv; ./configure --prefix=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .iconv
CLEAN_PKG += libiconv
DISTCLEAN_PKG += libiconv-$(LIBICONV_VERSION).tar.gz

# ***************************************************************************
# gettext
# ***************************************************************************

gettext-$(GETTEXT_VERSION).tar.gz:
	$(WGET) $(GETTEXT_URL)

gettext: gettext-$(GETTEXT_VERSION).tar.gz
	$(EXTRACT_GZ)

.intl: gettext .iconv
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .intl
CLEAN_PKG += gettext
DISTCLEAN_PKG += gettext-$(GETTEXT_VERSION).tar.gz

# ***************************************************************************
# freetype2
# ***************************************************************************

freetype-$(FREETYPE2_VERSION).tar.gz:
	$(WGET) $(FREETYPE2_URL)

freetype2: freetype-$(FREETYPE2_VERSION).tar.gz
	$(EXTRACT_GZ)

.freetype: freetype2
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
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
	patch -p 0 < Patches/fribidi.patch

.fribidi: fribidi
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
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
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
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

# It is easier to use bootstrapped versions...
mpeg2dec-0.3.2-cvs.tar.gz:
	$(WGET) $(MPEG2DEC_SNAPSHOT)
	mv mpeg2dec-snapshot.tar.gz $@

mpeg2dec: mpeg2dec-0.3.2-cvs.tar.gz
	$(EXTRACT_GZ)

.mpeg2: mpeg2dec
	(cd $<; ./configure --prefix=$(PREFIX) --without-x --disable-dsl && make && make install)
	touch $@

CLEAN_FILE += .mpeg2
CLEAN_PKG += mpeg2dec
DISTCLEAN_PKG += mpeg2dec-0.3.2-cvs.tar.gz

# ***************************************************************************
# libid3tag
# ***************************************************************************

libid3tag-$(LIBID3TAG_VERSION).tar.gz:
	$(WGET) $(LIBID3TAG_URL)

libid3tag: libid3tag-$(LIBID3TAG_VERSION).tar.gz
	$(EXTRACT_GZ)

.id3tag: libid3tag
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
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
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
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
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
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
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

.vorbisenc: .vorbis .ogg
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .vorbis .vorbisenc
CLEAN_PKG += libvorbis
DISTCLEAN_PKG += libvorbis-$(VORBIS_VERSION).tar.gz

# ***************************************************************************
# theora
# ***************************************************************************

libtheora-$(THEORA_VERSION).tar.gz:
	$(WGET) $(THEORA_URL)

libtheora: libtheora-$(THEORA_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p 0 < Patches/theora.patch

.theora: libtheora
	(cd $<; CC="gcc $(CFLAGS)" ./configure --prefix=$(PREFIX) && make && make install)
	touch $@

CLEAN_FILE += .theora
CLEAN_PKG += libtheora
DISTCLEAN_PKG += libtheora-$(THEORA_VERSION).tar.gz

# ***************************************************************************
# flac
# ***************************************************************************

flac-$(FLAC_VERSION).tar.gz:
	$(WGET) $(FLAC_URL)

flac: flac-$(FLAC_VERSION).tar.gz
	$(EXTRACT_GZ)

.FLAC: flac
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
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

.speex: speex
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
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

faad2-$(FAAD2_VERSION).tar.gz:
	$(WGET) $(FAAD2_URL)

faad2: faad2-$(FAAD2_VERSION).tar.gz
	$(EXTRACT_GZ)
	(cd $@; patch -p 0 < ../Patches/faad2.patch)

.faad: faad2
	(cd $<; ./configure --prefix=$(PREFIX) && make -C libfaad && make -C libfaad install)
	$(INSTALL_NAME)
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
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
	touch $@

CLEAN_FILE += .lame
CLEAN_PKG += lame
DISTCLEAN_PKG += lame-$(LAME_VERSION).tar.gz

# ***************************************************************************
# libebml
# ***************************************************************************

libebml-$(LIBEBML_VERSION).tar.gz:
	$(WGET) $(LIBEBML_URL)

libebml: libebml-$(LIBEBML_VERSION).tar.gz
	$(EXTRACT_GZ)
	-rm -rf $@/CVS

.ebml: libebml
	(cd $<; make -C make/linux prefix=$(PREFIX) && make -C make/linux install prefix=$(PREFIX))
	ranlib $(PREFIX)/lib/libebml.a
	touch $@

CLEAN_FILE += .ebml
CLEAN_PKG += libebml
DISTCLEAN_PKG += libebml-$(LIBEBML_VERSION).tar.bz2

# ***************************************************************************
# libmatroska
# ***************************************************************************

libmatroska-$(LIBMATROSKA_VERSION).tar.gz:
	$(WGET) $(LIBMATROSKA_URL)

libmatroska: libmatroska-$(LIBMATROSKA_VERSION).tar.gz
	$(EXTRACT_GZ)
	-rm -rf $@/CVS

.matroska: libmatroska
	(cd $<; make -C make/linux prefix=$(PREFIX) && make -C make/linux install prefix=$(PREFIX))
	ranlib $(PREFIX)/lib/libmatroska.a
	touch $@

CLEAN_FILE += .matroska
CLEAN_PKG += libmatroska
DISTCLEAN_PKG += libmatroska-$(LIBMATROSKA_VERSION).tar.bz2

# ***************************************************************************
# ffmpeg
# ***************************************************************************

#ffmpeg:
#	cvs -d $(FFMPEG_CVSROOT) login
#	cvs -d $(FFMPEG_CVSROOT) co ffmpeg
#	patch -p 0 < Patches/ffmpeg.patch.cvs
#	(cd $@; ./bootstrap)

ffmpeg-$(FFMPEG_VERSION).tar.gz:
	$(WGET) $(FFMPEG_URL)

ffmpeg: ffmpeg-$(FFMPEG_VERSION).tar.gz
	$(EXTRACT_GZ)
	patch -p 0 < Patches/ffmpeg.patch

.ffmpeg: ffmpeg
	(cd $<; ./configure --prefix=$(PREFIX) --extra-cflags="$(CFLAGS)" --extra-ldflags="$(LDFLAGS)" --enable-mp3lame --enable-pp --disable-vhook --disable-vorbis --disable-faad --disable-a52 --enable-mp3lame && make && make -C libavcodec/libpostproc install && make -C libavcodec installlib && make -C libavformat installlib)
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
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
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

.dvdread: libdvdread .dvdcss
	(cd $<; ./configure --prefix=$(PREFIX) --with-libdvdcss=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .dvdread
CLEAN_PKG += libdvdread
DISTCLEAN_PKG += libdvdread-$(LIBDVDREAD_VERSION).tar.gz

# ***************************************************************************
# libdvdplay
# ***************************************************************************

libdvdplay-$(LIBDVDPLAY_VERSION).tar.gz:
	$(WGET) $(LIBDVDPLAY_URL)

libdvdplay: libdvdplay-$(LIBDVDPLAY_VERSION).tar.gz
	$(EXTRACT_GZ)

.dvdplay: libdvdplay .dvdread
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .dvdplay
CLEAN_PKG += libdvdplay
DISTCLEAN_PKG += libdvdplay-$(LIBDVDPLAY_VERSION).tar.gz

# ***************************************************************************
# libdvbpsi
# ***************************************************************************

libdvbpsi-$(LIBDVBPSI_VERSION).tar.gz:
	$(WGET) $(LIBDVBPSI_URL)

libdvbpsi: libdvbpsi-$(LIBDVBPSI_VERSION).tar.gz
	$(EXTRACT_GZ)

.dvbpsi: libdvbpsi
	(cd $<; ./configure --prefix=$(PREFIX) && make && make install)
	$(INSTALL_NAME)
	touch $@

CLEAN_FILE += .dvbpsi
CLEAN_PKG += libdvbpsi
DISTCLEAN_PKG += libdvbpsi-$(LIBDVBPSI_VERSION).tar.gz

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
