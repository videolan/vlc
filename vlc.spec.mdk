%define name 		vlc
%define version 	0.6.0
%define rel		5
%define ffcvs		20030622
%define mpegcvs		20030612

%define libmajor	0

%define cvs     	0
%if %{cvs}
%define cvsrel		1
%define cvsdate 	20030203
%define release		0.%{cvsdate}.%{cvsrel}mdk
%define cvs_name 	%{name}-snapshot-%cvsdate
%else
%define release 	%{rel}mdk
%endif

%define with_dvdplay 1

%define with_mozilla 1
%define with_gtk 1
%define with_gnome 1
%define with_qt 0
%define with_kde 1
%define with_ncurses 1
%define with_lirc 1
%define with_wx 1

%define with_aa 1
%define with_sdl 1
%define with_ggi 1
%define with_svgalib 0
%define with_xosd 1

%define with_mad 1
%define with_ogg 1
%define with_flac 1
%define with_mkv 1
%define with_a52 1
%define with_dv 1
%define with_dvb 1
%define with_ffmpeg 1
%define with_mpeg2dec 1

%define with_esd 1
%define with_arts 1
%define with_alsa 1

%define with_slp 1
%define with_tar 1

%define buildfor_rh80	0
%define buildfor_mdk82	0
%define buildfor_mdk90	0
%define buildfor_mdk91  %(awk '{print ($4 == "9.1")}' %{_sysconfdir}/mandrake-release)
%define buildfor_mdk92  %(awk '{print ($4 == "9.2")}' %{_sysconfdir}/mandrake-release)

# new macros
%if %buildfor_mdk82 || %buildfor_mdk90 || %buildfor_rh80
%define libname		lib%name%libmajor
%else
%define libname		%mklibname %name %libmajor
%endif

%if %buildfor_rh80
%define release %rel
# some mdk macros that do not exist in rh
%define configure2_5x CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%_prefix --libdir=%_libdir
%define make %__make %_smp_mflags
%define makeinstall_std %__make DESTDIR="$RPM_BUILD_ROOT" install
# adjust define for Redhat.
%endif

# without
%{?_without_mozilla:	%{expand: %%global with_mozilla 0}}
%{?_without_gtk:	%{expand: %%global with_gtk 0}}
%{?_without_gnome:	%{expand: %%global with_gnome 0}}
%{?_without_qt:		%{expand: %%global with_qt 0}}
%{?_without_kde:	%{expand: %%global with_kde 0}}
%{?_without_ncurses:	%{expand: %%global with_ncurses 0}}
%{?_without_lirc:	%{expand: %%global with_lirc 0}}
%{?_without_wx:		%{expand: %%global with_wx 0}}

%{?_without_aa:   	%{expand: %%global with_aa 0}}
%{?_without_sdl:   	%{expand: %%global with_sdl 0}}
%{?_without_ggi:   	%{expand: %%global with_ggi 0}}
%{?_without_svgalib:	%{expand: %%global with_svgalib 0}}
%{?_without_xosd:	%{expand: %%global with_xosd 0}}

%{?_without_mad:	%{expand: %%global with_mad 0}}
%{?_without_ogg:	%{expand: %%global with_ogg 0}}
%{?_without_flac:	%{expand: %%global with_flac 0}}
%{?_without_mkv:	%{expand: %%global with_mkv 0}}
%{?_without_a52:	%{expand: %%global with_a52 0}}
%{?_without_dv:		%{expand: %%global with_dv 0}}
%{?_without_dvb:	%{expand: %%global with_dvb 0}}

%{?_without_esd:	%{expand: %%global with_esd 0}}
%{?_without_arts:	%{expand: %%global with_arts 0}}
%{?_without_alsa:	%{expand: %%global with_alsa 0}}

%{?_without_slp:	%{expand: %%global with_slp 0}}
%{?_without_tar:	%{expand: %%global with_tar 0}}

# with
%{?_with_mozilla:    	%{expand: %%global with_mozilla 1}}
%{?_with_gtk:		%{expand: %%global with_gtk 1}}
%{?_with_gnome:		%{expand: %%global with_gnome 1}}
%{?_with_qt:		%{expand: %%global with_qt 1}}
%{?_with_kde:		%{expand: %%global with_kde 1}}
%{?_with_ncurses:    	%{expand: %%global with_ncurses 1}}
%{?_with_lirc:       	%{expand: %%global with_lirc 1}}
%{?_with_wx:		%{expand: %%global with_wx 1}}

%{?_with_aa:         	%{expand: %%global with_aa 1}}
%{?_with_sdl:        	%{expand: %%global with_sdl 1}}
%{?_with_ggi:        	%{expand: %%global with_ggi 1}}
%{?_with_svgalib:    	%{expand: %%global with_svgalib 1}}
%{?_with_xosd:       	%{expand: %%global with_xosd 1}}

%{?_with_mad:		%{expand: %%global with_mad 1}}
%{?_with_ogg:        	%{expand: %%global with_ogg 1}}
%{?_with_flac:        	%{expand: %%global with_flac 1}}
%{?_with_mkv:        	%{expand: %%global with_mkv 1}}
%{?_with_a52:        	%{expand: %%global with_a52 1}}
%{?_with_dv:         	%{expand: %%global with_dv 1}}
%{?_with_dvb:        	%{expand: %%global with_dvb 1}}

%{?_with_esd:        	%{expand: %%global with_esd 1}}
%{?_with_arts:       	%{expand: %%global with_arts 1}}
%{?_with_alsa:       	%{expand: %%global with_alsa 1}}

%{?_with_slp:		%{expand: %%global with_slp 1}}
%{?_with_tar:		%{expand: %%global with_tar 1}}

Summary:	VLC media player, a multimedia player and streaming application.
Name:		%{name}
Version:	%{version}
Release:	%{release}

%if %{cvs}
Source0:	http://download.videolan.org/pub/videolan/vlc/snapshots/%{cvs_name}.tar.bz2
%else
Source0:	http://download.videolan.org/packages/%{version}/%{name}-%{version}.tar.bz2
%endif
Source1:	http://download.videolan.org/pub/videolan/vlc/0.6.0/contrib/ffmpeg-%ffcvs.tar.bz2
Source2:	http://download.videolan.org/pub/videolan/vlc/0.6.0/contrib/mpeg2dec-%mpegcvs.tar.bz2	
#gw remove NP_GetValue, as it was already defined in the mozilla headers
Patch:		vlc-0.6.0-mozilla-conflict.patch.bz2
License:	GPL
Group:		Video
URL:		http://www.videolan.org/
Requires:	vlc-gui
# vlc-mad needed by ffmpeg builtin (i want MPEG4 support out of box)
Requires:	vlc-plugin-mad
# DVD working out of box.
Requires:	vlc-plugin-a52

