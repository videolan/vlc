%define name 		vlc
%define version 	0.5.0
%define rel		0.1

%define	libmajor	0
%define libname		lib%name%libmajor

%define cvs     	1
%if %{cvs}
%define	cvsrel		1
%define cvsdate 	20021220
%define release		0.%{cvsdate}.%{cvsrel}mdk
%define cvs_name 	%{name}-snapshot-%cvsdate
%else
%define release 	%{rel}mdk
%endif

%define with_dvdplay 0

%define with_mozilla 0
%define with_gtk 1
%define with_gnome 1
%define with_qt 0
%define with_kde 0
%define with_ncurses 1
%define with_lirc 1
%define	with_wx 0

%define with_aa 1
%define with_sdl 1
%define with_ggi 1
%define with_svgalib 0
%define with_xosd 1

%define with_mad 1
%define with_ogg 1
%define with_a52 1
%define with_dv 0
%define with_dvb 0
%define	with_ffmpeg 1

%define with_esd 1
%define with_arts 1
%define with_alsa 1

%define redhat80 0
%if %redhat80
%define release %rel
# adjust define for Redhat.
%endif

# without
%{?_without_mozilla:	%{expand: %%define with_mozilla 0}}
%{?_without_gtk:	%{expand: %%define with_gtk 0}}
%{?_without_gnome:	%{expand: %%define with_gnome 0}}
%{?_without_qt:		%{expand: %%define with_qt 0}}
%{?_without_kde:	%{expand: %%define with_kde 0}}
%{?_without_ncurses:	%{expand: %%define with_ncurses 0}}
%{?_without_lirc:	%{expand: %%define with_lirc 0}}
%{?_without_wx:		%{expand: %%define with_wx 0}}

%{?_without_aa:   	%{expand: %%define with_aa 0}}
%{?_without_sdl:   	%{expand: %%define with_sdl 0}}
%{?_without_ggi:   	%{expand: %%define with_ggi 0}}
%{?_without_svgalib:	%{expand: %%define with_svgalib 0}}
%{?_without_xosd:	%{expand: %%define with_xosd 0}}

%{?_without_mad:	%{expand: %%define with_mad 0}}
%{?_without_ogg:	%{expand: %%define with_ogg 0}}
%{?_without_a52:	%{expand: %%define with_a52 0}}
%{?_without_dv:		%{expand: %%define with_dv 0}}
%{?_without_dvb:	%{expand: %%define with_dvb 0}}

%{?_without_esd:	%{expand: %%define with_esd 0}}
%{?_without_arts:	%{expand: %%define with_arts 0}}
%{?_without_alsa:	%{expand: %%define with_alsa 0}}

# with
%{?_with_mozilla:    	%{expand: %%define with_mozilla 1}}
%{?_with_gtk:		%{expand: %%define with_gtk 1}}
%{?_with_gnome:		%{expand: %%define with_gnome 1}}
%{?_with_qt:		%{expand: %%define with_qt 1}}
%{?_with_kde:		%{expand: %%define with_kde 1}}
%{?_with_ncurses:    	%{expand: %%define with_ncurses 1}}
%{?_with_lirc:       	%{expand: %%define with_lirc 1}}
%{?_with_wx:		%{expand: %%define with_wx 0}}

%{?_with_aa:         	%{expand: %%define with_aa 1}}
%{?_with_sdl:        	%{expand: %%define with_sdl 1}}
%{?_with_ggi:        	%{expand: %%define with_ggi 1}}
%{?_with_svgalib:    	%{expand: %%define with_svgalib 1}}
%{?_with_xosd:       	%{expand: %%define with_xosd 1}}

%{?_with_mad:		%{expand: %%define with_mad 1}}
%{?_with_ogg:        	%{expand: %%define with_ogg 1}}
%{?_with_a52:        	%{expand: %%define with_a52 1}}
%{?_with_dv:         	%{expand: %%define with_dv 1}}
%{?_with_dvb:        	%{expand: %%define with_dvb 1}}

%{?_with_esd:        	%{expand: %%define with_esd 1}}
%{?_with_arts:       	%{expand: %%define with_arts 1}}
%{?_with_alsa:       	%{expand: %%define with_alsa 1}}


Summary:	VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.
Name:		%{name}
Version:	%{version}
Release:	%{release}
Packager:	Yves Duret <yves@zarb.org>

