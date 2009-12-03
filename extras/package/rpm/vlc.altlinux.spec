%set_verify_elf_method textrel=relaxed

%define svnrevision 20348

%def_disable debug

%def_disable ggi
%def_disable svg
%def_disable upnp
%def_disable gnomevfs
%def_enable smb
%def_enable dirac
%def_disable dca
%def_disable libid3tag
%def_disable java_bindings
%def_disable mediacontrol_python_bindings

%if_enabled debug
%set_strip_method none
%endif

Name: vlc
Version: 0.9.0

Release: alt0.svn%svnrevision

Summary: VLC Media Player
License: GPL

Group: Video
Url: http://www.videolan.org
Packager: Pavlov Konstantin <thresh@altlinux.ru>

Source: vlc-%version.tar.bz2

Obsoletes: %name-mad
Provides: %name-interface = %version-%release

%define libdvdcss_ver 1.2.8
%define ffmpeg_ver 0.5.0-alt1.svn6729
%define mpeg2dec_ver 0.4.0
%define libvcd_ver 0.7.23
%define faad_ver 2.0-alt2.20040923
%define faac_ver 1.24
%define ebml_ver 0.7.6
%define matroska_ver 0.8.0
%define cddb_ver 1.2.1-alt1
%define seamonkey_ver 1.0.4-alt4
%define dirac_ver 0.7.0-alt1

Requires: lib%name = %version-%release

BuildPreReq: cvs
BuildPreReq: glibc-kernheaders
BuildPreReq: libdvdcss-devel >= %libdvdcss_ver
BuildPreReq: libavcodec-devel >= %ffmpeg_ver
BuildPreReq: libpostproc-devel >= %ffmpeg_ver
BuildPreReq: libavformat-devel >= %ffmpeg_ver
BuildPreReq: libswscale-devel >= %ffmpeg_ver
BuildPreReq: libmpeg2-devel >= %mpeg2dec_ver
BuildPreReq: libfaad-devel >= %faad_ver
BuildPreReq: libfaac-devel >= %faac_ver
BuildPreReq: libebml-devel >= %ebml_ver
BuildPreReq: libmatroska-devel >= %matroska_ver
BuildPreReq: seamonkey-devel >= %seamonkey_ver
BuildPreReq: libcddb-devel >= %cddb_ver
%if_enabled mediacontrol_python_bindings
BuildPreReq: python-devel >= 2.4
%endif
BuildPreReq: rpm-build-python
BuildPreReq: liblive-devel >= 0.0.0-alt0.2006.10.18a

BuildRequires: ORBit2-devel aalib-devel esound-devel freetype2-devel gcc-c++
BuildRequires: glib2-devel libSDL-devel libtwolame-devel
BuildRequires: libSDL_image-devel liba52-devel libalsa-devel libarts-devel
BuildRequires: libaudiofile-devel libbonobo2-devel libcaca-devel
BuildRequires: libcdio-devel libdvbpsi-devel libdvdnav-devel
BuildRequires: libdvdread-devel libflac-devel libgcrypt-devel
%{?_enable_ggi:BuildRequires: libggi-devel libgii-devel}
%{?_enable_svg:BuildRequires: librsvg2-devel}
BuildRequires: libgnutls-devel libgpg-error-devel libgtk+2-devel
BuildRequires: libjpeg-devel liblirc-devel
BuildRequires: libmad-devel libmodplug-devel libslang-devel libspeex-devel
BuildRequires: libmpcdec-devel libncurses-devel libogg-devel
BuildRequires: libpango-devel libpng-devel libshout2-devel
BuildRequires: libstdc++-devel libsysfs-devel libtheora-devel libtiff-devel
BuildRequires: libtinfo-devel libvcd-devel libvorbis-devel libxml2-devel
BuildRequires: libxosd-devel wxGTK2u-devel
BuildRequires: libnspr-devel libnss-devel libgoom-devel
BuildRequires: libhal-devel libx264-devel subversion vim-devel 
BuildRequires: jackit-devel liblame-devel xvid-devel zlib-devel
BuildRequires: libavahi-devel
BuildRequires: libnotify-devel libdbus-glib-devel
BuildRequires: fortune-mod >= 1.0-ipl33mdk
BuildRequires: libraw1394-devel libdc1394-devel libavc1394-devel
BuildRequires: browser-plugins-npapi-devel

%if_enabled libid3tag
BuildRequires: libid3tag-devel
%endif

%if_enabled dca
BuildRequires: libdca-devel
%endif

%if_enabled gnomevfs
BuildRequires: gnome-vfs2-devel gnome-vfs2 gnome-mime-data libGConf2-devel
%endif

%if_enabled java_bindings
BuildRequires: j2se1.5-sun-devel
%endif

%if_enabled upnp
BuildRequires: libupnp-devel
%endif

%if_enabled smb
BuildRequires: libsmbclient-devel
%endif

%if_enabled dirac
BuildPreReq: libdirac-devel = %dirac_ver
%endif

BuildRequires: libX11-devel libXv-devel libmesa-devel libXext-devel 
BuildRequires: libXt-devel

BuildRequires: libqt4-devel liblua5-devel

%description
VLC Media Player is a free network-aware MPEG1, MPEG2, MPEG4 (aka DivX),
DVD and many-many-more-player-and-streamer.

The VLC Media Player allows to play MPEG2 Transport Streams from the
network or from a file, as well as direct DVD playback.

This version includes MPEG1 support, direct DVD support, DVD decryption,
arbitrary, seeking in the stream, pause, fast forward and slow motion,
hardware YUV acceleration and a few new interface features including
drag'n'drop... and more more more. :)

If you want a GUI interface for VLC, install one of interface packages,
the best one is wxwidgets interface.

%package interface-http
Summary: HTTP interface plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release
Provides: %name-interface = %version-%release
Provides: %name-plugin-http = %version-%release

%description interface-http

This package is an http interface for VLC Media Player.

%package interface-lirc
Summary: Lirc inteface plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release
Provides: %name-interface = %version-%release
Provides: vlc-plugin-lirc = %version-%release
Obsoletes: vlc-plugin-lirc

%description interface-lirc

This package is an infrared lirc interface for
VLC Media Player. To activate it, use the `--intf lirc' flag.

%package interface-ncurses
Summary: ncurses plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release
Provides: %name-plugin-ncurses = %version-%release
Provides: %name-interface = %version-%release

%description interface-ncurses
This package is an ncurses interface for VLC Media Player.

%package interface-skins2
Summary: Skins2 plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release
Provides: %name-interface = %version-%release
Requires: %name-interface-wxwidgets = %version-%release

%description interface-skins2
This package is an skins2 interface for VLC Media Player.

%package interface-telnet
Summary: Telnet interface plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release
Provides: %name-interface = %version-%release

%description interface-telnet
This package is a telnet interface for VLC Media Player.

%package interface-wxwidgets
Summary: WXWidgets plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release
Provides: %name-interface = %version-%release
Provides: %name-plugin-wxwidgets = %version-%release

%description interface-wxwidgets
This package is an wxwidgets interface for VLC Media Player.

%package interface-qt4
Summary: QT4 interface plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release
Provides: %name-interface = %version-%release
Provides: %name-plugin-qt4 = %version-%release

%description interface-qt4
This package is an qt4 interface for VLC Media Player.

%package plugin-a52
Summary: a52 input/decoder plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-a52
This package contains A52 decoder plugin for VLC Media Player.

%package plugin-aa
Summary: ASCII art video output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-aa
This is an ASCII art video output plugin for VLC Media Player.
To activate it, use the `--vout aa' flag or select the `aa'
vout plugin from the preferences menu.

%package plugin-alsa
Summary: ALSA audio output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-alsa
This package adds support for Advanced Linux Sound Architecture to
VLC Media Player. To activate it, use the `--aout alsa' flag or
select the `alsa' aout plugin from the preferences menu.

%package plugin-arts
Summary: aRts audio output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-arts
This package adds support for aRts Sound System to VLC Media Player.
To activate it, use the `--aout arts' flag or
select the `arts' aout plugin from the preferences menu.

%package plugin-audiocd
Summary: AudioCD access plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-audiocd
This package contains AudioCD access plugin for VLC Media Player.

