# ***************************************************************************
# src/packages.mak : Archive locations
# ***************************************************************************
# Copyright (C) 2003 - 2005 the VideoLAN team
# $Id$
#
# Authors: Christophe Massiot <massiot@via.ecp.fr>
#          Derk-Jan Hartman <hartman at videolan dot org>
#          Felix Kühne <fkuehne@users.sourceforge.net>
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

GNU=http://ftp.belnet.be/mirror/ftp.gnu.org/gnu
SF=http://switch.dl.sourceforge.net/sourceforge
VIDEOLAN=http://download.videolan.org/pub/videolan
PERL_VERSION=5.8.5
PERL_URL=http://ftp.funet.fi/pub/CPAN/src/perl-$(PERL_VERSION).tar.gz
# Autoconf > 2.57 doesn't work ok on BeOS. Don't ask why.
# we have to use a newer though, because bootstrap won't work otherwise
AUTOCONF_VERSION=2.59
AUTOCONF_URL=$(GNU)/autoconf/autoconf-$(AUTOCONF_VERSION).tar.gz
LIBTOOL_VERSION=1.5.6
LIBTOOL_URL=$(GNU)/libtool/libtool-$(LIBTOOL_VERSION).tar.gz
AUTOMAKE_VERSION=1.7.8
AUTOMAKE_URL=$(GNU)/automake/automake-$(AUTOMAKE_VERSION).tar.gz
PKGCFG_VERSION=0.15.0
PKGCFG_URL=http://pkgconfig.freedesktop.org/releases/pkgconfig-$(PKGCFG_VERSION).tar.gz
LIBICONV_VERSION=1.9.1
LIBICONV_URL=$(GNU)/libiconv/libiconv-$(LIBICONV_VERSION).tar.gz
GETTEXT_VERSION=0.14.1
GETTEXT_URL=$(GNU)/gettext/gettext-$(GETTEXT_VERSION).tar.gz
FREETYPE2_VERSION=2.1.9
FREETYPE2_URL=$(SF)/freetype/freetype-$(FREETYPE2_VERSION).tar.gz
FRIBIDI_VERSION=0.10.4
FRIBIDI_URL=$(SF)/fribidi/fribidi-$(FRIBIDI_VERSION).tar.gz
A52DEC_VERSION=0.7.4
A52DEC_URL=$(VIDEOLAN)/testing/contrib/a52dec-$(A52DEC_VERSION).tar.gz
MPEG2DEC_VERSION=0.4.1-cvs
MPEG2DEC_DATE=20050802
MPEG2DEC_CVSROOT=:pserver:anonymous@cvs.libmpeg2.sourceforge.net:/cvsroot/libmpeg2
MPEG2DEC_SNAPSHOT=$(VIDEOLAN)/testing/contrib/mpeg2dec-$(MPEG2DEC_DATE).tar.gz
LIBID3TAG_VERSION=0.15.1b
LIBID3TAG_URL=ftp://ftp.mars.org/pub/mpeg/libid3tag-$(LIBID3TAG_VERSION).tar.gz
LIBMAD_VERSION=0.15.1b
LIBMAD_URL=ftp://ftp.mars.org/pub/mpeg/libmad-$(LIBMAD_VERSION).tar.gz
OGG_VERSION=1.1.2
OGG_URL=http://downloads.xiph.org/releases/ogg/libogg-$(OGG_VERSION).tar.gz
OGG_CVSROOT=:pserver:anoncvs@xiph.org:/usr/local/cvsroot
VORBIS_VERSION=1.1.1
VORBIS_URL=http://downloads.xiph.org/releases/vorbis/libvorbis-$(VORBIS_VERSION).tar.gz
THEORA_VERSION=1.0alpha5
THEORA_URL=$(VIDEOLAN)/testing/contrib/libtheora-$(THEORA_VERSION).tar.bz2
FLAC_VERSION=1.1.2
FLAC_URL=$(SF)/flac/flac-$(FLAC_VERSION).tar.gz
SPEEX_VERSION=1.1.10
SPEEX_URL=http://downloads.us.xiph.org/releases/speex/speex-$(SPEEX_VERSION).tar.gz
SHOUT_VERSION=2.1
SHOUT_URL=http://downloads.us.xiph.org/releases/libshout/libshout-$(SHOUT_VERSION).tar.gz
FAAD2_VERSION=20040923
FAAD2_URL=$(VIDEOLAN)/testing/contrib/faad2-$(FAAD2_VERSION).tar.bz2
FAAD2_CVSROOT=:pserver:anonymous@cvs.audiocoding.com:/cvsroot/faac
FAAC_VERSION=1.24
FAAC_URL=$(VIDEOLAN)/testing/contrib/faac-$(FAAC_VERSION).tar.bz2
LAME_VERSION=3.96.1
LAME_URL=$(SF)/lame/lame-$(LAME_VERSION).tar.gz
LIBEBML_VERSION=0.7.6
LIBEBML_URL=http://dl.matroska.org/downloads/libebml/libebml-$(LIBEBML_VERSION).tar.bz2
LIBMATROSKA_VERSION=0.8.0
LIBMATROSKA_URL=http://dl.matroska.org/downloads/libmatroska/libmatroska-$(LIBMATROSKA_VERSION).tar.bz2
FFMPEG_VERSION=0.4.8
FFMPEG_URL=$(SF)/ffmpeg/ffmpeg-$(FFMPEG_VERSION).tar.gz
FFMPEG_CVSROOT=:pserver:anonymous@mplayerhq.hu:/cvsroot/ffmpeg
LIBDVDCSS_VERSION=1.2.8
LIBDVDCSS_URL=$(VIDEOLAN)/libdvdcss/$(LIBDVDCSS_VERSION)/libdvdcss-$(LIBDVDCSS_VERSION).tar.gz
LIBDVDREAD_VERSION=20041028
LIBDVDREAD_URL=$(VIDEOLAN)/contrib/libdvdread-$(LIBDVDREAD_VERSION).tar.bz2
LIBDVDNAV_VERSION=20050211
LIBDVDNAV_URL=$(VIDEOLAN)/testing/contrib/libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2
LIBDVBPSI_VERSION=0.1.5
LIBDVBPSI_URL=$(VIDEOLAN)/contrib/libdvbpsi3-$(LIBDVBPSI_VERSION).tar.gz
LIVEDOTCOM_VERSION=latest
LIVEDOTCOM_URL=http://www.live555.com/liveMedia/public/live555-$(LIVEDOTCOM_VERSION).tar.gz
#GOOM_URL=$(VIDEOLAN)/testing/contrib/goom-macosx-altivec-bin.tar.gz
GOOM2k4_VERSION=2k4-0
GOOM2k4_URL=$(SF)/goom/goom-$(GOOM2k4_VERSION)-src.tar.gz
LIBCACA_VERSION=0.9
LIBCACA_URL=http://sam.zoy.org/libcaca/libcaca-$(LIBCACA_VERSION).tar.gz
LIBDTS_VERSION=0.0.2
LIBDTS_URL=http://debian.unnet.nl/pub/videolan/libdts/$(LIBDTS_VERSION)/libdts-$(LIBDTS_VERSION).tar.gz
MODPLUG_VERSION=0.7
MODPLUG_URL=$(VIDEOLAN)/testing/contrib/libmodplug-$(MODPLUG_VERSION).tar.gz
MASH_VERSION=5.2
MASH_URL=$(SF)/openmash/mash-src-$(MASH_VERSION).tar.gz
CDDB_VERSION=0.9.6
CDDB_URL=$(SF)/libcddb/libcddb-$(CDDB_VERSION).tar.gz
VCDIMAGER_VERSION=0.7.21
VCDIMAGER_URL=$(GNU)/vcdimager/vcdimager-$(VCDIMAGER_VERSION).tar.gz
CDIO_VERSION=0.72
CDIO_URL=$(GNU)/libcdio/libcdio-$(CDIO_VERSION).tar.gz
PNG_VERSION=1.2.5
PNG_URL=$(VIDEOLAN)/testing/contrib/libpng-$(PNG_VERSION).tar.bz2
GPGERROR_VERSION=1.0
GPGERROR_URL=$(VIDEOLAN)/testing/contrib/libgpg-error-$(GPGERROR_VERSION).tar.gz
GCRYPT_VERSION=1.2.1
GCRYPT_URL=$(VIDEOLAN)/testing/contrib/libgcrypt-$(GCRYPT_VERSION).tar.bz2
GNUTLS_VERSION=1.2.7
GNUTLS_URL=http://ftp.gnupg.org/gcrypt/gnutls/gnutls-$(GNUTLS_VERSION).tar.bz2
DAAP_VERSION=0.4.0
DAAP_URL=http://craz.net/programs/itunes/files/libopendaap-$(DAAP_VERSION).tar.bz2
GLIB_VERSION=1.2.8
GLIB_URL=ftp://ftp.gtk.org/pub/gtk/v1.2/glib-1.2.8.tar.gz
LIBIDL_VERSION=0.6.8
LIBIDL_URL=http://andrewtv.org/libIDL/libIDL-$(LIBIDL_VERSION).tar.gz
GECKO_SDK_MAC_URL=$(VIDEOLAN)/testing/contrib/gecko-sdk-ppc-macosx10.2-1.7.5.tar.gz
GECKO_SDK_WIN32_URL=ftp://ftp.mozilla.org/pub/mozilla.org/mozilla/releases/mozilla1.7.5/gecko-sdk-i586-pc-msvc-1.7.5.zip
LIBIDL_WIN32_BIN_URL=ftp://ftp.mozilla.org/pub/mozilla.org/mozilla/libraries/win32/libIDL-0.6.3-win32-bin.zip
VC71_GLIB_WIN32_BIN_URL=ftp://ftp.mozilla.org/pub/mozilla.org/mozilla/libraries/win32/vc71-glib-1.2.10-bin.zip
MOZILLA_VERSION=1.7.5
MOZILLA_URL=http://ftp.mozilla.org/pub/mozilla.org/mozilla/releases/mozilla$(MOZILLA_VERSION)/source/mozilla-source-$(MOZILLA_VERSION).tar.bz2
TWOLAME_VERSION=0.3.2
TWOLAME_URL=$(SF)/twolame/twolame-$(TWOLAME_VERSION).tar.gz
X264_VERSION=20050609
X264_URL=$(VIDEOLAN)/testing/contrib/x264-$(X264_VERSION).tar.gz
JPEG_VERSION=6b
JPEG_URL=$(VIDEOLAN)/contrib/jpeg-$(JPEG_VERSION).tar.gz
TIFF_VERSION=v3.4
TIFF_URL=ftp://ftp.sgi.com/graphics/tiff/tiff-$(TIFF_VERSION)-tar.gz
SDL_VERSION=1.2.8
SDL_URL=http://www.libsdl.org/release/SDL-$(SDL_VERSION).tar.gz
SDL_IMAGE_VERSION=1.2.4
SDL_IMAGE_URL=http://www.libsdl.org/projects/SDL_image/release/SDL_image-$(SDL_IMAGE_VERSION).tar.gz
MUSE_VERSION=1.2.1
MUSE_URL=http://files.musepack.net/source/libmpcdec-$(MUSE_VERSION).tar.bz2
WXWIDGETS_VERSION=2.6.2
WXWIDGETS_URL=$(SF)/wxwindows/wxWidgets-$(WXWIDGETS_VERSION).tar.gz
ZLIB_VERSION=1.2.3
ZLIB_URL=$(SF)/libpng/zlib-$(ZLIB_VERSION).tar.gz
XML_VERSION=2.6.20
XML_URL=http://xmlsoft.org/sources/libxml2-$(XML_VERSION).tar.gz
DIRAC_VERSION=0.5.3
DIRAC_URL=$(SF)/dirac/dirac-$(DIRAC_VERSION).tar.gz
DX_HEADERS_URL=$(VIDEOLAN)/testing/contrib/win32-dx7headers.tgz
DSHOW_HEADERS_URL=$(VIDEOLAN)/contrib/dshow-headers.tgz
PORTAUDIO_VERSION=19
PORTAUDIO_URL=http://www.portaudio.com/archives/pa_snapshot_v$(PORTAUDIO_VERSION).tar.gz
CLINKCC_VERSION=171
CLINKCC_URL=$(SF)/clinkcc/clinkcc$(CLINKCC_VERSION).tar.gz
EXPAT_VERSION=1.95.8
EXPAT_URL=$(SF)/expat/expat-$(EXPAT_VERSION).tar.gz
NASM_CVSROOT=:pserver:anonymous@cvs.sourceforge.net:/cvsroot/nasm
NASM_VERSION=0.98.40-20060130
NASM_URL=$(VIDEOLAN)/testing/contrib/nasm-0.98.40-20060130.tar.bz2