BuildRoot:	%_tmppath/%name-%version-%release-root
%if %with_tar
BuildRequires:  libtar-devel
%endif
BuildRequires:  freetype2-devel
%if %with_mozilla
Buildrequires:	mozilla-devel >= 1.3
%endif
%if %with_gtk
Buildrequires:	libgtk+1.2-devel
%endif
%if %with_gnome
Buildrequires:	gnome-libs-devel
%endif
%if %with_qt
Buildrequires:	libqt3-devel
%endif
%if %with_kde
Buildrequires:	kdelibs-devel
%endif
%if %with_ncurses
Buildrequires:	libncurses5-devel
%if %with_wx
Buildrequires:	wxGTK-devel >= 2.4
%endif
%endif
%if %with_lirc
Buildrequires:	liblirc-devel
%endif
%if %with_aa
Buildrequires:	aalib-devel
%endif
%if %with_sdl
Buildrequires:	SDL-devel
%endif
%if %with_ggi
Buildrequires:	libggi-devel
%endif
%if %with_svgalib
Buildrequires:	svgalib-devel
%endif
%if %with_xosd
Buildrequires:	libxosd-devel
%endif
%if %with_mad
%if %buildfor_mdk92
BuildRequires:  libid3tag-devel
%endif
Buildrequires:	libmad-devel
%endif
%if %with_ogg
Buildrequires:	libvorbis-devel
Buildrequires:	libogg-devel
%endif
%if %with_flac
Buildrequires:	libflac-devel
%endif
%if %with_mkv
Buildrequires:	libmatroska-devel >= 0.4.4-3mdk
%endif
%if %with_dv
Buildrequires:	libdv2-devel
%endif

%if %with_a52
Buildrequires:	liba52dec-devel
%endif

%if %with_ffmpeg
#gw we use included cvs version
#Buildrequires:	libffmpeg-devel
%endif

%if %with_mpeg2dec
#gw we use the included cvs version
#Buildrequires:	libmpeg2dec-devel >= 0.3.2
%endif

%if %with_alsa
Buildrequires:	libalsa2-devel
%endif
%if %with_esd
Buildrequires:	libesound0-devel
%endif
%if %with_arts
Buildrequires:	libarts-devel
%endif

%if %with_slp
Buildrequires:	libopenslp-devel
%endif

%if %with_dvdplay
BuildRequires: libdvdplay-devel
%endif


%if %with_dvb
BuildRequires: libdvbpsi-devel
%if %buildfor_mdk92
# gw the cooker kernel has the new incompatible DVB api
BuildRequires: kernel-multimedia-source
%else
BuildRequires: kernel-source
%endif
%endif


%description
VideoLAN is an OpenSource streaming solution for every OS developed by
students from the Ecole Centrale Paris and developers from all over the
World.
VLC media player is a highly portable multimedia player for various audio 
and video formats (MPEG-1, MPEG-2, MPEG-4, DivX, mp3, ogg, ...) as well as 
DVD's, VCD's, and various streaming protocols. It can also be used as a 
server to stream in unicast or multicast in IPv4 or IPv6 on a 
high-bandwidth network.
You may install vlc-gnome or vlc-gtk to have a nice graphical interface.
This package contains no CSS unscrambling functionality for DVDs ;
you need the libdvdcss library available from
http://www.videolan.org/libdvdcss/ or http://plf.zarb.org/

#general packages
%package -n %libname-devel
Summary: Development files for the VLC media player
Group: Development/C
Requires: %name = %version-%release
Provides: %name-devel = %version-%release
Provides: lib%name-devel = %version-%release
%description -n %libname-devel
Development files for the VLC media player
This package contains headers and a static library required to build plugins
for the VLC media player, or standalone applications using features from VLC.

%package -n mozilla-plugin-vlc
Summary: A multimedia plugin for Mozilla, based on vlc
group: Video
Requires: %name = %version-%release
%if %buildfor_mdk91
%define moz_ver 1.3.1
%else
%define moz_ver 1.4b
%endif
##%{e###xpand: %%define mozve %(rpm -q --queryformat "%{version}\n" mozilla)}
%{expand: %%define mozve %(rpm -q mozilla| sed 's/mozilla-\([0-9].*\)-.*$/\1/')}
Requires: mozilla = %mozve
%description -n mozilla-plugin-vlc
This plugin adds support for MPEG, MPEG2, DVD and DivX to your Mozilla
browser. The decoding process is done by vlc and the output window is
embedded in a webpage or directly in the browser window. There is also
support for fullscreen display.


# intf plugins
%package -n gvlc
Summary: Gtk plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
Provides: vlc-gui
%description -n gvlc
This plugin adds a Gtk+ interface to the VLC media player. To
activate it, use the `--intf gtk' flag or run the `gvlc' program.

%package -n gnome-vlc
Summary: Gnome plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
Provides: vlc-gui, vlc-gnome
Obsoletes: vlc-gnome
%description -n gnome-vlc
This plugin adds a Gnome interface to the VLC media player. To
activate it, use the `--intf gnome' flag or run the `gnome-vlc' program.

%package -n qvlc
Summary: QT plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
Provides: vlc-gui, vlc-qt
Obsoletes: vlc-qt
%description -n qvlc
This plugin adds a Qt interface to the VLC media player. To activate it,
use the `--intf qt' flag or run the `qvlc' program.

%package -n kvlc
Summary: KDE frontend for the VLC media player
Group: Video
Requires: %{name} = %{version}
Provides: vlc-gui
%description -n kvlc
This plugin adds a KDE interface to the VLC media player. To
activate it, use the `--intf kde' flag or run the `kvlc' program.

%package plugin-ncurses
Summary: Ncurses console-based plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
%description plugin-ncurses
This plugin adds a ncurses interface to the VLC media player. To
activate it, use the `--intf ncurses' flag.

%package plugin-lirc
Summary: Lirc plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
Obsoletes: vlc-lirc
Provides: vlc-lirc
%description plugin-lirc
This plugin is an infrared lirc interface for the VLC media player. To
activate it, use the `--extraintf lirc' flag.

%package -n wxvlc
Summary: WxWindow plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
Obsoletes: vlc-lirc
Provides: vlc-lirc
Provides: vlc-gui
%description -n wxvlc
This plugin adds a wxWindow interface to the VLC media player. To
activate it, use the `--intf wxwin' flag or run the `wxvlc' program.


#
# video plugins
%package plugin-aa
Summary: ASCII art video plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
Obsoletes: vlc-aa
Provides: vlc-aa
%description plugin-aa
This is an ASCII art video output plugin for the VLC media playe. To
activate it, use the `--vout aa' flag or select the `aa' video output
plugin from the preferences menu.


%package plugin-sdl
Summary: Simple DirectMedia Layer video plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
Obsoletes: vlc-sdl
Provides: vlc-sdl
%description plugin-sdl
This plugin adds support for the Simple DirectMedia Layer library to
the VLC media player. To activate it, use the `--vout sdl' or
`--aout sdl' flags or select the `sdl' video or audio output plugin
from the preferences menu.

%package plugin-ggi
Summary: GGI video plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
Obsoletes: vlc-ggi
Provides: vlc-ggi
%description plugin-ggi
This is a GGI plugin for the VLC media player. To activate it, use
the `--vout ggi' flag or select the `ggi' video output plugin from
the preferences menu.

%package plugin-svgalib
Summary: SVGAlib video plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
%description plugin-svgalib
This plugin adds support for SVGAlib to the VLC media player. To
activate it, use the `--vout svgalib' flag or select the `svgalib' video
output plugin from the preferences menu. Note that you will need root
permissions to use SVGAlib.


