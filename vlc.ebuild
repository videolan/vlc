###############################################################################
# vlc.ebuild: A Gentoo ebuild for vlc
###############################################################################
# Copyright (C) 2003 VideoLAN
# $Id: vlc.ebuild,v 1.4 2003/05/23 00:00:48 hartman Exp $
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
IUSE="arts qt ncurses dvd gtk nls 3dfx esd kde X alsa ggi oggvorbis gnome xv oss sdl fbcon aalib avi slp truetype"

# Change these to correspond with the
# unpacked dirnames of the CVS snapshots.
PFFMPEG=ffmpeg-20030517
PLIBMPEG2=mpeg2dec-20030418

S=${WORKDIR}/${P}
SFFMPEG=${WORKDIR}/${PFFMPEG}
SLIBMPEG2=${WORKDIR}/${PLIBMPEG2}

DESCRIPTION="VLC media player - A videoplayer that plays DVD,
             VCD, files and networkstreams o.a."

# Use the correct CVS snapshot links. 
SRC_URI="http://www.videolan.org/pub/testing/${P}/${P}.tar.bz2
         http://www.videolan.org/pub/testing/contrib/ffmpeg-20030517.tar.bz2
	 http://www.videolan.org/pub/testing/contrib/mpeg2dec-20030418.tar.bz2"

#SRC_URI="http://www.videolan.org/pub/videolan/${PN}/${PV}/${P}.tar.bz2
#		 http://www.videolan.org/pub/videolan/${PN}/${PV}/contrib/libmpeg2.tar.bz2
#		 http://www.videolan.org/pub/videolan/${PN}/${PV}/contrib/ffmpeg.tar.bz2"
HOMEPAGE="http://www.videolan.org"

SLOT="0"
LICENSE="GPL-2"
KEYWORDS="~x86 ~ppc"

DEPEND="X? ( virtual/x11 )
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
	avi? ( >=media-libs/xvid-0.9.1 )
	slp? ( >=net-libs/openslp-1.0.10 )
	truetype? ( >=media-libs/freetype-2.1.4 )
	>=media-sound/mad-0.14.2b
	>=media-libs/a52dec-0.7.4
	>=media-libs/faad2-1.1
	>=media-libs/libdvbpsi-0.1.2"
# other optional libraries
#	>=media-libs/flac-1.1.0
#	>=media-libs/libdv-0.98
# wxwindows ought to be a USE variable, like all the others
#	>=x11-libs/wxGTK-2.4.0

# not in gentoo
#	tarkin
#	theora
#	tremor

RDEPEND="nls? ( sys-devel/gettext )"

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
		cp configure.ac configure.ac.orig
		sed "s:-lkfile::" \
			configure.ac.orig > configure.ac
		# adding configure.ac.in
		cp configure.ac.in configure.ac.in.orig
		sed "s:-lkfile::" \
			configure.ac.in.orig > configure.ac.in

		cd ${S}/modules/gui/kde
		cp interface.h interface.h.orig
		sed "s:\(#include <kmainwindow.h>\):\1\n#include <kstatusbar.h>:" \
			interface.h.orig > interface.h

		cp preferences.cpp preferences.cpp.orig
		sed 's:\("vlc preferences", true, false, \)\("Save\):\1(KGuiItem)\2:' \
			preferences.cpp.orig > preferences.cpp
	fi
	)
	
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
	
	use X \
		&& myconf="${myconf} --enable-x11" \
		|| myconf="${myconf} --disable-x11"

	use xv \
		&& myconf="${myconf} --enable-xvideo" \
		|| myconf="${myconf} --diable-xvideo"
		
	use qt \
		&& myconf="${myconf} --enable-qt" \
		|| myconf="${myconf} --disable-qt"
	
	use dvd \
		&& myconf="${myconf} \
			--enable-dvd \
			--enable-dvdread \
			--enable-vcd" \
		|| myconf="${myconf} \
			--disable-dvd \
			--disable-dvdread \
			--disable-vcd"
	
	use esd \
		&& myconf="${myconf} --enable-esd" \
		|| myconf="${myconf} --disable-esd"

	use ggi \
		&& myconf="${myconf} --enable-ggi" \
		|| myconf="${myconf} --disable-ggi"
	
	# the current gtk2 and gnome2 are prelimenary frameworks
	use gtk \
		&& myconf="${myconf} --enable-gtk --disable-gtk2" \
		|| myconf="${myconf} --disable-gtki --disable-gtk2"

	use kde \
		&& myconf="${myconf} --enable-kde" \
		|| myconf="${myconf} --disable-kde"

	use nls \
		|| myconf="${myconf} --disable-nls"
	
	use 3dfx \
		&& myconf="${myconf} --enable-glide" \
		|| myconf="${myconf} --disable-glide"

	use arts \
		&& myconf="${myconf} --enable-arts" \
		|| myconf="${myconf} --disable-arts"
	
	use gnome \
		&& myconf="${myconf} --enable-gnome --disable-gnome2" \
		|| myconf="${myconf} --disable-gnome --disable-gnome2"
	
	use ncurses \
		&& myconf="${myconf} --enable-ncurses" \
		|| myconf="${myconf} --disable-ncurses"
	
	use oggvorbis \
		&& myconf="${myconf} --enable-vorbis --enable-ogg" \
		|| myconf="${myconf} --disable-vorbis --disable-ogg"

	use alsa \
		&& myconf="${myconf} --enable-alsa" \
		|| myconf="${myconf} --disable-alsa"
		
	use oss \
		&& myconf="${myconf} --enable-oss" \
		|| myconf="${myconf} --disable-oss"

	use sdl \
		&& myconf="${myconf} --enable-sdl" \
		|| myconf="${myconf} --disable-sdl"
	
	use fbcon \
		&& myconf="${myconf} --enable-fb" \
		|| myconf="${myconf} --disable-fb"

	use aalib \
		&& myconf="${myconf} --enable-aa" \
		|| myconf="${myconf} --disable-aa"

	# there is no xvid USE variable
	# it is pretty reasonable that users with the avi USE variable
	# want xvid too cq. not.
	use avi \
		&& myconf="${myconf} --enable-xvid" \
		|| myconf="${myconf} --disable-xvid"

	use slp \
		&& myconf="${myconf} --enable-slp" \
		|| myconf="${myconf} --disable-slp"

	# vlc uses its own ultraoptimizaed CXXFLAGS
	# and forcing custom ones generally fails building
	export CXXFLAGS=""
	export CFLAGS=""
	export WANT_AUTOCONF_2_5=1
	export WANT_AUTOMAKE_1_6=1

	econf \
		--enable-ffmpeg --with-ffmpeg-tree=${SFFMPEG} \
		--enable-libmpeg2 --with-libmpeg2-tree=${SLIBMPEG2} \
		--with-sdl \
		--enable-release \
		--enable-mad \
		--enable-a52 \
		--enable-dvbpsi \
		${myconf} || die "configure of VLC failed"

	# parallel make doesn't work with our complicated makefile
	# this is also the reason as why you shouldn't run autoconf
	# or automake yourself. (or bootstrap for that matter)
	make || die "make of VLC failed"
}

src_install() {
	
	einstall || die "make install failed"

	dodoc ABOUT-NLS AUTHORS COPYING ChangeLog HACKING INSTALL* \
	MAINTAINERS NEWS README* MODULES THANKS

}