%package plugin-caca
Summary: Colored ASCII art video output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-caca
This is an colored ASCII art video output plugin for VLC Media Player.
To activate it, use the `--vout caca' flag or select the `caca'
vout plugin from the preferences menu.

%package plugin-bonjour
Summary: Bonjour (avahi) services discovery plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-bonjour
This package contains Bonjour (avahi) service discovery plugin for VLC Media Player.

%package plugin-cmml
Summary: CMML input/codec plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-cmml
This package contains CMML codec plugin for VLC Media Player.

%package plugin-dv
Summary: DC1394/DV (firewire) plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-dv
This package contains DC1394/DV (firewire) access plugin for VLC Media Player.

%if_enabled dirac
%package plugin-dirac
Summary: Dirac codec plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-dirac
This package contains DIRAC codec plugin for VLC Media Player.
%endif

%if_enabled dca
%package plugin-dca
Summary: DTS demuxer plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release
Provides: vlc-plugin-dts = %version-%release
Obsoletes: vlc-plugin-dts < %version-%release

%description plugin-dca
This package contains DTS demuxer plugin for VLC Media Player.
%endif

%package plugin-dvb
Summary: DVB plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release
Requires: %name-plugin-ts = %version-%release

%description plugin-dvb
This package adds capability of demultiplexing a satellite DVB stream to VLC Media Player.

%package plugin-dvdnav
Summary: DVDNav input plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-dvdnav
This package adds capability of DVDNav (DVD w/ menu) input to VLC Media Player.

%package plugin-dvdread
Summary: DVDRead input (DVD without a menu) plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-dvdread
This package adds support of DVDRead (DVD w/o menu) input to VLC Media Player.

%package plugin-esd
Summary: ESD audio plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-esd
This package adds support for Enlightened Sound Daemon to VLC Media Player. 
To activate it, use the `--aout esd' flag or select the `esd' aout plugin
from the preferences menu.

%package plugin-faad
Summary: FAAD input plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-faad
This package adds support for FAAD codec in VLC Media Player.

%package plugin-ffmpeg
Summary: FFMPeg plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release
Requires: libavcodec >= 0.5.0-alt1.svn8045

%description plugin-ffmpeg
This package adds support for ffmpeg decoders, encoders and demuxers
in VLC Media Player.

%package plugin-framebuffer
Summary: Framebuffer output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-framebuffer
This package adds support for framebuffer video output in VLC Media Player.

%package plugin-flac
Summary: FLAC codec plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-flac
This package contains FLAC codec plugin for VLC Media Player.

%package plugin-freetype
Summary: FreeType OSD plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release
Requires: fonts-ttf-dejavu

%description plugin-freetype
This package contains freetype subtitles and OSD text output plugin 
to VLC Media Player.

%if_enabled ggi
%package plugin-ggi
Summary: GGI video output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-ggi
This is a GGI plugin for VLC Media Player.  To activate it, use the 
`--vout ggi' flag or select the `ggi' vout plugin from the preferences menu.
%endif

%package plugin-glx
Summary: GLX video output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-glx
This is an GLX video output plugin for VLC Media Player.
To activate it, use the `--vout glx' flag or select the `glx'
vout plugin from the preferences menu.

%if_enabled gnomevfs
%package plugin-gnomevfs
Summary: Gnome VFS 2 access plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-gnomevfs
This package contains Gnome VFS 2 access plugin for VLC Media Player.
%endif

%package plugin-gnutls
Summary: GNU TLS plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-gnutls
This package contains GNU TLS plugin for VLC Media Player.

%package plugin-goom
Summary: GOOM plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-goom
This package contains GOOM visualization plugin for VLC Media Player.

%package plugin-h264
Summary: h264 output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-h264
This package contains h264 coder/packetizer plugin for VLC Media Player.

%package plugin-hal
Summary: HAL services discovery plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-hal
This package contains HAL service discovery plugin for VLC Media Player.

%package plugin-jack
Summary: Jack audio output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-jack
This package contains Jack audio output plugin for VLC Media Player.

%package plugin-image
Summary: Image video output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-image
This is a image video output plugin for VLC Media Player.
To activate it, use the `--vout image' flag or select the `image'
vout plugin from the preferences menu.

%package plugin-live555
Summary: LiveMedia (RTSP) demuxing support for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-live555
This package contains LiveMedia (RTSP) demuxer support for VLC Media Player.

%ifnarch x86_64
%package plugin-loader
Summary: DLL Loader plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release
Provides: %name-plugin-realaudio = %version-%release
Obsoletes: %name-plugin-realaudio

%description plugin-loader
This package contains windows DLL loader plugin to VLC Media Player as well
as support for realaudio via those DLL.
%endif

%package plugin-mad
Summary: MAD (MP3/ID3) demuxer plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-mad
This package contains MAD (MP3 demux/ID3 tag) plugin for VLC Media Player.

%package plugin-matroska
Summary: Matroska Video demuxer plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-matroska
This package contains Matroska Video demuxing plugin for VLC Media Player.

%package plugin-mga
Summary: MGA Matrox video output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-mga
This package contains MGA Matrox output plugin for VLC Media Player.

%package plugin-modplug
Summary: modplug demuxer plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-modplug
This package contains modplug demuxing plugin for VLC Media Player.

%package plugin-mpeg2
Summary: MPEG1/2 codec plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-mpeg2
This package contains MPEG1/2 decoder plugin for VLC Media Player.

%package plugin-musepack
Summary: Musepack demuxer plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-musepack
This package contains musepack demuxer plugin for VLC Media Player.

%package plugin-notify
Summary: Notify SDP plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-notify
This package contains notify plugin for VLC Media Player.

%package plugin-ogg
Summary: OGG codec plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-ogg
This package contains OGG codec and Vorbis muxer/demuxer
plugin for VLC Media Player.

%package plugin-opengl
Summary: OpenGL video output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-opengl
This is an OpenGL video output plugin for VLC Media Player.
To activate it, use the `--vout opengl' flag or select the `opengl'
vout plugin from the preferences menu.

%package plugin-osd
Summary: OSD plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-osd
This package adds support for OSD visualization for VLC Media Player.

%package plugin-oss
Summary: OSS audio output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-oss
This package adds support for OSS to VLC Media Player.
To activate it, use the `--aout oss' flag or select the `oss'
aout plugin from the preferences menu.

%package plugin-png
Summary: PNG plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-png
This package contains PNG codec plugin for VLC Media Player.

%package plugin-podcast
Summary: Podcast SDP plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-podcast
This package contains podcast discovery plugin for VLC Media Player.

%package plugin-realrtsp
Summary: REAL RTSP access plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-realrtsp
This package contains REAL RTSP access plugin for VLC Media Player.

%package plugin-screen
Summary: Screen capture plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-screen
This package contains screen capture plugin for VLC Media Player.

%package plugin-sdl
Summary: Simple DirectMedia Layer video output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-sdl
This package adds support for Simple DirectMedia Layer library to 
VLC Media Player. To activate it, use the `--vout sdl' or
`--aout sdl' flags or select the `sdl' vout or aout plugin from the
preferences menu.

%package plugin-sdlimage
Summary: SDL Image codec plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-sdlimage
This package contains SDL Image codec plugin for VLC Media Player.

%package plugin-shout
Summary: SHOUT access output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-shout
This package adds support for SHOUT output access/services 
discovery to VLC Media Player.

%if_enabled smb
%package plugin-smb
Summary: SMB access plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-smb
This package contains SMB access plugin to VLC Media Player.
%endif

%package plugin-snapshot
Summary: Snapshot video output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-snapshot
This package contains snapshot video output plugin to VLC Media Player.

%package plugin-speex
Summary: speex codec support plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-speex
This package contains SPEEX plugin for VLC Media Player.

%if_enabled svg
%package plugin-svg
Summary: SVG plugin plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-svg
This package contains SVG plugin for VLC Media Player.
%endif

%package plugin-theora
Summary: Theora codec plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-theora
This package contains Theora codec support for VLC Media Player.

