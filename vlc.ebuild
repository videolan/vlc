###############################################################################
# vlc.ebuild: A Gentoo ebuild for vlc
###############################################################################
# Copyright (C) 2003 VideoLAN
# $Id: vlc.ebuild,v 1.19 2003/08/24 08:12:01 hartman Exp $
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

# Missing support for...
#	tarkin - package not in portage yet - experimental
#	theora - package not in portage yet - experimental
#	tremor - package not in portage yet - experimental

IUSE="arts ncurses dvd gtk nls 3dfx svga fbcon esd X alsa ggi
      oggvorbis gnome xv oss sdl aalib slp truetype v4l xvid lirc 
	  wxwindows imlib matroska dvb mozilla debug faad xosd altivec"

# Change these to correspond with the
# unpacked dirnames of the CVS snapshots.
PFFMPEG=ffmpeg-20030813
PLIBMPEG2=mpeg2dec-20030612

S=${WORKDIR}/${P}
SFFMPEG=${WORKDIR}/${PFFMPEG}
SLIBMPEG2=${WORKDIR}/${PLIBMPEG2}

DESCRIPTION="VLC media player - Video player and streamer"
SRC_URI="http://www.videolan.org/pub/${PN}/${PV}/${P}.tar.bz2
		 http://www.videolan.org/pub/${PN}/${PV}/contrib/mpeg2dec-20030612.tar.bz2
		 http://www.videolan.org/pub/${PN}/${PV}/contrib/ffmpeg-20030813.tar.bz2"

HOMEPAGE="http://www.videolan.org/vlc"

SLOT="0"
LICENSE="GPL-2"
KEYWORDS="~x86 ~ppc ~sparc ~alpha ~mips ~hppa ~amd64"

DEPEND="X? ( virtual/x11 )
	aalib? ( >=media-libs/aalib-1.4_rc4-r2 )
	alsa? ( >=media-libs/alsa-lib-0.9_rc2 )
	dvb? ( media-libs/libdvb
		media-tv/linuxtv-dvb )
	dvd? ( >=media-libs/libdvdread-0.9.3
		>=media-libs/libdvdcss-1.2.8
		>=media-libs/libdvdplay-1.0.1 )
	esd? ( >=media-sound/esound-0.2.22 )
	faad? ( >=media-libs/faad2-1.1 )
	ggi? ( >=media-libs/libggi-2.0_beta3 )
	gnome? ( >=gnome-base/gnome-libs-1.4.1.2-r1 )
	gtk? ( =x11-libs/gtk+-1.2* )
	imlib? ( >=media-libs/imlib2-1.0.6 )
	lirc? ( app-misc/lirc )
	mad? ( media-libs/libmad
		media-libs/libid3tag )
	matroska? ( >=media-libs/libmatroska-0.4.4 )
	mozilla? ( >=net-www/mozilla-1.4 )
	ncurses? ( sys-libs/ncurses )
	nls? ( sys-devel/gettext )
	oggvorbis? ( >=media-libs/libvorbis-1.0
		>=media-libs/libogg-1.0 )
	sdl? ( >=media-libs/libsdl-1.2.5 )
	slp? ( >=net-libs/openslp-1.0.10 )
	truetype? ( >=media-libs/freetype-2.1.4 )
	wxwindows? ( >=x11-libs/wxGTK-2.4.1 )
	xosd? ( >=x11-libs/xosd-2.0 )
	xvid? ( >=media-libs/xvid-0.9.1 )
	3dfx? ( media-libs/glide-v3 )
	>=media-sound/lame-3.93.1
	>=media-libs/libdvbpsi-0.1.3
	>=media-libs/a52dec-0.7.4
	>=media-libs/flac-1.1.0"

inherit gcc

src_unpack() {
	
	unpack ${A}
	cd ${S}

	# Change the location of the glide headers
	cd ${S}
	sed -i -e "s:/usr/include/glide:/usr/include/glide3:" configure
	sed -i -e "s:glide2x:glide3:" configure
	cd ${S}/modules/video_output
	epatch ${FILESDIR}/glide.patch
	cd ${S}
	
	# patch libmpeg2
	cd ${SLIBMPEG2}
	sed -i -e 's:OPT_CFLAGS=\"$CFLAGS -mcpu=.*\":OPT_CFLAGS=\"$CFLAGS\":g' configure

}