#
# visualization plugins
%package plugin-xosd
Summary: X On-Screen Display plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
%description plugin-xosd
This is an On-Screen Display plugin for the VLC media player. To activate
it, use the `--extraintf xosd' flag or select the `xosd' interface plugin
from the preferences menu.

# codec plugins
%package plugin-mad
Summary: MAD audio codec plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
Obsoletes: vlc-mad
Provides: vlc-mad
%description plugin-mad
This plugin adds support for libmad, the MPEG audio decoder library,
to the VLC media player. MAD is 100% fixed-point based. To activate
this plugin, use the `--mpeg_adec mad' flag or select the `mad' MPEG
decoder from the preferences menu.

%package plugin-ogg
Summary: Ogg demuxer and Vorbis codec plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
%description plugin-ogg
These plugins add support for the Ogg bitstream format and the Ogg Vorbis
compressed audio format to the VLC media player. They are autodetected.

%package plugin-flac
Summary: Flac codec plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
%description plugin-flac
These plugins add support for the FLAC compressed audio format to the
VLC media player.

%package plugin-a52
Summary: A-52 (AC-3) codec plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
%description plugin-a52
This plugin adds support for the ATSC A-52 (aka. AC-3) audio format to
the VLC media player. The plugin is autodetected.

%package plugin-dv
Summary: DV codec plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
%description plugin-dv
This plugin adds support for the DV video format to the VLC media player.
The plugin is autodetected.

#
# input plugins
%package plugin-dvb
Summary: DVB input plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
%description plugin-dvb
This plugin adds support for DVB cards to the VLC media player. Note
that your card needs to be supported by your kernel before vlc can use it.

#
# audio plugins
%package plugin-esd
Summary: Enlightened Sound Daemon audio plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
Obsoletes: vlc-esd
Provides: vlc-esd
%description plugin-esd
This plugin adds support for the Enlightened Sound Daemon to the VLC
media player. To activate it, use the `--aout esd' flag or select the
`esd' audio output plugin from the preferences menu.

%package plugin-arts
Summary: Arts audio plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
Obsoletes: vlc-arts
Provides: vlc-arts
%description plugin-arts
This plugin adds support for the aRts Sound System to the VLC media
player. To activate it, use the `--aout arts' flag or select the `arts'
audio output plugin from the preferences menu.

%package plugin-alsa
Summary: Advanced Linux Sound Architecture audio plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
Obsoletes: vlc-alsa
Provides: vlc-alsa
%description plugin-alsa
This plugin adds support for the Advanced Linux Sound Architecture to
the VLC media player. To activate it, use the `--aout alsa' flag or
select the `alsa' audio output plugin from the preferences menu.


%package plugin-slp
Summary: Service Location Protocol acces plugin for the VLC media player
Group: Video
Requires: %{name} = %{version}
%description plugin-slp
This plugin adds support for the Service Location Protocol to
the VLC media player.


%prep
%if %{cvs}
%setup -q -n %{cvs_name} -a 1 -a 2
%else
%setup -q -a 1 -a 2
%endif
%patch -p1
rm -rf doc/skins/curve_maker/CVS

%build
cd mpeg2dec-%mpegcvs
%configure2_5x --disable-sdl --without-x
%make
cd ..
# yves 0.4.0-1mdk
# ffmpeg: static linking cause no official ffmpeg release with a stable ABI
# ffmpeg:no plugin posible on ia64 due to the static linking (can not put .a in a .so)
cd ffmpeg-%ffcvs
./configure --libdir=%_libdir
%make
cd ..
export XPIDL=/usr/lib/mozilla-%moz_ver/xpidl
perl -pi -e  's#-I/usr/share/idl/mozilla#-I/usr/share/idl/mozilla-%{moz_ver}#' Makefile.in
export QTDIR=%{_libdir}/qt3
# mandrake kernel specific
export CPPFLAGS="${CPPFLAGS:--I/usr/src/linux/3rdparty/mod_dvb/include}"
# gw flags for the mozilla build 
export CPPFLAGS="$CPPFLAGS -DOJI -DMOZ_X11"
# add missing ebml include dir
export CPPFLAGS="$CPPFLAGS -I/usr/include/ebml"
# NO empty line or comments for the configure --switch or it won't work.
%configure2_5x  --enable-release \
	--enable-dvd --without-dvdcss \
%if %with_dvdplay
	--enable-dvdplay \
%else
	--disable-dvdplay \
%endif
%if %with_mozilla
	--enable-mozilla \
%else
	--disable-mozilla \
%endif
%if %with_gtk
	--enable-gtk \
%else
	--disable-gtk \
%endif
%if %with_gnome
	--enable-gnome \
%else
	--disable-gnome \
%endif
%if %with_qt
	--enable-qt \
%endif
%if %with_kde
	--enable-kde \
%endif
%if %with_ncurses
	--enable-ncurses \
%endif
%if %with_lirc
	--enable-lirc \
%endif
%if %with_wx
	--enable-wxwindows \
%else 
	--disable-wxwindows \
%endif
	--enable-x11 --enable-xvideo \
	--enable-fb --disable-mga \
%if %with_aa
	--enable-aa \
%endif
%if %with_sdl
	--enable-sdl \
%endif
%if %with_ggi
	--enable-ggi \
%endif
%if %with_svgalib
        --enable-svgalib \
%endif
%if %with_xosd
	--enable-xosd \
%else
	--disable-xosd \
%endif
%if %with_mad
        --enable-mad \
%endif  
%if %with_ffmpeg
        --enable-ffmpeg --with-ffmpeg-tree=ffmpeg-%ffcvs \
%else
        --disable-ffmpeg \
%endif
%if %with_ogg
	--enable-vorbis \
	--enable-ogg \
%else
	--disable-vorbis \
	--disable-ogg \
%endif
%if %with_flac
	--enable-flac \
%else
	--disable-flac \
%endif
%if %with_mkv
	--enable-mkv \
%else
	--disable-mkv \
%endif	
%if %with_dv
	--enable-dv \
%else
	--disable-dv \
%endif
%if %with_dvb
	--enable-dvb  --enable-dvbpsi --enable-satellite \
%else
	--disable-dvb  --disable-dvbpsi --disable-satellite \
%endif
%if %with_esd
	--enable-esd \
%endif
%if %with_alsa
	--enable-alsa \
%endif
%if %with_arts
	--enable-arts \
%endif
%if %with_mpeg2dec
	--enable-libmpeg2 --with-libmpeg2-tree=mpeg2dec-%mpegcvs \
%else
~	--disable-libmpeg2 \
%endif

%make

%install
rm -rf %buildroot
%makeinstall_std
%find_lang %name
install -d %buildroot/%_mandir/man1
install doc/vlc.1 %buildroot/%_mandir/man1
install doc/vlc-config.1 %buildroot/%_mandir/man1