%package plugin-ts
Summary: TS mux/demux plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-ts
This package contains TS mux/demux support for VLC Media Player.

One of the essential plugins.

%package plugin-twolame
Summary: TwoLAME encoding plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-twolame
This package contains TwoLAME mpeg2 encoder plugin for VLC Media Player.

%package plugin-v4l
Summary: Video4Linux input plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-v4l
This package adds support for Video4Linux to VLC Media Player.

%package plugin-videocd
Summary: VideoCD input plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-videocd
This package contains VideoCD access plugin for VLC Media Player.

%package plugin-x11
Summary: X11 output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-x11
This package adds support for X11 video output to VLC Media Player.

%package plugin-xml
Summary: XML plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-xml
This package contains XML plugin to VLC Media Player.

%package plugin-xvideo
Summary: XVideo output plugin for VLC Media Player
Group: Video
Requires: lib%name = %version-%release

%description plugin-xvideo
This package adds support for XVideo output to VLC Media Player.

%package -n lib%name
Summary: VLC Media Player library
Group: System/Libraries

%description -n lib%name
This is a base VLC library. EXPERIMENTAL!!!

%package -n lib%name-devel
Summary: Development files for VLC Media Player
Group: Development/C
Requires: lib%name = %version-%release

%description -n lib%name-devel
This package provides files needed to develop plugins for VLC Media Player.

%package -n mozilla-plugin-vlc
Summary: VLC plugin for mozilla-based browsers
Group: Video

%description -n mozilla-plugin-vlc
This package contains mozilla plugin for VLC Media Player.

%if_enabled mediacontrol_python_bindings
%package -n python-module-vlc
Summary: Python bindings for VLC Media Player
Group: Video

%description -n python-module-vlc
This package contains python bindings for VLC Media Player.
%endif

%package -n vim-plugin-vlc-syntax
Summary: VIm syntax for VLC Media Player
Group: Video

%description -n vim-plugin-vlc-syntax
This package contains VIm syntax for VLC Media Player.

%package -n fortunes-vlc
Summary: VLC fortunes
Group: Video
PreReq: fortune-mod >= 1.0-ipl33mdk

%description -n fortunes-vlc
This package contains fortunes from VLC Media Player.

%package maxi
Summary: Maxi package for VLC Media Player
Group: Video
Requires: vlc vlc-interface-ncurses vlc-interface-skins2 vlc-interface-wxwidgets vlc-interface-lirc vlc-interface-telnet vlc-interface-http vlc-plugin-a52 vlc-plugin-aa vlc-plugin-alsa vlc-plugin-arts vlc-plugin-audiocd vlc-plugin-caca vlc-plugin-cmml vlc-plugin-dvb vlc-plugin-dvdnav vlc-plugin-dvdread vlc-plugin-esd vlc-plugin-faad vlc-plugin-ffmpeg vlc-plugin-framebuffer vlc-plugin-flac vlc-plugin-freetype vlc-plugin-glx vlc-plugin-gnutls vlc-plugin-goom vlc-plugin-h264 vlc-plugin-hal vlc-plugin-jack vlc-plugin-image vlc-plugin-mad vlc-plugin-mga vlc-plugin-modplug vlc-plugin-mpeg2 vlc-plugin-musepack vlc-plugin-ogg vlc-plugin-opengl vlc-plugin-osd vlc-plugin-oss vlc-plugin-png vlc-plugin-podcast vlc-plugin-realrtsp vlc-plugin-screen vlc-plugin-sdl vlc-plugin-sdlimage vlc-plugin-shout vlc-plugin-snapshot vlc-plugin-speex vlc-plugin-theora vlc-plugin-v4l vlc-plugin-videocd vlc-plugin-x11 vlc-plugin-xml vlc-plugin-xvideo libvlc mozilla-plugin-vlc vim-plugin-vlc-syntax vlc-plugin-bonjour vlc-plugin-matroska vlc-plugin-ts vlc-plugin-notify vlc-plugin-live555 vlc-plugin-twolame vlc-plugin-dv
%{?_enable_dca:Requires: vlc-plugin-dca}
%{?_enable_svg:Requires: vlc-plugin-svg}
%{?_enable_ggi:Requires: vlc-plugin-ggi}
%ifnarch x86_64
Requires: vlc-plugin-loader vlc-plugin-realaudio
%endif
%if_enabled smb
Requires: vlc-plugin-smb
%endif
%if_enabled dirac
Requires: vlc-plugin-dirac
%endif
%if_enabled gnomevfs
Requires: vlc-plugin-gnomevfs
%endif

%description maxi
This is a virtual package with every plugin of VLC Media Player.

%package normal
Summary: Normal package for VLC Media Player
Group: Video
Requires: vlc vlc-interface-wxwidgets vlc-plugin-a52 vlc-plugin-alsa vlc-plugin-dvdread vlc-plugin-ffmpeg vlc-plugin-xvideo vlc-plugin-x11 libvlc vlc-plugin-ts vlc-plugin-live555 vlc-plugin-xml
Provides: %name-common = %version-%release
Obsoletes: %name-common < %version-%release

%description normal
This is a virtual 'common' package with most useable plugins of VLC Media Player.
It comes with wxWidgets interface, alsa audio output, full DVD read 
support, all of the ffmpeg capabilities to read and decode files 
and Xvideo/X11 video output plugins.

%define _vlc_pluginsdir %_libdir/%name

%prep
%setup -q -n %name-%version

%build

./bootstrap

%if_enabled java_bindings
export JAVA_HOME=%_libdir/j2se1.5-sun
%endif

%configure \
	%{subst_enable debug} \
	--disable-rpath \
	--disable-static \
	--enable-utf8 \
	--enable-a52 \
	--enable-aa \
	--enable-alsa \
	--enable-arts \
	--enable-audioscrobbler \
	--enable-caca \
	--enable-cdda \
	--disable-cddax \
	--enable-cmml \
	--enable-dc1394 \
	%{subst_enable dirac} \
	--enable-dmo \
	--enable-dv \
	--enable-dvb \
	--enable-dvbpsi \
	--enable-dvd \
	--enable-dvdnav \
	--enable-dvdplay \
	--enable-dvdread \
	%{subst_enable dca} \
	--enable-esd \
	--enable-faad \
	--enable-fb \
	--enable-ffmpeg \
	--enable-flac \
	--enable-freetype \
	--enable-fribidi \
	%{subst_enable ggi} \
	--enable-glx \
	%{subst_enable gnomevfs} \
	--enable-gnutls \
	--enable-goom \
	--enable-hal \
	--enable-httpd \
	--enable-jack \
	%{?_enable_java_bindings:--enable-java-bindings} \
	--enable-libtool \
	--enable-libcddb \
	--enable-libcdio \
	--enable-libmpeg2 \
	--enable-libxml2 \
	--enable-lirc \
	--enable-live555 \
	--with-live555-tree=%_libdir/live \
%ifnarch x86_64
	--enable-loader \
	--enable-real \
%endif
	--enable-mad \
	--enable-mga \
	--enable-mkv \
	--enable-mod \
	%{?_enable_mediacontrol_python_bindings:--enable-mediacontrol_python_bindings} \
	--enable-mozilla \
	--enable-mpc \
	--enable-ncurses \
	--enable-notify \
	--enable-ogg \
	--enable-opengl \
	--enable-oss \
	--enable-png \
	--enable-realrtsp \
	--enable-release \
	--enable-screen \
	--enable-sdl \
	--enable-shout \
	--enable-skins2 \
	--enable-slp \
	%{subst_enable smb} \
	--enable-snapshot \
	--enable-speex \
	%{subst_enable svg} \
	--enable-tarkin \
	--enable-theora \
	--enable-tremor \
	--enable-twolame \
	%{subst_enable upnp} \
	--enable-v4l \
	--enable-vcd \
	--enable-vcdx \
	--enable-visual \
	--enable-vlm \
	--enable-vorbis \
	--enable-wxwidgets \
	--enable-x11 \
	--enable-x264 \
	--enable-xosd \
	--enable-xvideo \
	--with-ffmpeg-faac \
	--with-ffmpeg-mp3lame \
	--with-ffmpeg-vorbis \
	--with-ffmpeg-theora \
	--with-ffmpeg-ogg \
	--with-ffmpeg-zlib