%if %{cvs} 
Source0:	http://www.videolan.org/pub/videolan/vlc/snapshots/%{cvs_name}.tar.bz2
%else
Source0:	http://www.videolan.org/packages/%{version}/%{name}-%{version}.tar.bz2
%endif
License:	GPL
Group:		Video
URL:		http://www.videolan.org/
Requires:	vlc-gui
# vlc-mad needed by ffmpeg builtin (i want MPEG4 support out of box)
Requires:	vlc-plugin-mad
#DVD working out of box.
Requires:	vlc-plugin-a52

BuildRoot:	%_tmppath/%name-%version-%release-root

%if %with_mozilla
Buildrequires:	mozilla-devel
%endif
%if %with_gtk
Buildrequires:	libgtk+1.2-devel
%endif
%if %with_gnome
Buildrequires:	gnome-libs-devel
%endif
%if %with_qt
Buildrequires:	libqt2-devel
%endif
%if %with_kde
Buildrequires:	libkde2-devel
%endif
%if %with_ncurses
Buildrequires:	libncurses5-devel
%if %with_wx
Buildrequires:	wxwindows
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
Buildrequires:	libxosd2-devel
%endif
%if %with_mad
Buildrequires:	libmad-devel
%endif
%if %with_ogg
Buildrequires:	libvorbis-devel
Buildrequires:	libogg-devel
%endif
%if %with_dv
Buildrequires:	libdv2-devel
%endif

%if %with_a52
#Buildrequires:	liba52dec-devel
%endif

%if %with_ffmpeg
Buildrequires:	libffmpeg-devel
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

%description
VideoLAN is an OpenSource streaming solution for every OS developed by
students from the Ecole Centrale Paris and developers from all over the
World.
The VideoLAN Client (vlc) plays MPEG1, MPEG2 and MPEG4 (aka DivX) files,
DVDs, VCDs, SVCDs, from a satellite card, from an MPEG2 Transport
Streams sent by the VideoLAN Server (vls) or from a Web server (with the
HTTP input).
You may install vlc-gnome or vlc-gtk to have a nice graphical interface.
This package contains no CSS unscrambling functionality for DVDs ;
you need the libdvdcss library available from 
http://www.videolan.org/libdvdcss/ or http://plf.zarb.org/

#general packages
%package -n %libname-devel
Summary: Development files for the VideoLAN Client
Group: Development/C
Requires: %name = %version-%release
Provides: %name-devel = %version-%release
Provides: lib%name-devel = %version-%release
%description -n %libname-devel
Development files for the VideoLAN Client
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This package contains headers and a static library required to build plugins
for the VideoLAN Client, or standalone applications using VideoLAN Client.

%package -n mozilla-plugin-vlc
Summary: A multimedia plugin for Mozilla, based on vlc
group: Video
Requires: %name = %version-%release
%description -n mozilla-plugin-vlc
This plugin adds support for MPEG, MPEG2, DVD and DivX to your Mozilla
browser. The decoding process is done by vlc and the output window is
embedded in a webpage or directly in the browser window. There is also
support for fullscreen display.


# intf plugins
%package -n gvlc
Summary: Gtk plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
Provides: vlc-gui
%description -n gvlc
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This plugin adds a Gtk+ interface to vlc, the VideoLAN Client. To
activate it, use the `--intf gtk' flag or run the `gvlc' program.

%package -n gnome-vlc
Summary: Gnome plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
Provides: vlc-gui, vlc-gnome
Obsoletes: vlc-gnome
%description -n gnome-vlc
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This plugin adds a Gnome interface to vlc, the VideoLAN Client. To
activate it, use the `--intf gnome' flag or run the `gnome-vlc' program.

%package -n qvlc
Summary: Qt2 plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
Provides: vlc-gui, vlc-qt
Obsoletes: vlc-qt
%description -n qvlc
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This plugin adds a Qt interface to vlc, the VideoLAN Client. To
activate it, use the `--intf qt' flag or run the `qvlc' program.

%package -n kvlc
Summary: KDE frontend for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
Provides: vlc-gui
%description -n kvlc
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This plugin adds a KDE interface to vlc, the VideoLAN Client. To
activate it, use the `--intf kde' flag or run the `kvlc' program.


%package plugin-ncurses
Summary: Ncurses console-based plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
%description plugin-ncurses
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This plugin adds a ncurses interface to vlc, the VideoLAN Client. To
activate it, use the `--intf ncurses' flag.

%package plugin-lirc
Summary: Lirc plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
Obsoletes: vlc-lirc
Provides: vlc-lirc
%description plugin-lirc
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This plugin is an infrared lirc interface for vlc, the
VideoLAN Client. To activate it, use the `--intf lirc' flag.