# menu
mkdir -p %buildroot/%_menudir
cat > %buildroot/%_menudir/vlc << EOF
?package(vlc): command="%_bindir/vlc" hotkey="V" needs="X11" longtitle="VLC is a free MPEG, MPEG2, DVD and DivX player" section="Multimedia/Video" title="VLC media player" icon="vlc.png" hints="Video" \
mimetypes="video/mpeg,video/msvideo,video/quicktime,video/x-avi,video/x-ms-asf,video/x-ms-wmv,video/x-msvideo,application/x-ogg,application/ogg,audio/x-mp3,audio/x-mpeg,video/x-fli,audio/x-wav"\
accept_url="true"\
multiple_files="true"
EOF
%if %with_gtk
cat > %buildroot/%_menudir/gvlc << EOF
?package(gvlc): command="%_bindir/gvlc" needs="X11" longtitle="VLC is a free MPEG, MPEG2, DVD and DivX player" section="Multimedia/Video" title="VLC Gtk media player" icon="gvlc.png" hints="Video" \
mimetypes="video/mpeg,video/msvideo,video/quicktime,video/x-avi,video/x-ms-asf,video/x-ms-wmv,video/x-msvideo,application/x-ogg,application/ogg,audio/x-mp3,audio/x-mpeg,video/x-fli,audio/x-wav"\
accept_url="true"\
multiple_files="true"
EOF
%endif
%if %with_gnome
cat > %buildroot/%_menudir/gnome-vlc << EOF
?package(gnome-vlc): command="%_bindir/gnome-vlc" needs="X11" longtitle="VLC is a free MPEG, MPEG2, DVD and DivX player" section="Multimedia/Video" title="VLC Gnome media player" icon="gnome-vlc.png" hints="Video" \
mimetypes="video/mpeg,video/msvideo,video/quicktime,video/x-avi,video/x-ms-asf,video/x-ms-wmv,video/x-msvideo,application/x-ogg,application/ogg,audio/x-mp3,audio/x-mpeg,video/x-fli,audio/x-wav"\
accept_url="true"\
multiple_files="true"
EOF
%endif
%if %with_qt
cat > %buildroot/%_menudir/qvlc << EOF
?package(qvlc): command="%_bindir/qvlc" needs="X11" longtitle="VLC is a free MPEG, MPEG2, DVD and DivX player" section="Multimedia/Video" title="VLC Qt media player" icon="qvlc.png" hints="Video" \
mimetypes="video/mpeg,video/msvideo,video/quicktime,video/x-avi,video/x-ms-asf,video/x-ms-wmv,video/x-msvideo,application/x-ogg,application/ogg,audio/x-mp3,audio/x-mpeg,video/x-fli,audio/x-wav"\
accept_url="true"\
multiple_files="true"
EOF
%endif
%if %with_kde
cat > %buildroot/%_menudir/kvlc << EOF
?package(kvlc): command="%_bindir/kvlc" needs="X11" longtitle="VLC is a free MPEG, MPEG2, DVD and DivX player" section="Multimedia/Video" title="VLC KDE media player" icon="kvlc.png" hints="Video" \
mimetypes="video/mpeg,video/msvideo,video/quicktime,video/x-avi,video/x-ms-asf,video/x-ms-wmv,video/x-msvideo,application/x-ogg,application/ogg,audio/x-mp3,audio/x-mpeg,video/x-fli,audio/x-wav"\
accept_url="true"\
multiple_files="true"
EOF
%endif
%if %with_wx
cat > %buildroot/%_menudir/wxvlc << EOF
?package(wxvlc): command="%_bindir/wxvlc" needs="X11" longtitle="VLC is a free MPEG, MPEG2, DVD and DivX player" section="Multimedia/Video" title="VLC wxWindow media player" icon="vlc.png" hints="Video" \
mimetypes="video/mpeg,video/msvideo,video/quicktime,video/x-avi,video/x-ms-asf,video/x-ms-wmv,video/x-msvideo,application/x-ogg,application/ogg,audio/x-mp3,audio/x-mpeg,video/x-fli,audio/x-wav"\
accept_url="true"\
multiple_files="true"
EOF
%endif

# icons
%define pngdir %buildroot/%_datadir/vlc
mkdir -p %{buildroot}/{%{_miconsdir},%{_liconsdir}}
install -m 644 %pngdir/vlc16x16.png %buildroot/%_miconsdir/vlc.png
install -m 644 %pngdir/vlc32x32.png %buildroot/%_iconsdir/vlc.png
install -m 644 %pngdir/vlc48x48.png %buildroot/%_liconsdir/vlc.png
%if %with_gnome
install -m 644 %pngdir/gnome-vlc16x16.png %buildroot/%_miconsdir/gnome-vlc.png
install -m 644 %pngdir/gnome-vlc32x32.png %buildroot/%_iconsdir/gnome-vlc.png
install -m 644 %pngdir/gnome-vlc48x48.png %buildroot/%_liconsdir/gnome-vlc.png
%endif
%if %with_gtk
install -m 644 %pngdir/gvlc16x16.png %buildroot/%_miconsdir/gvlc.png
install -m 644 %pngdir/gvlc32x32.png %buildroot/%_iconsdir/gvlc.png
install -m 644 %pngdir/gvlc48x48.png %buildroot/%_liconsdir/gvlc.png
%endif
%if %with_kde
install -m 644 %pngdir/kvlc16x16.png %buildroot/%_miconsdir/kvlc.png
install -m 644 %pngdir/kvlc32x32.png %buildroot/%_iconsdir/kvlc.png
install -m 644 %pngdir/kvlc48x48.png %buildroot/%_liconsdir/kvlc.png
%endif
%if %with_qt
install -m 644 %pngdir/qvlc16x16.png %buildroot/%_miconsdir/qvlc.png
install -m 644 %pngdir/qvlc32x32.png %buildroot/%_iconsdir/qvlc.png
install -m 644 %pngdir/qvlc48x48.png %buildroot/%_liconsdir/qvlc.png
%endif

%if ! %with_wx
rm -rf %buildroot%_datadir/vlc/skins
%endif

%post
%update_menus
%postun
%clean_menus

%clean
rm -fr %buildroot