src_compile(){
	# configure and building of libmpeg2
	cd ${SLIBMPEG2}
	econf --disable-sdl --without-x \
		|| die "./configure of libmpeg2 failed"
	
	emake || make || die "make of libmpeg2 failed"

	# configure and building of ffmpeg
	cd ${SFFMPEG}
	local myconf
	use mmx || myconf="--disable-mmx"

	./configure ${myconf} \
		--enable-mp3lame \
		--disable-vorbis || die "./configure of ffmpeg failed"
	
	cd libpostproc
	make || die "make of libpostproc failed"
	cd libavcodec
	make || die "make of libavcodec failed"

	# Configure and build VLC
	cd ${S}
	
	# Avoid timestamp skews with autotools
	touch configure.ac
	touch aclocal.m4
	touch configure
	touch config.h.in
	touch `find . -name Makefile.in`
	
	local myconf
	myconf="--disable-mga --enable-flac --with-gnu-ld"

    #--enable-pth				GNU Pth support (default disabled)
	#--enable-st				State Threads (default disabled)
	#--enable-gprof				gprof profiling (default disabled)
	#--enable-cprof				cprof profiling (default disabled)
	#--enable-mostly-builtin	most modules will be built-in (default enabled)
	#--disable-optimizations	disable compiler optimizations (default disabled)
	#--enable-testsuite			build test modules (default disabled)
	#--disable-plugins			make all plugins built-in (default disabled)

	use nls || myconf="${myconf} --disable-nls"
	
	use debug && myconf="${myconf} --enable-debug" \
		|| myconf="${myconf} --enable-release"
	
	use dvd \
		&& myconf="${myconf} --enable-dvdread" \
		|| myconf="${myconf} \
			--disable-dvd \
			--disable-dvdread \
			--disable-dvdplay \
			--disable-vcd"

	use v4l && myconf="${myconf} --enable-v4l"

	use dvb && myconf="${myconf} --enable-satellite --enable-pvr --enable-dvb"

	use oggvorbis || myconf="${myconf} --disable-vorbis --disable-ogg"

	use matroska || myconf="${myconf} --disable-mkv"

	use mad || myconf="${myconf} --disable-mad"

	use faad && myconf="${myconf} --enable-faad"

	use xvid && myconf="${myconf} --enable-xvid"

	use X || myconf="${myconf} --disable-x11"

	use xv || myconf="${myconf} --disable-xvideo"

	use sdl || myconf="${myconf} --disable-sdl"

	use truetype || myconf="${myconf} --disable-freetype"

	use fbcon || myconf="${myconf} --disable-fb"

	use svga && myconf="${myconf} --enable-svgalib"

	use ggi && myconf="${myconf} --enable-ggi"

	use 3dfx && myconf="${myconf} --enable-glide"

	use aalib && myconf="${myconf} --enable-aa"

	use oss || myconf="${myconf} --disable-oss"

	use esd && myconf="${myconf} --enable-esd"

	use arts && myconf="${myconf} --enable-arts"

	use alsa && myconf="${myconf} --enable-alsa"

	(use imlib && use wxwindows) && myconf="${myconf} --enable-skins"

	use gtk || myconf="${myconf} --disable-gtk"

	use gnome && myconf="${myconf} --enable-gnome"

	use ncurses && myconf="${myconf} --enable-ncurses"

	use xosd && myconf="${myconf} --enable-xosd"

	use slp || myconf="${myconf} --disable-slp"

	use lirc && myconf="${myconf} --enable-lirc"

	use joystick && myconf="${myconf} --enable-joystick"

	use mozilla && \
		myconf="${myconf} --enable-mozilla \
	    MOZILLA_CONFIG=/usr/lib/mozilla/mozilla-config \
	    XPIDL=/usr/bin/xpidl"

	use altivec || myconf="${myconf} --disable-altivec"

	# vlc uses its own ultraoptimizaed CXXFLAGS
	# and forcing custom ones generally fails building
	export CXXFLAGS=""
	export CFLAGS=""
	export WANT_AUTOCONF_2_5=1
	export WANT_AUTOMAKE_1_6=1

	myconf="${myconf} --enable-ffmpeg --with-ffmpeg-tree=${SFFMPEG} \
		--with-ffmpeg-mp3lame \
		--enable-libmpeg2 --with-libmpeg2-tree=${SLIBMPEG2} \
		--enable-flac \
		--disable-kde \
		--disable-qt"

	econf ${myconf} || die "configure of VLC failed"

	if [ `gcc-major-version` -eq 2 ]; then
		sed -i s:"-fomit-frame-pointer":: vlc-config
	fi

	# parallel make doesn't work with our complicated makefile
	# this is also the reason as why you shouldn't run autoconf
	# or automake yourself. (or bootstrap for that matter)
	MAKEOPTS="${MAKEOPTS} -j1"
	emake || die "make of VLC failed"
}

src_install() {
	
	einstall || die "make install failed"

	dodoc ABOUT-NLS AUTHORS COPYING ChangeLog HACKING INSTALL* \
	MAINTAINERS NEWS README* THANKS doc/ChangeLog-*

}