#
# video plugins
%package plugin-aa
Summary: ASCII art video plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
Obsoletes: vlc-aa
Provides: vlc-aa
%description plugin-aa
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This is an ASCII art video output plugin for vlc, the VideoLAN
Client. To activate it, use the `--vout aa' flag or select the `aa'
vout plugin from the preferences menu.


%package plugin-sdl
Summary: Simple DirectMedia Layer video plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
Obsoletes: vlc-sdl
Provides: vlc-sdl
%description plugin-sdl
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This plugin adds support for the Simple DirectMedia Layer library to
vlc, the VideoLAN Client. To activate it, use the `--vout sdl' or
`--aout sdl' flags or select the `sdl' vout or aout plugin from the
preferences menu.

%package plugin-ggi
Summary: GGI video plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
Obsoletes: vlc-ggi
Provides: vlc-ggi
%description plugin-ggi
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This is a GGI plugin for vlc, the VideoLAN Client.  To activate it, use
the `--vout ggi' flag or select the `ggi' vout plugin from the preferences
menu.

%package plugin-svgalib
Summary: SVGAlib video plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
%description plugin-svgalib
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This plugin adds support for SVGAlib to vlc, the VideoLAN Client. To
activate it, use the `--vout svgalib' flag or select the `svgalib' video
output plugin from the preferences menu. Note that you will need root
permissions to use SVGAlib.


#
# visualization plugins
%package plugin-xosd
Summary: X On-Screen Display plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
%description plugin-xosd
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This is an On-Screen Display plugin for vlc, the VideoLAN Client. To
activate it, use the `--intf xosd' flag or select the `xosd' interface
plugin from the preferences menu.

# codec plugins
%package plugin-mad
Summary: MAD audio codec plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
Obsoletes: vlc-mad
Provides: vlc-mad
%description plugin-mad
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This plugin adds support for libmad, the MPEG audio decoder library,
to the VideoLAN Client. MAD is 100% fixed-point based. To activate
this plugin, use the `--mpeg_adec mad' flag or select the `mad' MPEG
decoder from the preferences menu.

%package plugin-ogg
Summary: Ogg demuxer and Vorbis codec plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
%description plugin-ogg
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

These plugins add support for the Ogg bitstream format and the Ogg Vorbis
compressed audio format to vlc, the VideoLAN Client. They are autodetected.

%package plugin-a52
Summary: A-52 (AC-3) codec plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
%description plugin-a52
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This plugin adds support for the ATSC A-52 (aka. AC-3) audio format to
vlc, the VideoLAN Client. The plugin is autodetected.

%package plugin-dv
Summary: DV codec plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
%description plugin-dv
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This plugin adds support for the DV video format to vlc, the VideoLAN
Client. The plugin is autodetected.

#
# input plugins
%package plugin-dvb
Summary: DVB input plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
%description plugin-dvb
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This plugin adds support for DVB cards to vlc, the VideoLAN Client. Note
that your card needs to be supported by your kernel before vlc can use it.

#
# audio plugins
%package plugin-esd
Summary: Enlightened Sound Daemon audio plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
Obsoletes: vlc-esd
Provides: vlc-esd
%description plugin-esd
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This plugin adds support for the Enlightened Sound Daemon to vlc, the
VideoLAN Client. To activate it, use the `--aout esd' flag or select
the `esd' aout plugin from the preferences menu.

%package plugin-arts
Summary: aRts audio plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
Obsoletes: vlc-arts
Provides: vlc-arts
%description plugin-arts
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This plugin adds support for the aRts Sound System to vlc, the
VideoLAN Client. To activate it, use the `--aout arts' flag or
select the `arts' aout plugin from the preferences menu.

%package plugin-alsa
Summary: Advanced Linux Sound Architecture audio plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
Obsoletes: vlc-alsa
Provides: vlc-alsa
%description plugin-alsa
VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution.

This plugin adds support for the Advanced Linux Sound Architecture to
vlc, the VideoLAN Client. To activate it, use the `--aout alsa' flag or
select the `alsa' aout plugin from the preferences menu.

%prep
%if %{cvs}
%setup -q -n %{cvs_name}
%else
%setup -q 
%endif

%build
# yves 0.4.0-1mdk
# ffmpeg: static linking cause no official ffmpeg release aith a stable ABI
# ffmpeg:no plugin posible on ia64 due to the static linking (can not put .a in a .so)

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
        --enable-ffmpeg --with-ffmpeg=/usr --with-ffmpeg-tree=/usr/lib \
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