%files -f %name.lang
%defattr(-,root,root)
%doc NEWS README COPYING AUTHORS MAINTAINERS THANKS
%doc doc/web-streaming.html doc/vlc-howto.sgml doc/lirc/
%doc doc/fortunes.txt doc/bugreport-howto.txt
%_bindir/vlc
%dir %_datadir/vlc/
%_datadir/vlc/*.*
%dir %_libdir/vlc

%dir %_libdir/vlc/access
%_libdir/vlc/access/libcdda_plugin.so
%_libdir/vlc/access/libaccess_directory_plugin.so
%_libdir/vlc/access/libaccess_file_plugin.so
%_libdir/vlc/access/libaccess_ftp_plugin.so
%_libdir/vlc/access/libaccess_http_plugin.so
%_libdir/vlc/access/libaccess_mms_plugin.so
%_libdir/vlc/access/libaccess_udp_plugin.so
%if %with_dvdplay
%_libdir/vlc/access/libdvdplay_plugin.so
%endif
%_libdir/vlc/access/libdvd_plugin.so
%_libdir/vlc/access/libdvdread_plugin.so
%_libdir/vlc/access/libvcd_plugin.so

%dir %_libdir/vlc/access_output/
%_libdir/vlc/access_output/libaccess_output_dummy_plugin.so
%_libdir/vlc/access_output/libaccess_output_file_plugin.so
%_libdir/vlc/access_output/libaccess_output_http_plugin.so
%_libdir/vlc/access_output/libaccess_output_udp_plugin.so

%dir %_libdir/vlc/audio_filter
%_libdir/vlc/audio_filter/libbandlimited_resampler_plugin.so
%_libdir/vlc/audio_filter/libdolby_surround_decoder_plugin.so
%_libdir/vlc/audio_filter/libdtstospdif_plugin.so
%_libdir/vlc/audio_filter/libfixed32tofloat32_plugin.so
%_libdir/vlc/audio_filter/libfixed32tos16_plugin.so
%_libdir/vlc/audio_filter/libfloat32tos16_plugin.so
%_libdir/vlc/audio_filter/libfloat32tos8_plugin.so
%_libdir/vlc/audio_filter/libfloat32tou16_plugin.so
%_libdir/vlc/audio_filter/libfloat32tou8_plugin.so
%_libdir/vlc/audio_filter/libheadphone_channel_mixer_plugin.so
%_libdir/vlc/audio_filter/liblinear_resampler_plugin.so
%_libdir/vlc/audio_filter/libs16tofixed32_plugin.so
%_libdir/vlc/audio_filter/libs16tofloat32_plugin.so
%_libdir/vlc/audio_filter/libs16tofloat32swab_plugin.so
%_libdir/vlc/audio_filter/libs8tofloat32_plugin.so
%_libdir/vlc/audio_filter/libtrivial_channel_mixer_plugin.so
%_libdir/vlc/audio_filter/libtrivial_resampler_plugin.so
%_libdir/vlc/audio_filter/libu8tofixed32_plugin.so
%_libdir/vlc/audio_filter/libu8tofloat32_plugin.so
%_libdir/vlc/audio_filter/libugly_resampler_plugin.so

%dir %_libdir/vlc/audio_mixer
%_libdir/vlc/audio_mixer/libfloat32_mixer_plugin.so
%_libdir/vlc/audio_mixer/libspdif_mixer_plugin.so
%_libdir/vlc/audio_mixer/libtrivial_mixer_plugin.so

%dir %_libdir/vlc/audio_output
%_libdir/vlc/audio_output/libaout_file_plugin.so
%_libdir/vlc/audio_output/liboss_plugin.so

%dir %_libdir/vlc/codec
%_libdir/vlc/codec/liba52_plugin.so
%_libdir/vlc/codec/libadpcm_plugin.so
%_libdir/vlc/codec/libaraw_plugin.so
%_libdir/vlc/codec/librawvideo_plugin.so
%_libdir/vlc/codec/libcinepak_plugin.so
%_libdir/vlc/codec/libdts_plugin.so
#%_libdir/vlc/codec/libidctclassic_plugin.so
#%ifarch %ix86
#%_libdir/vlc/codec/libidctmmxext_plugin.so
#%_libdir/vlc/codec/libidctmmx_plugin.so
#%endif
#%_libdir/vlc/codec/libidct_plugin.so
%_libdir/vlc/codec/liblpcm_plugin.so
#%ifarch %ix86
#%_libdir/vlc/codec/libmotionmmxext_plugin.so
#%_libdir/vlc/codec/libmotionmmx_plugin.so
#%endif
#%_libdir/vlc/codec/libmotion_plugin.so
%_libdir/vlc/codec/liblibmpeg2_plugin.so
%_libdir/vlc/codec/libmpeg_audio_plugin.so
#%_libdir/vlc/codec/libmpeg_video_plugin.so
%if %with_ffmpeg
#%_libdir/vlc/codec/libpostprocessing_c_plugin.so
	%ifarch %ix86
#	%_libdir/vlc/codec/libpostprocessing_mmx_plugin.so
#	%_libdir/vlc/codec/libpostprocessing_mmxext_plugin.so
	%endif
%endif
%_libdir/vlc/codec/libspudec_plugin.so

%dir %_libdir/vlc/control
%_libdir/vlc/control/libhttp_plugin.so
%_libdir/vlc/control/librc_plugin.so
%_libdir/vlc/control/libgestures_plugin.so

%dir %_libdir/vlc/demux
%_libdir/vlc/demux/libaac_plugin.so
%_libdir/vlc/demux/libasf_plugin.so
%_libdir/vlc/demux/libau_plugin.so
%_libdir/vlc/demux/libaudio_plugin.so
%_libdir/vlc/demux/libavi_plugin.so
%_libdir/vlc/demux/liba52sys_plugin.so
%_libdir/vlc/demux/libdemuxdump_plugin.so
%_libdir/vlc/demux/libdemuxsub_plugin.so
%_libdir/vlc/demux/libes_plugin.so
%_libdir/vlc/demux/libid3_plugin.so
%_libdir/vlc/demux/libm3u_plugin.so
%_libdir/vlc/demux/libm4v_plugin.so
%if %with_mkv
%_libdir/vlc/demux/libmkv_plugin.so
%endif
%_libdir/vlc/demux/libmp4_plugin.so
%_libdir/vlc/demux/libmpeg_system_plugin.so
%_libdir/vlc/demux/libps_plugin.so
%_libdir/vlc/demux/librawdv_plugin.so
%_libdir/vlc/demux/libts_plugin.so
%_libdir/vlc/demux/libwav_plugin.so

%dir %_libdir/vlc/misc
%_libdir/vlc/misc/libdummy_plugin.so
%_libdir/vlc/misc/libhttpd_plugin.so
%_libdir/vlc/misc/libipv4_plugin.so
%_libdir/vlc/misc/libipv6_plugin.so
%_libdir/vlc/misc/liblogger_plugin.so
#%ifarch %ix86
#%_libdir/vlc/misc/libmemcpy3dn_plugin.so
#%_libdir/vlc/misc/libmemcpymmxext_plugin.so
#%_libdir/vlc/misc/libmemcpymmx_plugin.so
#%endif
%_libdir/vlc/misc/libmemcpy_plugin.so
%_libdir/vlc/misc/libsap_plugin.so
%_libdir/vlc/misc/libscreensaver_plugin.so

%dir %_libdir/vlc/mux
%_libdir/vlc/mux/libmux_avi_plugin.so
%_libdir/vlc/mux/libmux_dummy_plugin.so
%_libdir/vlc/mux/libmux_ogg_plugin.so
%_libdir/vlc/mux/libmux_ps_plugin.so
%_libdir/vlc/mux/libmux_ts_plugin.so

%dir %_libdir/vlc/packetizer
%_libdir/vlc/packetizer/libpacketizer_a52_plugin.so
%_libdir/vlc/packetizer/libpacketizer_copy_plugin.so
%_libdir/vlc/packetizer/libpacketizer_mpeg4audio_plugin.so
%_libdir/vlc/packetizer/libpacketizer_mpeg4video_plugin.so
%_libdir/vlc/packetizer/libpacketizer_mpegaudio_plugin.so
%_libdir/vlc/packetizer/libpacketizer_mpegvideo_plugin.so

%dir %_libdir/vlc/stream_out
%_libdir/vlc/stream_out/libstream_out_display_plugin.so
%_libdir/vlc/stream_out/libstream_out_dummy_plugin.so
%_libdir/vlc/stream_out/libstream_out_duplicate_plugin.so
%_libdir/vlc/stream_out/libstream_out_es_plugin.so
%_libdir/vlc/stream_out/libstream_out_standard_plugin.so

%dir %_libdir/vlc/video_chroma
%_libdir/vlc/video_chroma/libi420_rgb_plugin.so
%_libdir/vlc/video_chroma/libi420_ymga_plugin.so
%_libdir/vlc/video_chroma/libi420_yuy2_plugin.so
%_libdir/vlc/video_chroma/libi422_yuy2_plugin.so
#%ifarch %ix86
#%_libdir/vlc/video_chroma/libi420_rgb_mmx_plugin.so
#%_libdir/vlc/video_chroma/libi420_ymga_mmx_plugin.so
#%_libdir/vlc/video_chroma/libi420_yuy2_mmx_plugin.so
#%_libdir/vlc/video_chroma/libi422_yuy2_mmx_plugin.so
#%endif

%dir %_libdir/vlc/video_filter
%_libdir/vlc/video_filter/libadjust_plugin.so
%_libdir/vlc/video_filter/libclone_plugin.so
%_libdir/vlc/video_filter/libcrop_plugin.so
%_libdir/vlc/video_filter/libdeinterlace_plugin.so
%_libdir/vlc/video_filter/libdistort_plugin.so
%_libdir/vlc/video_filter/libinvert_plugin.so
%_libdir/vlc/video_filter/libmotionblur_plugin.so
%_libdir/vlc/video_filter/libosdtext_plugin.so
%_libdir/vlc/video_filter/libtransform_plugin.so
%_libdir/vlc/video_filter/libwall_plugin.so

%dir %_libdir/vlc/video_output
%_libdir/vlc/video_output/libfb_plugin.so
#%_libdir/vlc/video_output/libvout_encoder_plugin.so
%_libdir/vlc/video_output/libx11_plugin.so
%_libdir/vlc/video_output/libxvideo_plugin.so

%dir %_libdir/vlc/visualization

%_mandir/man1/vlc.*
%_menudir/vlc
%_miconsdir/vlc.png
%_iconsdir/vlc.png
%_liconsdir/vlc.png

%files -n %libname-devel
%defattr(-,root,root)
%doc README doc/release-howto.txt doc/skins doc/subtitles doc/Configure.help
%doc doc/arm-crosscompile-howto.sgml
%dir %_includedir/vlc
%_includedir/vlc/*
%_libdir/*a
%_libdir/vlc/*a
%_bindir/vlc-config
%_mandir/man1/vlc-config*

%if %with_mozilla
%files -n mozilla-plugin-vlc
%defattr(-,root,root)
%doc README
%_libdir/mozilla/*/*
%endif

