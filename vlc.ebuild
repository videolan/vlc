###############################################################################
# vlc.ebuild: A Gentoo ebuild for vlc
###############################################################################
# Copyright (C) 2003 VideoLAN
# $Id: vlc.ebuild,v 1.12 2003/07/10 00:47:42 hartman Exp $
#
# Authors: Derk-Jan Hartman <thedj at users.sf.net>
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
###############################################################################
# Instructions: http://wiki.videolan.org/index.php/Linux%20Gentoo
# Some of the ideas in this ebuild are derived from the official Gentoo ebuild
# Thanks to the Gentoo Team for supporting us.
###############################################################################

IUSE="arts qt ncurses dvd gtk nls 3dfx matrox svga fbcon esd kde X alsa ggi oggvorbis gnome xv oss sdl fbcon aalib slp truetype v4l xvid lirc wxwindows imlib matroska dvb pvr"

# Change these to correspond with the
# unpacked dirnames of the CVS snapshots.
PFFMPEG=ffmpeg-20030622
PLIBMPEG2=mpeg2dec-20030612

S=${WORKDIR}/${P}
SFFMPEG=${WORKDIR}/${PFFMPEG}
SLIBMPEG2=${WORKDIR}/${PLIBMPEG2}

DESCRIPTION="VLC media player - Video player and streamer"
SRC_URI="http://www.videolan.org/pub/${PN}/${PV}/${P}.tar.bz2
		 http://www.videolan.org/pub/${PN}/${PV}/contrib/mpeg2dec-20030612.tar.bz2
		 http://www.videolan.org/pub/${PN}/${PV}/contrib/ffmpeg-20030622.tar.bz2"

HOMEPAGE="http://www.videolan.org/vlc"

SLOT="0"
LICENSE="GPL-2"
KEYWORDS="~x86 ~ppc ~sparc ~alpha ~mips ~hppa"

DEPEND="X? ( virtual/x11 )
	nls? ( sys-devel/gettext )
	qt? ( x11-libs/qt )
	dvd? ( >=media-libs/libdvdread-0.9.3
		>=media-libs/libdvdcss-1.2.6
		>=media-libs/libdvdplay-1.0.1 )
	sdl? ( >=media-libs/libsdl-1.2.5 )
	esd? ( >=media-sound/esound-0.2.22 )
	ggi? ( >=media-libs/libggi-2.0_beta3 )
	gtk? ( =x11-libs/gtk+-1.2* )
	kde? ( kde-base/kdelibs )
	arts? ( kde-base/kdelibs )
	gnome? ( >=gnome-base/gnome-libs-1.4.1.2-r1 )
	ncurses? ( sys-libs/ncurses )
	oggvorbis? ( >=media-libs/libvorbis-1.0 
	             >=media-libs/libogg-1.0 )
	alsa? ( >=media-libs/alsa-lib-0.9_rc2 )
	aalib? ( >=media-libs/aalib-1.4_rc4-r2 )
	xvid? ( >=media-libs/xvid-0.9.1 )
	slp? ( >=net-libs/openslp-1.0.10 )
	truetype? ( >=media-libs/freetype-2.1.4 )
	lirc? ( app-misc/lirc )
	imlib? ( >=media-libs/imlib2-1.0.6 )
	wxwindows? ( >=x11-libs/wxGTK-2.4.0 )
	matroska? ( media-libs/libmatroska )
	dvb? ( media-libs/libdvb 
		media-tv/linuxtv-dvb )
        >=media-libs/libdvbpsi-0.1.2
	>=media-sound/mad-0.14.2b
	>=media-libs/faad2-1.1
	>=media-libs/a52dec-0.7.4
	>=media-libs/flac-1.1.0"

# Missing support for
#	tarkin
#	theora
#	tremor

# get kde and arts paths
if [ -n "`use kde`" -o -n "`use arts`" ]; then
    inherit kde-functions
    set-kdedir 3
    # $KDEDIR is now set to arts/kdelibs location
fi

src_unpack() {
	
	unpack ${A}
	cd ${S}

	# if qt3 is installed, patch vlc to work with it instead of qt2
	( use qt || use kde ) && ( \
	if [ ${QTDIR} = "/usr/qt/3" ]
	then
		cp configure configure.orig
		sed -e "s:-lkfile::" configure.orig > configure

		cd ${S}/modules/gui/kde
		cp interface.h interface.h.orig
		sed -e "s:\(#include <kmainwindow.h>\):\1\n#include <kstatusbar.h>:" \
			interface.h.orig > interface.h

		cp preferences.cpp preferences.cpp.orig
		sed -e 's:\("vlc preferences", true, false, \)\("Save\):\1(KGuiItem)\2:' \
			preferences.cpp.orig > preferences.cpp
	fi
	)

	# Change the location of the glide headers
	cd ${S}
	cp configure configure.orig
	sed -e "s:/usr/include/glide:/usr/include/glide3:" configure.orig > configure
	
	# patch libmpeg2
	cd ${SLIBMPEG2}
	cp configure configure.orig
	sed -e 's:OPT_CFLAGS=\"$CFLAGS -mcpu=.*\":OPT_CFLAGS=\"$CFLAGS\":g' \
		configure.orig > configure

}