# debian configure
# --enable-a52 --enable-aa --enable-dvbpsi --enable-xosd --enable-mozilla --enable-kde --enable-mp4 --enable-dvb --enable-dv --enable-svgalib --enable-satellite --enable-ogg --enable-vorbis

export QTDIR=%{_libdir}/qt3 
%make

%install
%makeinstall_std
%find_lang %name
install -d %buildroot/%_mandir/man1
install doc/vlc.1 %buildroot/%_mandir/man1
install doc/vlc-config.1 %buildroot/%_mandir/man1

# menu
mkdir -p %buildroot/%_menudir
cat > %buildroot/%_menudir/vlc << EOF
?package(vlc): command="%_bindir/vlc" hotkey="V" needs="X11" longtitle="VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution" section="Multimedia/Video" title="VideoLAN Client" icon="vlc.png" hints="Video"
EOF
%if %with_gtk
cat > %buildroot/%_menudir/gvlc << EOF
?package(gvlc): command="%_bindir/gvlc" needs="X11" longtitle="VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution" section="Multimedia/Video" title="Gtk VideoLAN Client" icon="gvlc.png" hints="Video"
EOF
%endif
%if %with_gnome
cat > %buildroot/%_menudir/gnome-vlc << EOF
?package(gnome-vlc): command="%_bindir/gnome-vlc" needs="X11" longtitle="VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution" section="Multimedia/Video" title="Gnome VideoLAN Client" icon="gnome-vlc.png" hints="Video"
EOF
%endif
%if %with_qt
cat > %buildroot/%_menudir/qvlc << EOF
?package(qvlc): command="%_bindir/qvlc" needs="X11" longtitle="VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution" section="Multimedia/Video" title="Qt VideoLAN Client" icon="qvlc.png" hints="Video"
EOF
%endif
%if %with_kde
cat > %buildroot/%_menudir/kvlc << EOF
?package(kvlc): command="%_bindir/kvlc" needs="X11" longtitle="VideoLAN is a free MPEG, MPEG2, DVD and DivX software solution" section="Multimedia/Video" title="Gnome VideoLAN Client" icon="kvlc.png" hints="Video"
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