# intf plugins
%if %with_gtk
%files -n gvlc
%defattr(-,root,root)
%doc README
%_libdir/vlc/misc/libgtk_main_plugin.so
%_libdir/vlc/gui/libgtk_plugin.so
%_bindir/gvlc
%_menudir/gvlc
%_miconsdir/gvlc.png
%_iconsdir/gvlc.png
%_liconsdir/gvlc.png
%post -n gvlc
%update_menus
%postun -n gvlc
%clean_menus
%endif

%if %with_gnome
%files -n gnome-vlc
%defattr(-,root,root)
%doc README
%_libdir/vlc/misc/libgnome_main_plugin.so
%_libdir/vlc/gui/libgnome_plugin.so
%_bindir/gnome-vlc
%_menudir/gnome-vlc
%_miconsdir/gnome-vlc.png
%_iconsdir/gnome-vlc.png
%_liconsdir/gnome-vlc.png
%post   -n gnome-vlc
%update_menus
%postun -n gnome-vlc
%clean_menus
%endif

%if %with_wx
%files -n wxvlc
%defattr(-,root,root)
%doc README
%_bindir/wxvlc
%_libdir/vlc/gui/libwxwindows_plugin.so
%_menudir/wxvlc
%_datadir/vlc/skins
%post   -n wxvlc
%update_menus
%postun -n wxvlc
%clean_menus
%endif


%if %with_qt
%files -n qvlc
%defattr(-,root,root)
%doc README
%_libdir/vlc/gui/libqt_plugin.so
%_bindir/qvlc
%_menudir/qvlc
%_miconsdir/qvlc.png
%_iconsdir/qvlc.png
%_liconsdir/qvlc.png
%post   -n qvlc
%update_menus
%postun -n qvlc
%clean_menus
%endif

%if %with_kde
%files -n kvlc
%defattr(-,root,root)
%doc README
%_libdir/vlc/gui/libkde_plugin.so
%_bindir/kvlc
%_menudir/kvlc
%_miconsdir/kvlc.png
%_iconsdir/kvlc.png
%_liconsdir/kvlc.png
%post   -n kvlc
%update_menus
%postun -n kvlc
%clean_menus
%endif

%if %with_ncurses
%files plugin-ncurses
%defattr(-,root,root)
%doc README
%_libdir/vlc/gui/libncurses_plugin.so
%endif

%if %with_lirc
%files plugin-lirc
%defattr(-,root,root)
%doc README
%_libdir/vlc/control/liblirc_plugin.so
%endif

# video plugins
%if %with_sdl
%files plugin-sdl
%defattr(-,root,root)
%doc README
%_libdir/vlc/audio_output/libaout_sdl_plugin.so
%_libdir/vlc/video_output/libvout_sdl_plugin.so
%endif

%if %with_ggi
%files plugin-ggi
%defattr(-,root,root)
%doc README
%{_libdir}/vlc/video_output/libggi_plugin.so
%endif

%if %with_aa
%files plugin-aa
%defattr(-,root,root)
%doc README
%{_libdir}/vlc/video_output/libaa_plugin.so
%endif

%if %with_svgalib
%files plugin-svgalib
%defattr(-,root,root)
%doc README
%_libdir/vlc/video_output/libsvgalib_plugin.so
%endif

# visualization plugin
%if %with_xosd
%files plugin-xosd
%defattr(-,root,root)
%doc README
%_libdir/vlc/visualization/libxosd_plugin.so
%endif

# codec plugin
%if %with_mad
%files plugin-mad
%defattr(-,root,root)
%doc README
%_libdir/vlc/audio_filter/libmpgatofixed32_plugin.so
%_libdir/vlc/demux/libid3tag_plugin.so
%endif

%if %with_ogg
%files plugin-ogg
%defattr(-,root,root)
%doc README
%_libdir/vlc/demux/libogg_plugin.so
%_libdir/vlc/codec/libvorbis_plugin.so
%endif

%if %with_ogg
%files plugin-flac
%defattr(-,root,root)
%doc README
%_libdir/vlc/demux/libflac_plugin.so
%_libdir/vlc/codec/libflacdec_plugin.so
%endif


%if %with_dv
%files plugin-dv
%defattr(-,root,root)
%doc README
%_libdir/vlc/codec/libdv_plugin.so
%endif

%if %with_a52
%files plugin-a52
%defattr(-,root,root)
%doc README
%_libdir/vlc/audio_filter/liba52tofloat32_plugin.so
%_libdir/vlc/audio_filter/liba52tospdif_plugin.so
%endif

