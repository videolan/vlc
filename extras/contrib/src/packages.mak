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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
# ***************************************************************************

GNU=http://ftp.belnet.be/mirror/ftp.gnu.org/gnu
SF=http://switch.dl.sourceforge.net/sourceforge
VIDEOLAN=http://download.videolan.org/pub/videolan
PERL_VERSION=5.8.5
PERL_URL=http://ftp.funet.fi/pub/CPAN/src/perl-$(PERL_VERSION).tar.gz
# Autoconf > 2.57 doesn't work ok on BeOS. Don't ask why.
AUTOCONF_VERSION=2.57
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
A52DEC_URL=http://liba52.sourceforge.net/files/a52dec-$(A52DEC_VERSION).tar.gz
MPEG2DEC_VERSION=0.4.1-cvs
MPEG2DEC_CVSROOT=:pserver:anonymous@cvs.libmpeg2.sourceforge.net:/cvsroot/libmpeg2
MPEG2DEC_SNAPSHOT=$(SF)/libmpeg2/mpeg2dec-snapshot.tar.gz
LIBID3TAG_VERSION=0.15.1b
LIBID3TAG_URL=ftp://ftp.mars.org/pub/mpeg/libid3tag-$(LIBID3TAG_VERSION).tar.gz
LIBMAD_VERSION=0.15.1b
LIBMAD_URL=ftp://ftp.mars.org/pub/mpeg/libmad-$(LIBMAD_VERSION).tar.gz
OGG_VERSION=1.1
OGG_URL=http://www.vorbis.com/files/1.0.1/unix/libogg-$(OGG_VERSION).tar.gz
OGG_CVSROOT=:pserver:anoncvs@xiph.org:/usr/local/cvsroot
VORBIS_VERSION=1.0
VORBIS_URL=http://us.xiph.org/ogg/vorbis/download/libvorbis-$(VORBIS_VERSION).tar.gz
THEORA_VERSION=1.0alpha4
THEORA_URL=$(VIDEOLAN)/testing/contrib/libtheora-$(THEORA_VERSION).tar.bz2
FLAC_VERSION=1.1.2
FLAC_URL=$(SF)/flac/flac-$(FLAC_VERSION).tar.gz
SPEEX_VERSION=1.1.5
SPEEX_URL=http://us.speex.org/download/speex-$(SPEEX_VERSION).tar.gz
FAAD2_VERSION=20040923
FAAD2_URL=$(VIDEOLAN)/testing/contrib/faad2-$(FAAD2_VERSION).tar.bz2
FAAD2_CVSROOT=:pserver:anonymous@cvs.audiocoding.com:/cvsroot/faac
FAAC_VERSION=1.24
FAAC_URL=$(VIDEOLAN)/testing/contrib/faac-$(FAAC_VERSION).tar.bz2
LAME_VERSION=3.93.1
LAME_URL=$(SF)/lame/lame-$(LAME_VERSION).tar.gz
LIBEBML_VERSION=0.7.5
LIBEBML_URL=http://dl.matroska.org/downloads/libebml/libebml-$(LIBEBML_VERSION).tar.bz2
LIBMATROSKA_VERSION=0.7.7
LIBMATROSKA_URL=http://dl.matroska.org/downloads/libmatroska/libmatroska-$(LIBMATROSKA_VERSION).tar.bz2
FFMPEG_VERSION=0.4.8
FFMPEG_URL=$(SF)/ffmpeg/ffmpeg-$(FFMPEG_VERSION).tar.gz
FFMPEG_CVSROOT=:pserver:anonymous@mplayerhq.hu:/cvsroot/ffmpeg
OPENSLP_VERSION=1.0.11
OPENSLP_URL=$(SF)/openslp/openslp-$(OPENSLP_VERSION).tar.gz
LIBDVDCSS_VERSION=1.2.8
LIBDVDCSS_URL=$(VIDEOLAN)/libdvdcss/$(LIBDVDCSS_VERSION)/libdvdcss-$(LIBDVDCSS_VERSION).tar.gz
LIBDVDREAD_VERSION=0.9.4
LIBDVDREAD_URL=http://download.videolan.org/pub/videolan/libdvdread/$(LIBDVDREAD_VERSION)/libdvdread-$(LIBDVDREAD_VERSION).tar.gz
LIBDVDNAV_VERSION=0.1.10
LIBDVDNAV_URL=$(VIDEOLAN)/testing/contrib/libdvdnav-$(LIBDVDNAV_VERSION).tar.gz
LIBDVBPSI_VERSION=0.1.5
LIBDVBPSI_URL=$(VIDEOLAN)/contrib/libdvbpsi3-$(LIBDVBPSI_VERSION).tar.gz
LIVEDOTCOM_VERSION=latest
LIVEDOTCOM_URL=http://www.live.com/liveMedia/public/live-$(LIVEDOTCOM_VERSION).tar.gz
#GOOM_URL=$(VIDEOLAN)/testing/contrib/goom-macosx-altivec-bin.tar.gz
GOOM2k4_VERSION=2k4-0
GOOM2k4_URL=$(SF)/goom/goom-$(GOOM2k4_VERSION)-src.tar.gz
LIBCACA_VERSION=0.9
LIBCACA_URL=http://sam.zoy.org/libcaca/libcaca-$(LIBCACA_VERSION).tar.gz
LIBDTS_VERSION=0.0.2
LIBDTS_URL=$(VIDEOLAN)/libdts/$(LIBDTS_VERSION)/libdts-$(LIBDTS_VERSION).tar.gz
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
GCRYPT_VERSION=1.2.0
GCRYPT_URL=$(VIDEOLAN)/testing/contrib/libgcrypt-$(GCRYPT_VERSION).tar.gz
GNUTLS_VERSION=1.2.6
GNUTLS_URL=ftp://ftp.gnutls.org/pub/gnutls/gnutls-$(GNUTLS_VERSION).tar.bz2
DAAP_VERSION=0.3.0
DAAP_URL=http://crazney.net/programs/itunes/files/libopendaap-$(DAAP_VERSION).tar.bz2
GLIB_VERSION=1.2.8
GLIB_URL=ftp://ftp.gtk.org/pub/gtk/v1.2/glib-1.2.8.tar.gz
LIBIDL_VERSION=0.6.8
LIBIDL_URL=http://andrewtv.org/libIDL/libIDL-$(LIBIDL_VERSION).tar.gz
MOZILLA_VERSION=1.7.5
MOZILLA_URL=http://ftp.mozilla.org/pub/mozilla.org/mozilla/releases/mozilla$(MOZILLA_VERSION)/source/mozilla-source-$(MOZILLA_VERSION).tar.bz2
TWOLAME_VERSION=0.3.2
TWOLAME_URL=$(SF)/twolame/twolame-$(TWOLAME_VERSION).tar.gz
X264_VERSION=20050609
X264_URL=$(VIDEOLAN)/testing/contrib/x264-$(X264_VERSION).tar.gz
