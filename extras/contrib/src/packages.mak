# ***************************************************************************
# src/packages.mak : Archive locations
# ***************************************************************************
# Copyright © 2003 - 2008 the VideoLAN team
# $Id$
#
# Authors: Christophe Massiot <massiot@via.ecp.fr>
#          Derk-Jan Hartman <hartman at videolan dot org>
#          Felix Kühne <fkuehne@users.sourceforge.net>
#          Rafaël Carré <funman@videolanorg>
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

PENGUIN=http://ftp.penguin.cz/pub/users/utx/amr
GNU=ftp://ftp.esat.net/pub/gnu
SF=http://switch.dl.sourceforge.net/sourceforge
VIDEOLAN=http://download.videolan.org/pub/videolan
PERL_VERSION=5.8.8
PERL_URL=http://ftp.funet.fi/pub/CPAN/src/perl-$(PERL_VERSION).tar.gz
LIBAMR_NB_VERSION=7.0.0.0
LIBAMR_NB=$(PENGUIN)/amrnb-$(LIBAMR_NB_VERSION).tar.bz2
LIBAMR_WB_VERSION=7.0.0.2
LIBAMR_WB=$(PENGUIN)/amrwb-$(LIBAMR_WB_VERSION).tar.bz2
# Autoconf > 2.57 doesn't work ok on BeOS. Don't ask why.
# we have to use a newer though, because bootstrap won't work otherwise
AUTOCONF_VERSION=2.61
AUTOCONF_URL=$(GNU)/autoconf/autoconf-$(AUTOCONF_VERSION).tar.bz2
CMAKE_VERSION=2.4.8
CMAKE_URL=http://www.cmake.org/files/v2.4/cmake-$(CMAKE_VERSION).tar.gz
LIBTOOL_VERSION=1.5.24
LIBTOOL_URL=$(GNU)/libtool/libtool-$(LIBTOOL_VERSION).tar.gz
AUTOMAKE_VERSION=1.9.6
AUTOMAKE_URL=$(GNU)/automake/automake-$(AUTOMAKE_VERSION).tar.gz
PKGCFG_VERSION=0.20
PKGCFG_URL=$(VIDEOLAN)/testing/contrib/pkg-config-$(PKGCFG_VERSION).tar.gz
#PKGCFG_URL=http://pkgconfig.freedesktop.org/releases/pkg-config-$(PKGCFG_VERSION).tar.gz
LIBICONV_VERSION=1.9.2
LIBICONV_URL=$(GNU)/libiconv/libiconv-$(LIBICONV_VERSION).tar.gz
GETTEXT_VERSION=0.17
GETTEXT_URL=$(GNU)/gettext/gettext-$(GETTEXT_VERSION).tar.gz
FONTCONFIG_VERSION=2.4.2
FONTCONFIG_URL=http://fontconfig.org/release/fontconfig-$(FONTCONFIG_VERSION).tar.gz
FREETYPE2_VERSION=2.3.5
FREETYPE2_URL=$(SF)/freetype/freetype-$(FREETYPE2_VERSION).tar.gz
FRIBIDI_VERSION=0.19.1
FRIBIDI_URL=http://fribidi.org/download/fribidi-$(FRIBIDI_VERSION).tar.gz
#FRIBIDI_URL=ftp://ftp.videolan.org/pub/testing/contrib/fribidi-$(FRIBIDI_VERSION).tar.bz2
A52DEC_VERSION=0.7.4
A52DEC_URL=$(VIDEOLAN)/testing/contrib/a52dec-$(A52DEC_VERSION).tar.gz
MPEG2DEC_VERSION=0.4.1
MPEG2DEC_DATE=20050802
MPEG2DEC_CVSROOT=:pserver:anonymous@cvs.libmpeg2.sourceforge.net:/cvsroot/libmpeg2
MPEG2DEC_SNAPSHOT=$(VIDEOLAN)/testing/contrib/mpeg2dec-$(MPEG2DEC_DATE).tar.gz
MPEG2DEC_URL=$(VIDEOLAN)/testing/contrib/mpeg2dec-$(MPEG2DEC_VERSION).tar.gz
LIBID3TAG_VERSION=0.15.1b
LIBID3TAG_URL=$(VIDEOLAN)/testing/contrib/libid3tag-$(LIBID3TAG_VERSION).tar.gz
LIBMAD_VERSION=0.15.1b
LIBMAD_URL=$(VIDEOLAN)/testing/contrib/libmad-$(LIBMAD_VERSION).tar.gz
OGG_VERSION=1.1.3
#OGG_URL=http://downloads.xiph.org/releases/ogg/libogg-$(OGG_VERSION).tar.gz
OGG_URL=$(VIDEOLAN)/testing/contrib/libogg-$(OGG_VERSION).tar.gz
OGG_CVSROOT=:pserver:anoncvs@xiph.org:/usr/local/cvsroot
VORBIS_VERSION=1.2.0
VORBIS_URL=http://downloads.xiph.org/releases/vorbis/libvorbis-$(VORBIS_VERSION).tar.gz
#VORBIS_URL=$(VIDEOLAN)/testing/contrib/libvorbis-$(VORBIS_VERSION).tar.gz
THEORA_VERSION=1.0beta2
THEORA_URL=http://downloads.xiph.org/releases/theora/libtheora-$(THEORA_VERSION).tar.bz2
#THEORA_URL=$(VIDEOLAN)/testing/contrib/libtheora-$(THEORA_VERSION).tar.bz2
FLAC_VERSION=1.2.1
FLAC_URL=$(SF)/flac/flac-$(FLAC_VERSION).tar.gz
SPEEX_VERSION=1.2beta3
SPEEX_URL=http://downloads.us.xiph.org/releases/speex/speex-$(SPEEX_VERSION).tar.gz
SHOUT_VERSION=2.2.2
SHOUT_URL=http://downloads.us.xiph.org/releases/libshout/libshout-$(SHOUT_VERSION).tar.gz
FAAD2_VERSION=2.6.1
FAAD2_URL=$(SF)/faac/faad2-$(FAAD2_VERSION).tar.gz
#FAAD2_URL=$(VIDEOLAN)/testing/contrib/faad2-$(FAAD2_VERSION).tar.bz2
FAAD2_CVSROOT=:pserver:anonymous@cvs.audiocoding.com:/cvsroot/faac
FAAC_VERSION=1.26
FAAC_URL=$(SF)/faac/faac-$(FAAC_VERSION).tar.gz
LAME_VERSION=3.97
LAME_URL=$(SF)/lame/lame-$(LAME_VERSION).tar.gz
LIBEBML_VERSION=0.7.7
#LIBEBML_URL=http://dl.matroska.org/downloads/libebml/libebml-$(LIBEBML_VERSION).tar.bz2
LIBEBML_URL=$(VIDEOLAN)/testing/contrib/libebml-$(LIBEBML_VERSION).tar.bz2
LIBMATROSKA_VERSION=0.8.1
#LIBMATROSKA_URL=http://dl.matroska.org/downloads/libmatroska/libmatroska-$(LIBMATROSKA_VERSION).tar.bz2
LIBMATROSKA_URL=$(VIDEOLAN)/testing/contrib/libmatroska-$(LIBMATROSKA_VERSION).tar.bz2
FFMPEG_VERSION=0.4.8
FFMPEG_URL=$(SF)/ffmpeg/ffmpeg-$(FFMPEG_VERSION).tar.gz
FFMPEG_SVN=svn://svn.mplayerhq.hu/ffmpeg/trunk
LIBDVDCSS_VERSION=1.2.9
LIBDVDCSS_URL=$(VIDEOLAN)/libdvdcss/$(LIBDVDCSS_VERSION)/libdvdcss-$(LIBDVDCSS_VERSION).tar.gz
LIBDVDNAV_VERSION=4.1.1
LIBDVDNAV_URL=http://www1.mplayerhq.hu/MPlayer/releases/dvdnav/libdvdnav-$(LIBDVDNAV_VERSION).tar.gz
LIBDVDNAV_SVN=svn://svn.mplayerhq.hu/dvdnav/trunk/libdvdnav
LIBDVBPSI_VERSION=0.1.6
LIBDVBPSI_URL=$(VIDEOLAN)/libdvbpsi/$(LIBDVBPSI_VERSION)/libdvbpsi5-$(LIBDVBPSI_VERSION).tar.gz
LIVEDOTCOM_VERSION=latest
LIVEDOTCOM_URL=http://live555.com/liveMedia/public/live555-$(LIVEDOTCOM_VERSION).tar.gz
#GOOM_URL=$(VIDEOLAN)/testing/contrib/goom-macosx-altivec-bin.tar.gz
GOOM2k4_VERSION=2k4-0
GOOM2k4_URL=$(SF)/goom/goom-$(GOOM2k4_VERSION)-src.tar.gz
LIBCACA_VERSION=0.99.beta13b
LIBCACA_URL=http://libcaca.zoy.org/files/libcaca-$(LIBCACA_VERSION).tar.gz
LIBDCA_VERSION=0.0.5
LIBDCA_URL=ftp://ftp.videolan.org/pub/videolan/libdca/$(LIBDCA_VERSION)/libdca-$(LIBDCA_VERSION).tar.bz2
LIBDC1394_VERSION=1.2.1
LIBDC1394_URL=$(SF)/libdc1394/libdc1394-$(LIBDC1394_VERSION).tar.gz
LIBDC1394_SVN=https://svn.sourceforge.net/svnroot
LIBRAW1394_VERSION=1.2.0
LIBRAW1394_URL=$(SF)/libraw1394/libraw1394-$(LIBRAW1394_VERSION).tar.gz
LIBRAW1394_SVN=https://svn.sourceforge.net/svnroot
LIBDTS_VERSION=0.0.2
LIBDTS_URL=http://debian.unnet.nl/pub/videolan/libdts/$(LIBDTS_VERSION)/libdts-$(LIBDTS_VERSION).tar.gz
LIBDCA_SVN=svn://svn.videolan.org/libdca/trunk
MODPLUG_VERSION=0.8
MODPLUG_URL=$(SF)/modplug-xmms/libmodplug-$(MODPLUG_VERSION).tar.gz
CDDB_VERSION=1.3.0
CDDB_URL=$(SF)/libcddb/libcddb-$(CDDB_VERSION).tar.bz2
VCDIMAGER_VERSION=0.7.23
VCDIMAGER_URL=$(GNU)/vcdimager/vcdimager-$(VCDIMAGER_VERSION).tar.gz
CDIO_VERSION=0.79
CDIO_URL=$(GNU)/libcdio/libcdio-$(CDIO_VERSION).tar.gz
PNG_VERSION=1.2.25
PNG_URL=$(SF)/libpng/libpng-$(PNG_VERSION).tar.bz2
GPGERROR_VERSION=1.6
GPGERROR_URL=$(VIDEOLAN)/testing/contrib/libgpg-error-$(GPGERROR_VERSION).tar.bz2
#GPGERROR_URL=ftp://ftp.gnupg.org/gcrypt/libgpg-error/libgpg-error-$(GPGERROR_VERSION).tar.bz2
GCRYPT_VERSION=1.2.3
GCRYPT_URL=$(VIDEOLAN)/testing/contrib/libgcrypt-$(GCRYPT_VERSION).tar.bz2
#GCRYPT_URL=ftp://ftp.gnupg.org/gcrypt/libgcrypt/libgcrypt-$(GCRYPT_VERSION).tar.bz2
GNUTLS_VERSION=1.6.3
GNUTLS_URL=$(VIDEOLAN)/testing/contrib/gnutls-$(GNUTLS_VERSION).tar.bz2
#GNUTLS_URL=ftp://ftp.gnupg.org/gcrypt/gnutls/gnutls-$(GNUTLS_VERSION).tar.bz2
DAAP_VERSION=0.4.0
DAAP_URL=http://craz.net/programs/itunes/files/libopendaap-$(DAAP_VERSION).tar.bz2
GLIB_VERSION=1.2.10
GLIB_URL=ftp://ftp.gtk.org/pub/gtk/v1.2/glib-1.2.10.tar.gz
LIBIDL_VERSION=0.6.8
LIBIDL_URL=http://andrewtv.org/libIDL/libIDL-$(LIBIDL_VERSION).tar.gz
GECKO_SDK_MAC_URL=$(VIDEOLAN)/testing/contrib/gecko-sdk-ppc-macosx10.2-1.7.5.tar.gz
GECKO_SDK_WIN32_URL=ftp://ftp.mozilla.org/pub/mozilla.org/mozilla/releases/mozilla1.8b1/gecko-sdk-i586-pc-msvc-1.8b1.zip
LIBIDL_WIN32_BIN_URL=ftp://ftp.mozilla.org/pub/mozilla.org/mozilla/libraries/win32/historic/vc6/libIDL-0.6.3-win32-bin.zip
GLIB_WIN32_BIN_URL=ftp://ftp.mozilla.org/pub/mozilla.org/mozilla/libraries/win32/historic/vc6/glib-19990228.zip
MOZILLA_VERSION=1.7.5
MOZILLA_URL=http://ftp.mozilla.org/pub/mozilla.org/mozilla/releases/mozilla$(MOZILLA_VERSION)/source/mozilla-source-$(MOZILLA_VERSION).tar.bz2
TWOLAME_VERSION=0.3.12
TWOLAME_URL=$(SF)/twolame/twolame-$(TWOLAME_VERSION).tar.gz
X264_VERSION=20050609
X264_URL=$(VIDEOLAN)/testing/contrib/x264-$(X264_VERSION).tar.gz
JPEG_VERSION=6b
JPEG_URL=$(VIDEOLAN)/contrib/jpeg-$(JPEG_VERSION).tar.gz
TIFF_VERSION=3.8.2
#TIFF_URL=ftp://ftp.remotesensing.org/libtiff/tiff-$(TIFF_VERSION).tar.gz
TIFF_URL=http://dl.maptools.org/dl/libtiff/tiff-$(TIFF_VERSION).tar.gz
SDL_VERSION=1.2.13
SDL_URL=http://www.libsdl.org/release/SDL-$(SDL_VERSION).tar.gz
SDL_IMAGE_VERSION=1.2.6
SDL_IMAGE_URL=http://www.libsdl.org/projects/SDL_image/release/SDL_image-$(SDL_IMAGE_VERSION).tar.gz
MUSE_VERSION=1.2.6
#MUSE_URL=http://files.musepack.net/source/libmpcdec-$(MUSE_VERSION).tar.bz2
MUSE_URL=http://files2.musepack.net/source/libmpcdec-$(MUSE_VERSION).tar.bz2
WXWIDGETS_VERSION=2.6.4
WXWIDGETS_URL=$(SF)/wxwindows/wxWidgets-$(WXWIDGETS_VERSION).tar.gz
QT4_VERSION=4.3.4
QT4_URL=$(VIDEOLAN)/testing/contrib/qt4-$(QT4_VERSION)-win32-bin.tar.bz2
QT4_MAC_VERSION=4.3.3
QT4_MAC_URL=ftp://ftp.trolltech.com/qt/source/qt-mac-opensource-src-$(QT4_MAC_VERSION).tar.gz
QT4T_VERSION=4.3.2
QT4T_URL=ftp://ftp.trolltech.com/pub/qt/source/qt-win-opensource-$(QT4T_VERSION)-mingw.exe
ZLIB_VERSION=1.2.3
ZLIB_URL=$(SF)/libpng/zlib-$(ZLIB_VERSION).tar.gz
XML_VERSION=2.6.31
XML_URL=http://xmlsoft.org/sources/libxml2-$(XML_VERSION).tar.gz
DIRAC_VERSION=0.9.1
DIRAC_URL=$(SF)/dirac/dirac-$(DIRAC_VERSION).tar.gz
DX_HEADERS_URL=$(VIDEOLAN)/testing/contrib/win32-dx7headers.tgz
DSHOW_HEADERS_URL=$(VIDEOLAN)/contrib/dshow-headers.tgz
PORTAUDIO_VERSION=19
PORTAUDIO_URL=http://www.portaudio.com/archives/pa_snapshot_v$(PORTAUDIO_VERSION).tar.gz
CLINKCC_VERSION=171
CLINKCC_URL=$(SF)/clinkcc/clinkcc$(CLINKCC_VERSION).tar.gz
UPNP_VERSION=1.2.1
UPNP_URL=$(SF)/upnp/libupnp-$(UPNP_VERSION).tar.gz
EXPAT_VERSION=2.0.0
EXPAT_URL=$(SF)/expat/expat-$(EXPAT_VERSION).tar.gz
NASM_CVSROOT=:pserver:anonymous@cvs.sourceforge.net:/cvsroot/nasm
NASM_VERSION=2.02
NASM_URL=$(SF)/nasm/nasm-$(NASM_VERSION).tar.bz2
PTHREADS_VERSION=2-8-0
PTHREADS_URL=ftp://sources.redhat.com/pub/pthreads-win32/pthreads-w32-$(PTHREADS_VERSION)-release.tar.gz
UNICOWS_VERSION=1.1.1
UNICOWS_URL=$(SF)/libunicows/libunicows-$(UNICOWS_VERSION)-src.tar.gz
ZVBI_VERSION=0.2.25
ZVBI_URL=$(SF)/zapping/zvbi-$(ZVBI_VERSION).tar.bz2
TAGLIB_VERSION=1.4
TAGLIB_URL=http://developer.kde.org/~wheeler/files/src/taglib-$(TAGLIB_VERSION).tar.gz
LUA_VERSION=5.1
LUA_URL=http://www.lua.org/ftp/lua-$(LUA_VERSION).tar.gz
NCURSES_VERSION=5.6
NCURSES_URL=$(GNU)/ncurses/ncurses-$(NCURSES_VERSION).tar.gz
ASA_URL=$(VIDEOLAN)/testing/contrib/asa.git.tar.gz
PCRE_VERSION=7.6
PCRE_URL=ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre-$(PCRE_VERSION).tar.bz2
