###############################################################################
# vlc.ebuild: A Gentoo ebuild for vlc
###############################################################################
# Copyright (C) 2003 VideoLAN
# $Id: vlc-cvs.ebuild,v 1.1 2003/11/01 14:35:38 hartman Exp $
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

inherit gcc cvs eutils flag-o-matic libtool

# Missing support for...
#	tarkin - package not in portage yet - experimental
#	theora - package not in portage yet - experimental
#	tremor - package not in portage yet - experimental

IUSE="arts ncurses dvd gtk nls 3dfx svga fbcon esd X alsa ggi
      oggvorbis gnome xv oss sdl aalib slp truetype v4l xvid lirc 
	  wxwindows imlib matroska dvb mozilla debug faad xosd altivec"

DESCRIPTION="VLC media player - Video player and streamer"
HOMEPAGE="http://www.videolan.org/vlc"

ECVS_SERVER="anoncvs.videolan.org:/var/cvs/videolan"
ECVS_USER="anonymous"
ECVS_MODULE="vlc"

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
	>=media-sound/lame-3.93.1
	>=media-libs/libdvbpsi-0.1.3
	>=media-video/ffmpeg-0.4.8
	>media-libs/libmpeg2-0.3.1
	>=media-libs/a52dec-0.7.4
	>=media-libs/flac-1.1.0"

S=${WORKDIR}/vlc
RESTRICT="nostrip"

src_compile(){
	./bootstrap
	
	local myconf
	myconf="--disable-glide --disable-mga --enable-flac --with-gnu-ld"

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

	myconf="${myconf} --enable-ffmpeg \
		--with-ffmpeg-mp3lame \
		--enable-libmpeg2 \
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