src_compile(){
	# configure and building of libmpeg2
	cd ${SLIBMPEG2}
	econf --disable-sdl --without-x \
		|| die "./configure of libmpeg2 failed"
	
	emake || make || die "make of libmpeg2 failed"

	# configure and building of ffmpeg
	cd ${SFFMPEG}
	myconf=""
	use mmx || myconf="--disable-mmx"

	./configure ${myconf} \
		--disable-mp3lame \
		--disable-vorbis || die "./configure of ffmpeg failed"
	
	cd libavcodec
	make || die "make of ffmpeg failed"
	cd libpostproc
	make || die "make of libpostproc failed"

	# Configure and build VLC
	cd ${S}
	myconf=""

	use X || myconf="${myconf} --disable-x11"

	use xv || myconf="${myconf} --disable-xvideo"

	use ggi && myconf="${myconf} --enable-ggi"

        use 3dfx && myconf="${myconf} --enable-glide"

        use matrox && myconf="${myconf} --enable-mga"

        use svga && myconf="${myconf} --enable-svgalib"

	use sdl || myconf="${myconf} --disable-sdl"

        use fbcon || myconf="${myconf} --disable-fb"

        use aalib && myconf="${myconf} --enable-aa"

	use dvd \
		&& myconf="${myconf} --enable-dvdread" \
		|| myconf="${myconf} \
			--disable-dvd \
			--disable-dvdread \
			--disable-dvdplay \
			--disable-vcd"

	use alsa && myconf="${myconf} --enable-alsa"

        use oss || myconf="${myconf} --disable-oss"

	use esd && myconf="${myconf} --enable-esd"

	use arts && myconf="${myconf} --enable-arts"

	use nls || myconf="${myconf} --disable-nls"

	# the current gtk2 and gnome2 are prelimenary frameworks
	use gtk \
		&& myconf="${myconf} --disable-gtk2" \
		|| myconf="${myconf} --disable-gtk --disable-gtk2"

	use gnome && myconf="${myconf} --enable-gnome --disable-gnome2"

	use kde && myconf="${myconf} --enable-kde"

	use qt && myconf="${myconf} --enable-qt"

	use ncurses && myconf="${myconf} --enable-ncurses"

	use oggvorbis || myconf="${myconf} --disable-vorbis --disable-ogg"

	use lirc && myconf="${myconf} --enable-lirc"

	use slp || myconf="${myconf} --disable-slp"

	use truetype && myconf="${myconf} --enable-freetype"

	# xvid is a local USE var, see /usr/portage/profiles/use.local.desc for more details
	use xvid && myconf="${myconf} --enable-xvid"

	# v4l is a local USE var, see /usr/portage/profiles/use.local.desc for more details
	use v4l && myconf="${myconf} --enable-v4l"

	# wxwindows is a local USE var. already enabled by default, but depends on wxGTK
	# but if we use wxwindows and imlib, then we can also use skins
	(use imlib && use wxwindows) && myconf="${myconf} --enable-skins"

	# matroska is a local USE var. 
	# http://forums.gentoo.org/viewtopic.php?t=63722&highlight=matroska
	use matroska && myconf="${myconf} --enable-mkv"

	use dvb && myconf="${myconf} --enable-satellite"

	# pvr is a local USE var, see /usr/portage/profiles/use.local.desc for more details
        use pvr && myconf="${myconf} --enable-pvr"

	# vlc uses its own ultraoptimizaed CXXFLAGS
	# and forcing custom ones generally fails building
	export CXXFLAGS=""
	export CFLAGS=""
	export WANT_AUTOCONF_2_5=1
	export WANT_AUTOMAKE_1_6=1

	# Avoid timestamp skews with autotools
	touch configure.ac
	touch aclocal.m4
	touch configure
	touch config.h.in
	touch `find . -name Makefile.in`

	myconf="${myconf} --enable-ffmpeg --with-ffmpeg-tree=${SFFMPEG} \
		--enable-libmpeg2 --with-libmpeg2-tree=${SLIBMPEG2} \
		--enable-dvbpsi \
		--enable-release \
		--enable-mad \
		--enable-faad \
		--enable-flac \
		--enable-a52"

	ewarn ${myconf}
	econf ${myconf} || die "configure of VLC failed"

	# parallel make doesn't work with our complicated makefile
	# this is also the reason as why you shouldn't run autoconf
	# or automake yourself. (or bootstrap for that matter)
	make || die "make of VLC failed"
}

src_install() {
	
	einstall || die "make install failed"

	dodoc ABOUT-NLS AUTHORS COPYING ChangeLog HACKING INSTALL* \
	MAINTAINERS NEWS README* MODULES THANKS doc/ChangeLog-*

}