#rpm (>= 4.0.4-20mdk) now checks for installed (but unpackaged) files
rm -f %pngdir/*

%post
%update_menus
%postun
%update_menus

%clean
rm -fr %buildroot

%files -f %name.lang
%defattr(-,root,root)
%doc README COPYING
%_bindir/vlc

%dir %_libdir/vlc

%dir %_libdir/vlc/access
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
%_libdir/vlc/access_output/libaccess_output_udp_plugin.so

%dir %_libdir/vlc/audio_filter
%_libdir/vlc/audio_filter/libfixed32tofloat32_plugin.so
%_libdir/vlc/audio_filter/libfixed32tos16_plugin.so
%_libdir/vlc/audio_filter/libfloat32tos16_plugin.so
%_libdir/vlc/audio_filter/libfloat32tos8_plugin.so
%_libdir/vlc/audio_filter/libfloat32tou16_plugin.so
%_libdir/vlc/audio_filter/libfloat32tou8_plugin.so
%_libdir/vlc/audio_filter/libheadphone_channel_mixer_plugin.so
%_libdir/vlc/audio_filter/liblinear_resampler_plugin.so
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
%_libdir/vlc/codec/libcinepak_plugin.so
%_libdir/vlc/codec/libidctclassic_plugin.so
%ifarch %ix86
%_libdir/vlc/codec/libidctmmxext_plugin.so
%_libdir/vlc/codec/libidctmmx_plugin.so
%endif
%_libdir/vlc/codec/libidct_plugin.so
%_libdir/vlc/codec/liblpcm_plugin.so
%ifarch %ix86
%_libdir/vlc/codec/libmotionmmxext_plugin.so
%_libdir/vlc/codec/libmotionmmx_plugin.so
%endif
%_libdir/vlc/codec/libmotion_plugin.so
%_libdir/vlc/codec/libmpeg_audio_plugin.so
%_libdir/vlc/codec/libmpeg_video_plugin.so
%if %with_ffmpeg
%_libdir/vlc/codec/libpostprocessing_c_plugin.so
	%ifarch %ix86
	%_libdir/vlc/codec/libpostprocessing_mmx_plugin.so
	%_libdir/vlc/codec/libpostprocessing_mmxext_plugin.so
	%endif
%endif
%_libdir/vlc/codec/libspudec_plugin.so

%dir %_libdir/vlc/control
%_libdir/vlc/control/librc_plugin.so

%dir %_libdir/vlc/demux
%_libdir/vlc/demux/libaac_plugin.so
%_libdir/vlc/demux/libasf_plugin.so
%_libdir/vlc/demux/libaudio_plugin.so
%_libdir/vlc/demux/libavi_plugin.so
%_libdir/vlc/demux/libdemuxdump_plugin.so
%_libdir/vlc/demux/libdemuxsub_plugin.so
%_libdir/vlc/demux/libes_plugin.so
%_libdir/vlc/demux/libid3_plugin.so
%_libdir/vlc/demux/libm3u_plugin.so
%_libdir/vlc/demux/libmp4_plugin.so
%_libdir/vlc/demux/libmpeg_system_plugin.so
%_libdir/vlc/demux/libps_plugin.so
%_libdir/vlc/demux/librawdv_plugin.so
%_libdir/vlc/demux/libts_plugin.so
%_libdir/vlc/demux/libwav_plugin.so

%dir %_libdir/vlc/misc
%_libdir/vlc/misc/libdummy_plugin.so
%_libdir/vlc/misc/libipv4_plugin.so
%_libdir/vlc/misc/libipv6_plugin.so
%_libdir/vlc/misc/liblogger_plugin.so
%ifarch %ix86
%_libdir/vlc/misc/libmemcpy3dn_plugin.so
%_libdir/vlc/misc/libmemcpymmxext_plugin.so
%_libdir/vlc/misc/libmemcpymmx_plugin.so
%endif
%_libdir/vlc/misc/libmemcpy_plugin.so
%_libdir/vlc/misc/libsap_plugin.so

%dir %_libdir/vlc/mux
%_libdir/vlc/mux/libmux_dummy_plugin.so
%_libdir/vlc/mux/libmux_ps_plugin.so
%_libdir/vlc/mux/libmux_ts_plugin.so

%dir %_libdir/vlc/packetizer
%_libdir/vlc/packetizer/libpacketizer_a52_plugin.so
%_libdir/vlc/packetizer/libpacketizer_copy_plugin.so
%_libdir/vlc/packetizer/libpacketizer_mpeg4audio_plugin.so
%_libdir/vlc/packetizer/libpacketizer_mpeg4video_plugin.so
%_libdir/vlc/packetizer/libpacketizer_mpegaudio_plugin.so
%_libdir/vlc/packetizer/libpacketizer_mpegvideo_plugin.so

%dir %_libdir/vlc/video_chroma
%_libdir/vlc/video_chroma/libi420_rgb_plugin.so
%_libdir/vlc/video_chroma/libi420_ymga_plugin.so
%_libdir/vlc/video_chroma/libi420_yuy2_plugin.so
%_libdir/vlc/video_chroma/libi422_yuy2_plugin.so
%ifarch %ix86
%_libdir/vlc/video_chroma/libi420_rgb_mmx_plugin.so
%_libdir/vlc/video_chroma/libi420_ymga_mmx_plugin.so
%_libdir/vlc/video_chroma/libi420_yuy2_mmx_plugin.so
%_libdir/vlc/video_chroma/libi422_yuy2_mmx_plugin.so
%endif

%dir %_libdir/vlc/video_filter
%_libdir/vlc/video_filter/libadjust_plugin.so
%_libdir/vlc/video_filter/libclone_plugin.so
%_libdir/vlc/video_filter/libcrop_plugin.so
%_libdir/vlc/video_filter/libdeinterlace_plugin.so
%_libdir/vlc/video_filter/libdistort_plugin.so
%_libdir/vlc/video_filter/libinvert_plugin.so
%_libdir/vlc/video_filter/libmotionblur_plugin.so
%_libdir/vlc/video_filter/libtransform_plugin.so
%_libdir/vlc/video_filter/libwall_plugin.so

%dir %_libdir/vlc/video_output
%_libdir/vlc/video_output/libfb_plugin.so
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
%doc README
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
# FIXME: seems to be mozilla-version/plugin on Mandrake
#%dir %_libdir/mozilla
%_libdir/mozilla*/*
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
%update_menus
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
%update_menus
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
%update_menus
%endif

%if %with_kde
%files -n kvlc
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
%update_menus
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
%_libdir/vlc/codec/libmad_plugin.so
%_libdir/vlc/demux/libid3tag_plugin.so
%endif

%if %with_ogg
%files plugin-ogg
%defattr(-,root,root)
%doc README
%_libdir/vlc/demux/libogg_plugin.so
%_libdir/vlc/codec/libvorbis_plugin.so
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

%changelog
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