# input plugin
%if %with_dvb
%files plugin-dvb
%defattr(-,root,root)
%doc README
%_libdir/vlc/access/libsatellite_plugin.so
%_libdir/vlc/demux/libts_dvbpsi_plugin.so
%_libdir/vlc/mux/libmux_ts_dvbpsi_plugin.so
%endif

#audio plugins
%if %with_esd
%files plugin-esd
%defattr(-,root,root)
%doc README
%_libdir/vlc/audio_output/libesd_plugin.so
%endif

%if %with_arts
%files plugin-arts
%defattr(-,root,root)
%doc README
%_libdir/vlc/audio_output/libarts_plugin.so
%endif

%if %with_alsa
%files plugin-alsa
%defattr(-,root,root)
%doc README
%_libdir/vlc/audio_output/libalsa_plugin.so
%endif

%if %with_slp
%files plugin-slp
%defattr(-,root,root)
%doc README
%_libdir/vlc/access/libslp_plugin.so
%endif

%changelog
* Mon Jun 30 2003 Götz Waschk <waschk@linux-mandrake.com> 0.6.0-5mdk
- reenable wxvlc, but use 2.4 instead of 2.5
- remove the packager tag
- fix build on mdk 9.1

* Mon Jun 30 2003 Götz Waschk <waschk@linux-mandrake.com> 0.6.0-4mdk
- add some more docs
- enable libtar
- fix comment about the DVB headers
- small spec fix

* Mon Jun 30 2003 Götz Waschk <waschk@linux-mandrake.com> 0.6.0-3mdk
- some spec fixes
- add mime types to the menu entries
- enable the kde plugin
- the wx package provides wx-gui
- move the skins to the wx package
- fix all --with options
- enable matroska
- disable wx, didn't work

* Fri Jun 27 2003 Götz Waschk <waschk@linux-mandrake.com> 0.6.0-2mdk
- add matroska to the spec file, but don't enable it yet
- enable the flac plugin
- enable DVB, use headers from kernel-multimedia-source

* Fri Jun 27 2003 Götz Waschk <waschk@linux-mandrake.com> 0.6.0-1mdk
- add lots of new plugins
- disable libvout_encoder_plugin.so
- disable postprocessing plugins
- add the data dir to the main package
- disable dvb (were have all the headers gone?)
- include static ffmpeg
- include static mpeg2dec
- enable wxGTK
- fix mozilla build
- fix buildrequires
- new version

* Tue Apr  8 2003 Götz Waschk <waschk@linux-mandrake.com> 0.5.2-2mdk
- new dvdread

* Sun Apr 06 2003 Yves Duret <yves@zarb.org> 0.5.2-1mdk
- 0.5.2

* Tue Feb 18 2003 Götz Waschk <waschk@linux-mandrake.com> 0.5.0-4mdk
- new xosd

* Thu Feb 06 2003 Olivier Thauvin <thauvin@aerov.jussieu.fr> 0.5.0-3mdk
- BuildRequires libdvbpsi-devel libdvdplay-devel

* Tue Feb  4 2003 Götz Waschk <waschk@linux-mandrake.com> 0.5.0-2mdk
- rebuild for new xosd

* Mon Feb 03 2003 Yves Duret <yves@zarb.org> 0.5.0-1mdk
- Natalya release.
- enables DVD menus.

* Mon Feb 03 2003 Yves Duret <yves@zarb.org> 0.5.0-0.20030203.1mdk
- latest (!) cvs snapshot before release, oh yeah.
- added dv and dvb sub rpm (satellite).
- added mozilla-plugin.
- more docs.
- fixes here and here.
- sync with CVS one.

* Fri Jan 31 2003 Yves Duret <yves@zarb.org> 0.5.0-0.20030131.1mdk
- new cvs snapshot.

* Tue Jan 28 2003 Yves Duret <yves@zarb.org> 0.5.0-0.20030128.1mdk
- new cvs snapshot.
- sync specfile with HEAD CVS one.

* Mon Jan 27 2003 Yves Duret <yves@zarb.org> 0.5.0-0.20030127.1mdk
- new cvs snapshot.

* Fri Jan 24 2003 Yves Duret <yves@zarb.org> 0.5.0-0.20030124.1mdk
- new cvs snapshot adding transcoding feature!
- new video_output/vout_encoder plugin.
- new demux/a52sys plugin.

* Mon Jan 20 2003 Yves Duret <yves@zarb.org> 0.5.0-0.20030120.1mdk
- new cvs snapshot.
- new access/slp plugin.
- more buildfor_{rh80,mdk{82,90}} stuff.
- use %%mklibname macro.
- use %%clean_menus in postun instead of %%update_menus everywhere.

* Thu Jan 16 2003 Yves Duret <yves@zarb.org> 0.5.0-0.20030116.1mdk
- new cvs snapshot.
- codec/mad plugin is replaced by audio_filter/mpgatofixed32.

* Tue Jan 14 2003 Yves Duret <yves@zarb.org> 0.5.0-0.20030114.1mdk
- new cvs snapshot.
- new demux/m4v and mux/avi plugins.

* Fri Jan 10 2003 Yves Duret <yves@zarb.org> 0.5.0-0.20030110.1mdk
- new cvs snapshot.
- new packetizer/mpeg4audio plugin.

* Tue Jan 07 2003 Yves Duret <yves@zarb.org> 0.5.0-0.20030107.1mdk
- cvs 2003/01/07.
- mmx/3dn plugins are only for x86 arch (use %ifarch %ix86 to list them)
  ie. fix rpm building on ppc thx Olivier Thauvin <olivier.thauvin@aerov.jussieu.fr>

* Mon Jan 06 2003 Yves Duret <yves@zarb.org> 0.5.0-0.20030106.1mdk
- cvs 2003/01/06.
- removed access/rtp plugin (merged in access/udp).
- removed codec/imdct and codec/downmix plugins (deprecated).
- libvlc0-devel provides libvlc-devel.

* Sun Jan 05 2003 Yves Duret <yves@zarb.org> 0.5.0-0.20021220.2mdk
- rebuild against new glibc.
- rpm configure macro is now fixed.

* Fri Dec 20 2002 Yves Duret <yves@zarb.org> 0.5.0-0.20021220.1mdk
- cvs 20021220 (aka fix segfaulting with broken trancoded avi)
- added rawdv plugin.
- few spec enhacement and sync with upstream CVS.

* Wed Dec 18 2002 Olivier Thauvin <thauvin@aerov.jussieu.fr> 0.5.0-0.20021218.1mdk
- don't harcore arch in name
- cvs 20021218


* Mon Jun 20 2002 Yves Duret <yduret@mandrakesoft.com> 0.4.2-1mdk
- new upstream release

* Mon Jun 3 2002 Yves Duret <yduret@mandrakesoft.com> 0.4.1-1mdk
- new upstream release

* Thu May 23 2002 Yves Duret <yduret@mandrakesoft.com> 0.4.0-1mdk
- version 0.4.0 with MPEG4 (DivX) support thx ffmpeg.
  thus s/MPEG, MPEG2 and DVD/multimedia/g
- sync %%description with debian ones.
- vlc now requires a vlc-gui (gtk, gnome or qt).
- removed gcc3.1 patches since merged upstream.