%make_build

%install

mkdir -p %buildroot%_libdir
%make_install DESTDIR="%buildroot" install

install -pD -m644 doc/vlc.1 %buildroot/%_man1dir/vlc.1

# freedesktop menu
mkdir -p %buildroot%_datadir/applications 
install -pm644 share/applications/vlc.desktop %buildroot%_datadir/applications/vlc.desktop

# icons
mkdir -p %buildroot/{%_miconsdir,%_liconsdir}
install -m644 %buildroot/%_datadir/vlc/vlc32x32.png %buildroot/%_iconsdir/vlc.png

# fix installation of mozilloids plugin
mkdir -p %buildroot%browser_plugins_path
mv %buildroot%_libdir/mozilla/plugins/* %buildroot%browser_plugins_path

# remove non-packaged files
rm -f %buildroot%_libdir/*.a
rm -f %buildroot%_vlc_pluginsdir/*.a
rm -rf %buildroot%_docdir/%name
find %buildroot -type f -name "*.la" -delete

# vim stuff
mkdir -p %buildroot%vim_syntax_dir
cp extras/vlc.vim %buildroot%vim_syntax_dir/

# fortunes stuff
mkdir -p %buildroot%_gamesdatadir/fortune
cp doc/fortunes.txt %buildroot%_gamesdatadir/fortune/vlc
strfile %buildroot%_gamesdatadir/fortune/vlc %buildroot%_gamesdatadir/fortune/vlc.dat

%find_lang --output=%name.files %name

%post
%update_menus

%postun
%clean_menus

%files -f %name.files
%_bindir/vlc
%_bindir/svlc
%dir %_vlc_pluginsdir

%exclude %_datadir/%name/http
%exclude %_datadir/%name/skins2

%_datadir/%name
%_man1dir/*
%_iconsdir/vlc.png

%dir %_vlc_pluginsdir/access
%_vlc_pluginsdir/access/libaccess_directory_plugin.so*
%_vlc_pluginsdir/access/libaccess_file_plugin.so*
%_vlc_pluginsdir/access/libaccess_ftp_plugin.so*
%_vlc_pluginsdir/access/libaccess_http_plugin.so*
%_vlc_pluginsdir/access/libaccess_mms_plugin.so*
%_vlc_pluginsdir/access/libaccess_udp_plugin.so*
%_vlc_pluginsdir/access/libaccess_fake_plugin.so*
%_vlc_pluginsdir/access/libaccess_tcp_plugin.so*

%dir %_vlc_pluginsdir/access_output
%_vlc_pluginsdir/access_output/libaccess_output_dummy_plugin.so*
%_vlc_pluginsdir/access_output/libaccess_output_file_plugin.so*
%_vlc_pluginsdir/access_output/libaccess_output_http_plugin.so*
%_vlc_pluginsdir/access_output/libaccess_output_udp_plugin.so*

%dir %_vlc_pluginsdir/access_filter
%_vlc_pluginsdir/access_filter/libaccess_filter_record_plugin.so*
%_vlc_pluginsdir/access_filter/libaccess_filter_timeshift_plugin.so*
%_vlc_pluginsdir/access_filter/libaccess_filter_dump_plugin.so*
%_vlc_pluginsdir/access_filter/libaccess_filter_bandwidth_plugin.so*

%dir %_vlc_pluginsdir/audio_filter
%_vlc_pluginsdir/audio_filter/libbandlimited_resampler_plugin.so*
%_vlc_pluginsdir/audio_filter/libdolby_surround_decoder_plugin.so*
%_vlc_pluginsdir/audio_filter/libdtstospdif_plugin.so*
%_vlc_pluginsdir/audio_filter/libheadphone_channel_mixer_plugin.so*
%_vlc_pluginsdir/audio_filter/liblinear_resampler_plugin.so*
%_vlc_pluginsdir/audio_filter/libtrivial_channel_mixer_plugin.so*
%_vlc_pluginsdir/audio_filter/libtrivial_resampler_plugin.so*
%_vlc_pluginsdir/audio_filter/libugly_resampler_plugin.so*
%_vlc_pluginsdir/audio_filter/libaudio_format_plugin.so*
%_vlc_pluginsdir/audio_filter/libequalizer_plugin.so*
%_vlc_pluginsdir/audio_filter/libnormvol_plugin.so*
%_vlc_pluginsdir/audio_filter/libsimple_channel_mixer_plugin.so*
%_vlc_pluginsdir/audio_filter/libparam_eq_plugin.so*
%_vlc_pluginsdir/audio_filter/libconverter_fixed_plugin.so*
%_vlc_pluginsdir/audio_filter/libconverter_float_plugin.so*
%_vlc_pluginsdir/audio_filter/libmono_plugin.so*

%dir %_vlc_pluginsdir/audio_mixer
%_vlc_pluginsdir/audio_mixer/libfloat32_mixer_plugin.so*
%_vlc_pluginsdir/audio_mixer/libspdif_mixer_plugin.so*
%_vlc_pluginsdir/audio_mixer/libtrivial_mixer_plugin.so*

%dir %_vlc_pluginsdir/audio_output
%_vlc_pluginsdir/audio_output/libaout_file_plugin.so*

%dir %_vlc_pluginsdir/codec
%_vlc_pluginsdir/codec/liba52_plugin.so*
%_vlc_pluginsdir/codec/libadpcm_plugin.so*
%_vlc_pluginsdir/codec/libaraw_plugin.so*
%_vlc_pluginsdir/codec/librawvideo_plugin.so*
%_vlc_pluginsdir/codec/libcinepak_plugin.so*
%_vlc_pluginsdir/codec/libdts_plugin.so*
%_vlc_pluginsdir/codec/liblpcm_plugin.so*
%_vlc_pluginsdir/codec/libmpeg_audio_plugin.so*
%_vlc_pluginsdir/codec/libspudec_plugin.so*
%_vlc_pluginsdir/codec/libfake_plugin.so*
%_vlc_pluginsdir/codec/libsubsdec_plugin.so*
%_vlc_pluginsdir/codec/libcvdsub_plugin.so*
%_vlc_pluginsdir/codec/libtelx_plugin.so*

%dir %_vlc_pluginsdir/control
%_vlc_pluginsdir/control/librc_plugin.so*
%_vlc_pluginsdir/control/libgestures_plugin.so*
%_vlc_pluginsdir/control/libhotkeys_plugin.so*
%_vlc_pluginsdir/control/libnetsync_plugin.so*
%_vlc_pluginsdir/control/libshowintf_plugin.so*
%_vlc_pluginsdir/control/libmotion_plugin.so*

%dir %_vlc_pluginsdir/demux
#%_vlc_pluginsdir/demux/libaac_plugin.so*
%_vlc_pluginsdir/demux/libasf_plugin.so*
%_vlc_pluginsdir/demux/libau_plugin.so*
#%_vlc_pluginsdir/demux/libaudio_plugin.so*
%_vlc_pluginsdir/demux/libavi_plugin.so*
%_vlc_pluginsdir/demux/liba52sys_plugin.so*
%_vlc_pluginsdir/demux/libdemuxdump_plugin.so*
#%_vlc_pluginsdir/demux/libdemuxsub_plugin.so*
#%_vlc_pluginsdir/demux/libes_plugin.so*
#%_vlc_pluginsdir/demux/libid3_plugin.so*
#%_vlc_pluginsdir/demux/libm3u_plugin.so*
%_vlc_pluginsdir/demux/libm4v_plugin.so*

%_vlc_pluginsdir/demux/libmp4_plugin.so*
%_vlc_pluginsdir/demux/libps_plugin.so*
%_vlc_pluginsdir/demux/librawdv_plugin.so*
%_vlc_pluginsdir/demux/libwav_plugin.so*
%_vlc_pluginsdir/demux/libaiff_plugin.so*
%_vlc_pluginsdir/demux/libdtssys_plugin.so*
%_vlc_pluginsdir/demux/libm4a_plugin.so*
%_vlc_pluginsdir/demux/libmjpeg_plugin.so*
#%_vlc_pluginsdir/demux/libmod_plugin.so*
%_vlc_pluginsdir/demux/libmpga_plugin.so*
%_vlc_pluginsdir/demux/libmpgv_plugin.so*
%_vlc_pluginsdir/demux/libnsc_plugin.so*
%_vlc_pluginsdir/demux/libnsv_plugin.so*
%_vlc_pluginsdir/demux/libnuv_plugin.so*
%_vlc_pluginsdir/demux/libplaylist_plugin.so*
%_vlc_pluginsdir/demux/libpva_plugin.so*
%_vlc_pluginsdir/demux/libreal_plugin.so*
#%_vlc_pluginsdir/demux/libsgimb_plugin.so*
%_vlc_pluginsdir/demux/libsubtitle_plugin.so*
%_vlc_pluginsdir/demux/libty_plugin.so*
%_vlc_pluginsdir/demux/libvobsub_plugin.so*
%_vlc_pluginsdir/demux/libvoc_plugin.so*
%_vlc_pluginsdir/demux/libxa_plugin.so*
%_vlc_pluginsdir/demux/libtta_plugin.so
%_vlc_pluginsdir/demux/libh264_plugin.so*
%_vlc_pluginsdir/demux/libvc1_plugin.so*
%_vlc_pluginsdir/demux/libluaplaylist_plugin.so*
%_vlc_pluginsdir/demux/librawvid_plugin.so*

%dir %_vlc_pluginsdir/gui

%dir %_vlc_pluginsdir/misc
%_vlc_pluginsdir/misc/libdummy_plugin.so*
#%_vlc_pluginsdir/misc/libhttpd_plugin.so*
#%_vlc_pluginsdir/misc/libipv4_plugin.so*
#%_vlc_pluginsdir/misc/libipv6_plugin.so*
%_vlc_pluginsdir/misc/liblogger_plugin.so*
%_vlc_pluginsdir/misc/libvod_rtsp_plugin.so*
%_vlc_pluginsdir/misc/libmemcpy_plugin.so*
%_vlc_pluginsdir/misc/libscreensaver_plugin.so*
%_vlc_pluginsdir/misc/libexport_plugin.so*
%_vlc_pluginsdir/misc/libgrowl_plugin.so*
%_vlc_pluginsdir/misc/libmemcpy3dn_plugin.so*
%_vlc_pluginsdir/misc/libmemcpymmx_plugin.so*
%_vlc_pluginsdir/misc/libmemcpymmxext_plugin.so*
%_vlc_pluginsdir/misc/libaudioscrobbler_plugin.so*
%_vlc_pluginsdir/misc/libprobe_hal_plugin.so*
%_vlc_pluginsdir/misc/libprofile_parser_plugin.so*


%dir %_vlc_pluginsdir/services_discovery
%_vlc_pluginsdir/services_discovery/libsap_plugin.so*

%dir %_vlc_pluginsdir/mux
%_vlc_pluginsdir/mux/libmux_asf_plugin.so*
%_vlc_pluginsdir/mux/libmux_avi_plugin.so*
%_vlc_pluginsdir/mux/libmux_dummy_plugin.so*
#%_vlc_pluginsdir/mux/libmux_ogg_plugin.so*
%_vlc_pluginsdir/mux/libmux_ps_plugin.so*
%_vlc_pluginsdir/mux/libmux_mp4_plugin.so*
%_vlc_pluginsdir/mux/libmux_mpjpeg_plugin.so*
%_vlc_pluginsdir/mux/libmux_wav_plugin.so*

#%_vlc_pluginsdir/mux/libmux_ts_plugin.so*

%dir %_vlc_pluginsdir/packetizer
#_vlc_pluginsdir/packetizer/libpacketizer_a52_plugin.so*
%_vlc_pluginsdir/packetizer/libpacketizer_copy_plugin.so*
%_vlc_pluginsdir/packetizer/libpacketizer_mpeg4audio_plugin.so*
%_vlc_pluginsdir/packetizer/libpacketizer_mpeg4video_plugin.so*
#%_vlc_pluginsdir/packetizer/libpacketizer_mpegaudio_plugin.so*
%_vlc_pluginsdir/packetizer/libpacketizer_mpegvideo_plugin.so*
%_vlc_pluginsdir/packetizer/libpacketizer_vc1_plugin.so*


%dir %_vlc_pluginsdir/stream_out
%_vlc_pluginsdir/stream_out/libstream_out_display_plugin.so*
%_vlc_pluginsdir/stream_out/libstream_out_dummy_plugin.so*
%_vlc_pluginsdir/stream_out/libstream_out_duplicate_plugin.so*
%_vlc_pluginsdir/stream_out/libstream_out_es_plugin.so*
%_vlc_pluginsdir/stream_out/libstream_out_standard_plugin.so*
%_vlc_pluginsdir/stream_out/libstream_out_bridge_plugin.so*
%_vlc_pluginsdir/stream_out/libstream_out_description_plugin.so*
%_vlc_pluginsdir/stream_out/libstream_out_gather_plugin.so*
%_vlc_pluginsdir/stream_out/libstream_out_mosaic_bridge_plugin.so*
%_vlc_pluginsdir/stream_out/libstream_out_rtp_plugin.so*
#%_vlc_pluginsdir/stream_out/libstream_out_switcher_plugin.so*
%_vlc_pluginsdir/stream_out/libstream_out_transcode_plugin.so*
%_vlc_pluginsdir/stream_out/libstream_out_autodel_plugin.so*

%dir %_vlc_pluginsdir/video_chroma
%_vlc_pluginsdir/video_chroma/libi420_rgb_plugin.so*
%_vlc_pluginsdir/video_chroma/libi420_ymga_plugin.so*
%_vlc_pluginsdir/video_chroma/libi420_yuy2_plugin.so*
%_vlc_pluginsdir/video_chroma/libi422_yuy2_plugin.so*
%_vlc_pluginsdir/video_chroma/libi420_rgb_mmx_plugin.so*
%_vlc_pluginsdir/video_chroma/libi420_ymga_mmx_plugin.so*
%_vlc_pluginsdir/video_chroma/libi420_yuy2_mmx_plugin.so*
%_vlc_pluginsdir/video_chroma/libi422_yuy2_mmx_plugin.so*
						
%dir %_vlc_pluginsdir/video_filter
%_vlc_pluginsdir/video_filter/libadjust_plugin.so*
%_vlc_pluginsdir/video_filter/libclone_plugin.so*
%_vlc_pluginsdir/video_filter/libcrop_plugin.so*
%_vlc_pluginsdir/video_filter/libdeinterlace_plugin.so*
#%_vlc_pluginsdir/video_filter/libdistort_plugin.so*
%_vlc_pluginsdir/video_filter/libinvert_plugin.so*
%_vlc_pluginsdir/video_filter/libmotionblur_plugin.so*
#%_vlc_pluginsdir/video_filter/libosdtext_plugin.so*
%_vlc_pluginsdir/video_filter/libtransform_plugin.so*
%_vlc_pluginsdir/video_filter/libwall_plugin.so*
%_vlc_pluginsdir/video_filter/libblend_plugin.so*
%_vlc_pluginsdir/video_filter/liblogo_plugin.so*
%_vlc_pluginsdir/video_filter/libmarq_plugin.so*
%_vlc_pluginsdir/video_filter/libmosaic_plugin.so*
%_vlc_pluginsdir/video_filter/libmotiondetect_plugin.so*
%_vlc_pluginsdir/video_filter/libosdmenu_plugin.so*
%_vlc_pluginsdir/video_filter/librss_plugin.so*
%_vlc_pluginsdir/video_filter/librv32_plugin.so*
%_vlc_pluginsdir/video_filter/libscale_plugin.so*
#%_vlc_pluginsdir/video_filter/libtime_plugin.so*
%_vlc_pluginsdir/video_filter/libmagnify_plugin.so*
%_vlc_pluginsdir/video_filter/libalphamask_plugin.so*
%_vlc_pluginsdir/video_filter/libbluescreen_plugin.so*
%_vlc_pluginsdir/video_filter/libcolorthres_plugin.so*
%_vlc_pluginsdir/video_filter/liberase_plugin.so*
%_vlc_pluginsdir/video_filter/libextract_plugin.so*
%_vlc_pluginsdir/video_filter/libgaussianblur_plugin.so*
%_vlc_pluginsdir/video_filter/libgradient_plugin.so*
%_vlc_pluginsdir/video_filter/libnoise_plugin.so*
%_vlc_pluginsdir/video_filter/libpanoramix_plugin.so*
%_vlc_pluginsdir/video_filter/libpsychedelic_plugin.so*
%_vlc_pluginsdir/video_filter/libpuzzle_plugin.so*
%_vlc_pluginsdir/video_filter/libripple_plugin.so*
%_vlc_pluginsdir/video_filter/librotate_plugin.so*
%_vlc_pluginsdir/video_filter/libsharpen_plugin.so*
%_vlc_pluginsdir/video_filter/libwave_plugin.so*

%dir %_vlc_pluginsdir/video_output

%dir %_vlc_pluginsdir/visualization
%_vlc_pluginsdir/visualization/libvisual_plugin.so*

%dir %_vlc_pluginsdir/meta_engine
%_vlc_pluginsdir/meta_engine/libfolder_plugin.so*

%doc AUTHORS README NEWS THANKS

%files interface-ncurses
%_vlc_pluginsdir/gui/libncurses_plugin.so*

%files interface-wxwidgets
%_vlc_pluginsdir/gui/libwxwidgets_plugin.so*
%_datadir/applications/vlc.desktop
%_bindir/wxvlc

%files interface-skins2
%_vlc_pluginsdir/gui/libskins2_plugin.so*
%_datadir/%name/skins2

%files interface-http
%_vlc_pluginsdir/control/libhttp_plugin.so*
%_datadir/%name/http

%files interface-telnet
%_vlc_pluginsdir/control/libtelnet_plugin.so*

%files interface-lirc
%_vlc_pluginsdir/control/liblirc_plugin.so*

%files interface-qt4
%_bindir/qvlc
%_vlc_pluginsdir/gui/libqt4_plugin.so*

%files plugin-sdl
%_vlc_pluginsdir/audio_output/libaout_sdl_plugin.so*
%_vlc_pluginsdir/video_output/libvout_sdl_plugin.so*

%files plugin-jack
%_vlc_pluginsdir/audio_output/libjack_plugin.so*
%_vlc_pluginsdir/access/libaccess_jack_plugin.so*

%files plugin-snapshot
%_vlc_pluginsdir/video_output/libsnapshot_plugin.so*

%if_enabled ggi
%files plugin-ggi
%_vlc_pluginsdir/video_output/libggi_plugin.so*
%endif

%files plugin-goom
%_vlc_pluginsdir/visualization/libgoom_plugin.so*

%files plugin-v4l
%_vlc_pluginsdir/access/libv4l_plugin.so*

%files plugin-live555
%_vlc_pluginsdir/demux/liblive555_plugin.so*

%ifnarch x86_64
%files plugin-loader
%_vlc_pluginsdir/codec/libdmo_plugin.so*
%_vlc_pluginsdir/codec/librealaudio_plugin.so*
%endif

%files plugin-osd
%_vlc_pluginsdir/misc/libxosd_plugin.so*

%files plugin-mad
%_vlc_pluginsdir/audio_filter/libmpgatofixed32_plugin.so*
#%_vlc_pluginsdir/demux/libid3tag_plugin.so*

%files plugin-matroska
%_vlc_pluginsdir/demux/libmkv_plugin.so*

%files plugin-mga
%_vlc_pluginsdir/video_output/libmga_plugin.so*

%files plugin-modplug
%_vlc_pluginsdir/demux/libmod_plugin.so*

%files plugin-mpeg2
%_vlc_pluginsdir/codec/liblibmpeg2_plugin.so*

%files plugin-musepack
%_vlc_pluginsdir/demux/libmpc_plugin.so*

%files plugin-notify
%_vlc_pluginsdir/misc/libnotify_plugin.so*

%files plugin-speex
%_vlc_pluginsdir/codec/libspeex_plugin.so*

%files plugin-ogg
%_vlc_pluginsdir/mux/libmux_ogg_plugin.so*
%_vlc_pluginsdir/demux/libogg_plugin.so*
%_vlc_pluginsdir/codec/libvorbis_plugin.so*

%files plugin-flac
%_vlc_pluginsdir/demux/libflacsys_plugin.so*
%_vlc_pluginsdir/codec/libflac_plugin.so*

%files plugin-a52
%_vlc_pluginsdir/audio_filter/liba52tofloat32_plugin.so*
%_vlc_pluginsdir/audio_filter/liba52tospdif_plugin.so*

%files plugin-h264
%_vlc_pluginsdir/codec/libx264_plugin.so*
%_vlc_pluginsdir/packetizer/libpacketizer_h264_plugin.so*

%files plugin-hal
%_vlc_pluginsdir/services_discovery/libhal_plugin.so*

%files plugin-bonjour
%_vlc_pluginsdir/services_discovery/libbonjour_plugin.so*

%files plugin-aa
%_vlc_pluginsdir/video_output/libaa_plugin.so*

%files plugin-caca
%_vlc_pluginsdir/video_output/libcaca_plugin.so*

%files plugin-image
%_vlc_pluginsdir/video_output/libimage_plugin.so*

%files plugin-opengl
%_vlc_pluginsdir/video_output/libopengl_plugin.so*

%files plugin-theora
%_vlc_pluginsdir/codec/libtheora_plugin.so*

%files plugin-glx
%_vlc_pluginsdir/video_output/libglx_plugin.so*

%files plugin-esd
%_vlc_pluginsdir/audio_output/libesd_plugin.so*

%files plugin-arts
%_vlc_pluginsdir/audio_output/libarts_plugin.so*

%files plugin-faad
%_vlc_pluginsdir/codec/libfaad_plugin.so*

%files plugin-ffmpeg
%_vlc_pluginsdir/codec/libffmpeg_plugin.so*
#%_vlc_pluginsdir/stream_out/libstream_out_switcher_plugin.so*

%files plugin-framebuffer
%_vlc_pluginsdir/video_output/libfb_plugin.so*

%files plugin-alsa
%_vlc_pluginsdir/audio_output/libalsa_plugin.so*

%files plugin-oss
%_vlc_pluginsdir/audio_output/liboss_plugin.so*

%files plugin-shout
%_vlc_pluginsdir/access_output/libaccess_output_shout_plugin.so*
%_vlc_pluginsdir/services_discovery/libshout_plugin.so*

%files plugin-x11
%_vlc_pluginsdir/video_output/libx11_plugin.so*

%files plugin-xml
%_vlc_pluginsdir/misc/libxml_plugin.so*
%_vlc_pluginsdir/misc/libxtag_plugin.so*

%files plugin-xvideo
%_vlc_pluginsdir/video_output/libxvideo_plugin.so*

%files plugin-png
%_vlc_pluginsdir/codec/libpng_plugin.so*

%files plugin-podcast
%_vlc_pluginsdir/services_discovery/libpodcast_plugin.so*

%files plugin-realrtsp
%_vlc_pluginsdir/access/libaccess_realrtsp_plugin.so*

%files plugin-cmml
%_vlc_pluginsdir/codec/libcmml_plugin.so*

%files plugin-dv
%_vlc_pluginsdir/access/libdc1394_plugin.so*
%_vlc_pluginsdir/access/libaccess_dv_plugin.so*

%files plugin-ts
%_vlc_pluginsdir/mux/libmux_ts_plugin.so*
%_vlc_pluginsdir/demux/libts_plugin.so*

%files plugin-twolame
%_vlc_pluginsdir/codec/libtwolame_plugin.so*

%files plugin-dvb
%_vlc_pluginsdir/codec/libdvbsub_plugin.so*
%_vlc_pluginsdir/access/libdvb_plugin.so*

%files plugin-dvdnav
%_vlc_pluginsdir/access/libdvdnav_plugin.so*

%files plugin-dvdread
%_vlc_pluginsdir/access/libdvdread_plugin.so*

%if_enabled dirac
%files plugin-dirac
%_vlc_pluginsdir/codec/libdirac_plugin.so*
%endif

%if_enabled dca
%files plugin-dca
%_vlc_pluginsdir/audio_filter/libdtstofloat32_plugin.so*
%endif

%if_enabled gnomevfs
%files plugin-gnomevfs
%_vlc_pluginsdir/access/libaccess_gnomevfs_plugin.so*
%endif

%files plugin-freetype
%_vlc_pluginsdir/misc/libfreetype_plugin.so*

%files plugin-gnutls
%_vlc_pluginsdir/misc/libgnutls_plugin.so*

%if_enabled smb
%files plugin-smb
%_vlc_pluginsdir/access/libaccess_smb_plugin.so*
%endif

%files plugin-screen
%_vlc_pluginsdir/access/libscreen_plugin.so*

%files plugin-sdlimage
%_vlc_pluginsdir/codec/libsdl_image_plugin.so*

%if_enabled svg
%files plugin-svg
%_vlc_pluginsdir/misc/libsvg_plugin.so*
%endif

%files -n mozilla-plugin-vlc
%browser_plugins_path/*

%files plugin-videocd
%_vlc_pluginsdir/access/libvcd_plugin.so*
%_vlc_pluginsdir/codec/libsvcdsub_plugin.so*
%_vlc_pluginsdir/access/libvcdx_plugin.so*

%files plugin-audiocd
%_vlc_pluginsdir/access/libcdda_plugin.so*

%files -n lib%name
%_libdir/libvlc.so.*
%_libdir/libvlc-control.so.*

%files -n lib%name-devel
%_bindir/%name-config
%_includedir/*
%_libdir/libvlc.so
%_libdir/libvlc-control.so

%if_enabled python-bindings
%files -n python-module-vlc
%_bindir/vlcdebug.py
%python_sitelibdir/vlc.so
%endif

%files -n vim-plugin-vlc-syntax
%vim_syntax_dir/vlc.vim

%files -n fortunes-vlc
%_gamesdatadir/fortune/vlc*

%files maxi

%files normal

%changelog
* Wed May 30 2007 Pavlov Konstantin <thresh@altlinux.ru> 0.9.0-alt0.svn20348
- 20348 revision.
- Fixed packaging issues.

* Tue May 29 2007 Pavlov Konstantin <thresh@altlinux.ru> 0.9.0-alt0.svn20336
- 20336 revision of 0.9.0 trunk.

* Fri May 18 2007 Pavlov Konstantin <thresh@altlinux.ru> 0.9.0-alt0.svn20165
- First build of 0.9.0 trunk snapshot.

* Tue May 15 2007 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6b-alt2
- Fix mozilloids plugin installation.
- Backport ffmpeg.c from trunk: lots of new fourccs.
- Backport fix seeking in mkv from trunk.
- Backport atrac support in rm files from trunk.
- Some tweaks to WX interface.
- Backport RSS fixes from trunk.
- Build with dirac = 0.7

* Thu Apr 19 2007 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6b-alt1
- 0.8.6b release.

* Fri Mar 30 2007 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6a-alt5
- Backported dirac 0.6 support from trunk.
- Applied numerous fixes from 0.8.6-bugfix branch.

* Fri Mar 23 2007 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6a-alt4
- Revert patch for build with wxgtk-2.8. It sucks.

* Wed Feb 21 2007 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6a-alt3
- Added strict requires for libavcodec >= 0.5.0-alt1.svn8045
  to ffmpeg plugin due to dirac DE support.
- Removed dirac plugin.
- Added a patch fixing build with libflac8.
- Added a patch fixing build with wxgtk-2.8, BR changed accordingly.
- Fixed BuildRequires a bit.
- Added vlc-plugin-xml to vlc-normal subpackage (apparently, vlc stores
  playlist in xml-based format...).
- Made plugins require libvlc instead of vlc package - this could help
  using libvlc+plugins with bindings without vlc package. Even interface
  plugins now require libvlc, though i have no idea if you could use them
  via bindings. :)

* Sun Jan 21 2007 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6a-alt2
- Spec cleanup, we're VLC Media Player, not VideoLAN Client.
- Moved h264 demuxer to main vlc package.
- Made vlc package own %%_libdir/vlc/gui directory.
- Proper packaging of libvlc/libvlc-devel.
- Use glibc-kernheaders instead of linux-libc-headers.

* Sat Jan 06 2007 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6a-alt1
- 0.8.6a bugfix release (well i fixed the bug in -alt3, but anyway...).

* Wed Jan 03 2007 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt3
- Added fix for udp format string vulnerability (affects VCD/CDDAX modules)

* Wed Dec 27 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt2
- Rebuild with new dbus.

* Tue Dec 12 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt1
- 0.8.6 release.

* Fri Dec 01 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.18182
- 0.8.6-rc1.

* Thu Nov 30 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.18168
- 18168, yeah, it's almost test3, i swear!
- gnomevfs -> def_disable, it doesn't work at all.
- New plugin : dv/dc1394.

* Wed Nov 29 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.18153
- 18153, almost test3.

* Thu Nov 16 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.17817
- 17817 revision.

* Thu Nov 16 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.17797
- 17797 revision.
- 0.8.6-test2.

* Tue Nov 07 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.17528
- 17528 revision.

* Tue Oct 31 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.17389
- 17389 revision.

* Wed Oct 18 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.17132
- 17132, almost -test1.

* Thu Oct 05 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.16948
- 16948 revision.

* Tue Sep 26 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.16876
- 16876 revision.

* Mon Sep 18 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.16724
- 16724 revision of 0.8.6 stable branch.

* Sun Sep 03 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.16525
- 16525 revision.
- Removed patch10, patch13 as they moved upstream (theora, mux_ts -> plugins).
- Get rid of sed (ffmpeg was fixed).

* Fri Aug 25 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.16345
- 16345 revision.
- Moved FreeDesktop menu file to separate %%SOURCE1, modified it a bit.

* Mon Aug 07 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.16235
- 16235 revision.
- Added libtwolame-devel to BuildRequires.
- New plugin: twolame.
- Removed demux/ts from main vlc package.

* Sat Aug 05 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.16209
- 16209 revision.
- Use switch to on/off dirac plugin (now it's on).
- Stricted libdirac version to use (now it's 0.5.4-alt1).
- Stricted faad version to use (now it's 2.0-alt2.20040923).
- Stricted ffmpeg version to use (now it's 0.5.0-alt1.svn5790).
- probe_hal plugin goes to plugin-hal package.
- motion plugin -- punch your notebook to navigate thru playlist
  (only supported on some Thinkpad models, don't forget to modprobe hdaps).
- Made %%summary'ies for plugins more descriptive.
- Made %%summary'ies and %%description's for packages unified.
- Cleaned up %%description's.
- Enablind smb plugin.

* Tue Jun 20 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.15926
- 15926 revision.
- BuildRequires changes:
  + Added libnotify-devel, libdbus-glib-devel, libupnp-devel.
  + XOrg7ification.
- Not building upnp plugin due to current trunk playlist changes.
- Added Requires: vlc-interfaces-wxwidgets to vlc-interface-skins2.
- Added java bindings toggle, disabling by default, it is not ready yet.
- Added working python mediacontrol bindings toggle, disabling it by default.
- Live555 version bump.
- Live555 moved to plugins, added to -normal virtual package.
- Temporarily disabled smb plugin.

* Fri Jun 02 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.15786
- Unified provides system for interfaces plugins:
  each interface plugin now provides vlc-interface.
  each interface plugin now provides vlc-plugin-%%name.
- Added fortunes-vlc package.
- s/vlc-plugin-qt4/vlc-interface-qt4/ in maxi requires (thx legion@).
- Patch15 (qt4 fix) merged upstream, removing it.
- More requires/obsoletes in vlc-normal package.
- Added THANKS file to documentation.

* Tue May 30 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.15765
- 15765.
- Added libqt4-devel to buildreq, enabling by default, goes to separate 
  interface package, added patch to build qt4 interface.
- Lightened buildreq dependancy to liblive-devel.

* Sat May 06 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.15548
- 15548 (post 0.8.5-test4).
- Removed patch14 (mozilla paths) as it merged upstream.
- Removed patch5 (svg build) as it was fixed in upstream.
- Trying to build mozilla plugin without nspr4 xpcom etc stuff (patch15,
  merged upstream).
- Mosaic plugin linked with -lm (same fix in upstream).
- Moved ts_plugin from vlc package to plugin-ts with mux_ts, added
  plugin-ts to the list of the essential packages to run vlc on desktop.
- Renamed vlc-common to vlc-normal.
- Moved stream_out_switcher to ffmpeg plugin.
- Some spec cleanup.

* Fri Apr 28 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.15395
- 15395.

* Wed Apr 26 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.15364
- 15364.
- fixed mozilla plugin build.

* Sat Apr 22 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.6-alt0.15299
- 0.8.6 trunk.
- 15299 revision.
- Some spec cleanup.
- Renamed some patches.
- Removing kludgy *FLAGS.
- Enabling DAAP support thru libdaap, goes to separate plugin.
- Built with libdts -- enables DTS decoding, goes to separate plugin.
- Matroska, dts, bonjour (avahi), theora, mux_ts goes to plugins.
- Moving telnet and http interface to plugins.
- Fixed some unresolved symbols in some plugins (patch moved to upstream).
- Improved Xorg7 detection under ia32 (patch moved to upstream).
- added fonts-ttf-dejavu to freetype plugin requires as it uses hardcoded
font from dejavu by default (suggestions on default font are welcome).

* Tue Apr 04 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.15087
- 15087 (post test2).

* Sat Mar 25 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14911
- 14911.
- updated mozilla and wxnoupdates patches.
- to sisyphus.
- mmx memcpy and stuff moved to plugins from builtins and goes into main package.
- textrel = relaxed, unresolved = relaxed.

* Sun Mar 05 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14640
- 14640.
- compiling with linux-libc-headers.

* Sat Mar 04 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14613
- 14613.

* Sat Mar 04 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14602
- 14602
- same thing with svg, disabled it until fixed in upstream.

* Tue Feb 28 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14534
- 14534.
- added def_disable ggi and ifdeffed everything around it.

* Mon Feb 27 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14521
- 14521.

* Sun Feb 26 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14512
- 14512.
- added liblame devel to buildreq.

* Wed Feb 22 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14462
- 14462.
- enabling debug.

* Tue Feb 21 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14446
- 14446.

* Tue Feb 21 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14430
- 14430.

* Mon Feb 20 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14421
- 14421.

* Sun Feb 19 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14378
- 14378.
- fixed maxi/common packages.

* Fri Feb 17 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14350
- Added -maxi and -common packages for installation ease.

* Fri Feb 17 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14349
- 14349.
- deffed python bindings build. currently disabled it because of upstream changes.

* Thu Feb 16 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14333
- 14333.
- python patch edited.

* Mon Feb 06 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14182
- 14182.
- added ugly hack (tm) to enable building on 32bit systems.
- enabling quicktime and dmo only for 32bit.

* Fri Feb 03 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14148
- 14148.
- added jack plugin.
- added snapshot plugin.

* Fri Feb 03 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14146
- 14146.
- removed debian menu.

* Thu Feb 02 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14139
- 14139.
- skipping verify_elf.

* Tue Jan 31 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14090
- 14090
- added vim-plugin-vlc-syntax package.
- let's try to link with x264.
- removed --disable-rpath.
- added ./toolbox.

* Thu Jan 26 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14014
- 14014.
- removing configure patch.

* Tue Jan 24 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14009
- 14009.
- vcdx plugin.

* Mon Jan 23 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.14004
- svg fix build.

* Thu Jan 19 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.13949
- removed fix for xorg7, cause 13949 revision should check for xlibs better. ;)
- moved mpc to plugin-musepack.
- moved dvdread to plugin-dvdread.
- moved dvdnav to plugin-dvdnav.
- moved ffmpeg to plugin-ffmpeg.
- moved mpeg2 to plugin-mpeg2.
- moved modplug to plugin-modplug.
- moved ogg muxer to plugin-ogg.

* Thu Jan 19 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.13948
- revision 13948.
- fixed build with xorg7.0.

* Sat Jan 14 2006 Pavlov Konstantin <thresh@altlinux.ru> 0.8.5-alt0.13911
- svn trunk.
- new versioning.
- added wxnoupdates patch and osdmenu patch.
- moved all interface plugins to interface-* packets.
- added provides vlc-interface to all those packets.
- added .desktop file.
- removed broken documentation.
- Hopefully fixed xvideo on x86_64.
- Add requires libx264-devel-static.
- fixed mozilla plugin build (hello, new world with new mozilla-devel & co!).
- added goom plugin.
- removed java bindings as they doesn't even install. :(
- enabling python bindings.
- added podcast module.
- sed-i'ng modules path.
- using ./bootstrap.

* Tue Dec 13 2005 Pavlov Konstantin <thresh@altlinux.ru> 0.8.4a-alt0.8
- 0.8.4a release.
- altered buildreqs.
- enabled hal.

* Sat Dec 10 2005 Pavlov Konstantin <thresh@altlinux.ru> 0.8.4-alt0.77
- altered ffmpeg buildreq.
- remove vlc requires in mozilla-plugin-vlc package.

* Sat Dec 10 2005 Pavlov Konstantin <thresh@altlinux.ru> 0.8.4-alt0.76
- buildreqs cleanup.

* Sat Dec 10 2005 Pavlov Konstantin <thresh@altlinux.ru> 0.8.4-alt0.75
- Added LIVE555.com (formerly live.com, blame MS!) support for VLC to support RTSP.
- hal doesn't build on x86, let's %def_disable it.

* Thu Dec 08 2005 Pavlov Konstantin <thresh@altlinux.ru> 0.8.4-alt0.71
- added librsvg2-devel to buildreqs.

* Wed Dec 07 2005 Pavlov Konstantin <thresh@altlinux.ru> 0.8.4-alt0.6
- 0.6 version of spec.
- all the plugins are packaged, some of them (~40) are in the separate packages.
- xvideo plugin doesn't build on x86_64.

* Thu Jan 20 2005 ALT QA Team Robot <qa-robot@altlinux.org> 0.7.2-alt0.5.1.1
- Rebuilt with libstdc++.so.6.

* Tue Oct 05 2004 ALT QA Team Robot <qa-robot@altlinux.org> 0.7.2-alt0.5.1
- Rebuilt with libdvdread.so.3.

* Mon May 31 2004 Yuri N. Sedunov <aris@altlinux.ru> 0.7.2-alt0.5
- 0.7.2

* Thu Apr 15 2004 Yuri N. Sedunov <aris@altlinux.ru> 0.7.1-alt0.2
- 0.7.1

* Mon Jan 05 2004 Yuri N. Sedunov <aris@altlinux.ru> 0.7.0-alt0.2
- 0.7.0

* Thu Apr 10 2003 Yuri N. Sedunov <aris@altlinux.ru> 0.5.3-alt0.5
- 0.5.3

* Tue Feb 04 2003 Yuri N. Sedunov <aris@altlinux.ru> 0.5.0-alt0.5
- 0.5.0

* Sat Nov 16 2002 Yuri N. Sedunov <aris@altlinux.ru> 0.4.6-alt0.5
- 0.4.6

* Fri Nov 01 2002 Yuri N. Sedunov <aris@altlinux.ru> 0.4.5-alt0.5
- 0.4.5

* Fri Sep 13 2002 Yuri N. Sedunov <aris@altlinux.ru> 0.4.4-alt0.5
- 0.4.4

* Sat Jul 27 2002 Yuri N. Sedunov <aris@altlinux.ru> 0.4.3-alt0.5
- 0.4.3

* Thu Jul 11 2002 Yuri N. Sedunov <aris@altlinux.ru> 0.4.2-alt0.5
- 0.4.2
- built with libffmpeg-0.4.6-alt0.2cvs20020721

* Wed Jun 05 2002 Yuri N. Sedunov <aris@altlinux.ru> 0.4.1-alt0.5
- 0.4.1
- built with libffmpeg-0.4.6-alt0.1cvs20020605
- configure.in patch removed.

* Fri May 24 2002 Yuri N. Sedunov <aris@altlinux.ru> 0.4-alt0.5
- Adopted for Sisyphus.

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
  * removed the workaround for VLC's bad /dev/dsp detection.
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
  interface for Gnome-impaired, an even better DVD support

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