* Mon May 13 2002 Yves Duret <yduret@mandrakesoft.com> 0.3.1-4mdk
- removed xmga plugin (currently broken).
- manual rebuild in gcc3.1 environment aka added Patch0 & Patch1
- various summary/description changes.

* Fri May 03 2002 Yves Duret <yduret@mandrakesoft.com> 0.3.1-3mdk
- added vlc-lirc intf plugin rpm.

* Tue Apr 30 2002 Yves Duret <yduret@mandrakesoft.com> 0.3.1-2mdk
- rebuild against libalsa2 (vlc-sdl)

* Fri Apr 19 2002 Yves Duret <yduret@mandrakesoft.com> 0.3.1-1mdk
- version 0.3.1.
- removed patch0 merged upstream.
- removed old %%ifarch ppc
- added missing libmad-devel buldrequires

* Wed Apr 17 2002 Yves Duret <yduret@mandrakesoft.com> 0.3.0-4mdk
- added liba52 support (buildrequires).
- added vlc-alsa audio plugin.
- mad is a codec (audio) plugin. corrected description and summary.

* Wed Apr 10 2002 Yves Duret <yduret@mandrakesoft.com> 0.3.0-3mdk
- added patch0 from CVS: fix crashing GTK popup menus thx Michal Bukovjan <bukovjan@mbox.dkm.cz>

* Wed Apr 10 2002 Yves Duret <yduret@mandrakesoft.com> 0.3.0-2mdk
- added vlc-arts rpm plugin thx blindauer Emmanuel <manu@agat.net>
- better summary for plugin
- add packager tag to myself

* Sun Apr 07 2002 Yves Duret <yduret@mandrakesoft.com> 0.3.0-1mdk
- version 0.3.0
- added aa (Asci Art) plugin in vlc-aa rpm
- merged with sam's one:
  * using his plugins list into %%files
  * removed libdvdcss from the whole tarball.
  * removed the workaround for vlc's bad /dev/dsp detection.
- few spell corrections in all %%description
- added buildrequires on SDL-devel

* Tue Mar 05 2002 Yves Duret <yduret@mandrakesoft.com> 0.2.93-0.1mdk
- new cvs snapshot
- fix requires

* Mon Mar 04 2002 Yves Duret <yduret@mandrakesoft.com> 0.2.92-5mdk
- cvs snapshot

* Sat Jan 26 2002 Yves Duret <yduret@mandrakesoft.com> 0.2.92-4mdk
- mad plugin in vlc-mad rpm

* Mon Jan 21 2002 Yves Duret <yduret@mandrakesoft.com> 0.2.92-3mdk
- synced with main cvs specfile wich "fixed a few minor inaccuracies"

* Thu Jan 17 2002 Yves Duret <yduret@mandrakesoft.com> 0.2.92-2mdk
- readded libdvdcss rpm in specfile. use %%define css 1 with correct sources
  to build libdvdcss rpm.

* Wed Jan 09 2002 Yves Duret <yduret@mandrakesoft.com> 0.2.92-1mdk
- version 0.2.92
- %%makeinstall_std
- splitted again, added vlc-sdl vlc-esd vlc-ggi
- bring back some missing plugins
- fixed buildrequires
- added menu entries and icons (from cvs)

* Tue Oct 23 2001 Yves Duret <yduret@mandrakesoft.com> 0.2.83-2mdk
- rebuild against libpng3
- added some doc for sir rpmlint
- #5583: option -g

* Thu Aug 23 2001 Yves Duret <yduret@mandrakesoft.com> 0.2.83-1mdk
- version 0.2.83 : 
  * Activated subtitles in overlay mode (far from perfect, but this
    was an often requested feature).

* Fri Aug 10 2001 Yves Duret <yduret@mandrakesoft.com> 0.2.82-1mdk
- version 0.2.82

* Mon Jul 30 2001 Yves Duret <yduret@mandrakesoft.com> 0.2.81-1mdk
- version 0.2.81
- added vlc-ncurses

* Wed Jun 20 2001 Yves Duret <yduret@mandrakesoft.com> 0.2.81-0.20010619-1mdk
- cvs snapshot
- added libdvdcss

* Wed Jun 13 2001 Yves Duret <yduret@mandrakesoft.com> 0.2.80-2mdk
- fix build on ppc (c) dadou

* Mon Jun 11 2001 Yves Duret <yduret@mandrakesoft.com> 0.2.80-1mdk
- version 0.2.80 : bug fixes and bug fixes and bug fixes and small
  improvements of the gtk interface.
- corrected Summary in vlc-qt

* Wed May 23 2001 Yves Duret <yduret@mandrakesoft.com> 0.2.73-2mdk
- added qt2 plugin (vlc-qt)

* Wed May 16 2001 Yves Duret <yduret@mandrakesoft.com> 0.2.73-1mdk
- version 0.2.73
- you can now get decss threw a plugin
- rewritte srcipt to build vlc (decss plugin)
- rebuild with SDL 1.2

* Thu Apr 26 2001 Yves Duret <yduret@mandrakesoft.com> 0.2.72-2mdk
- true 0.2.72

* Mon Apr 16 2001 Yves Duret <yduret@mandrakesoft.com> 0.2.72-1mdk
- version 0.2.72
- package split into vlc, vlc-gnome, vlc-gtk

* Fri Apr 13 2001 Yves Duret <yduret@mandrakesoft.com> 0.2.71-1mdk
- version 0.2.71 :
  * Fixed segfaults when compiled with gcc 3.0pre and versions of gcc
    shipped with the latest RedHat distributions.
  * Fixed the BeOS CSS decryption.
  * Fixed a few issues in IFO parsing.
  * Fixed XVideo video output.
  * Updated icons under Linux, BeOS, MacOS X.

* Wed Apr 11 2001 Yves Duret <yduret@mandrakesoft.com> 0.2.70-1mdk
- version 0.2.70

* Thu Mar 22 2001 Yves Duret <yduret@mandrakesoft.com> 0.2.63-1mdk
- version 0.2.63 : Bugfixes, bugfixes, and bugfixes again, a Gtk+
  interface for the Gnome-impaired, an even better DVD support

* Fri Feb 16 2001 Yves Duret <yduret@mandrakesoft.com> 0.2.61-1mdk
- new version for all the DVD fans (add MPEG1 support, direct DVD support,
  DVD decryption, arbitrary, seeking in the stream, pause, fast forward
  and slow motion, hardware YUV acceleration enhanced CSS support and a few
  new interface features including drag'n'drop.
- first *real* public release (now under the GPL)

* Sat Jan 06 2001 David BAUDENS <baudens@mandrakesoft.com> 0.1.99i-2mdk
- Fix build and use right optimizations on PPC
- Enable SDL support
- Spec clean up

* Fri Jan  5 2001 Guillaume Cottenceau <gc@mandrakesoft.com> 0.1.99i-1mdk
- 0.1.99i, rebuild

* Fri Aug 25 2000 Guillaume Cottenceau <gc@mandrakesoft.com> 0.1.99h-1mdk
- 0.1.99h

* Mon Jul 10 2000 Guillaume Cottenceau <gc@mandrakesoft.com> 0.1.99c-1mdk
- first Mandrake package with help of Sam
