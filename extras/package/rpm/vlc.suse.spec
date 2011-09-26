Summary: vlc - Video Lan Client
Name: 	vlc-beta
Version: 	0.9.0
Release: 	20395.0
Source: 	%{name}-%{version}.tar.bz2
Packager: 	Dominique Leuenberger <dominique-rpm@leuenberger.net>
License: 	GPL
Group: Productivity/Multimedia/Video/Players	
BuildRoot:  %{_tmppath}/%{name}-%{version}-build
BuildRequires: libdvdnav-devel gettext-devel libvorbis-devel libogg-devel libtheora-devel
BuildRequires: cvs gnome-vfs2-devel libcdio-devel libdvdread-devel libcddb-devel gnutls-devel alsa-devel
BuildRequires: xosd-devel aalib-devel gcc-c++ vcdimager-devel xvidcore-devel freetype2-devel slang-devel
BuildRequires: libqt4-devel
BuildRequires: x264-devel ffmpeg-devel libmad-devel libmpeg2-devel faad2-devel faac-devel libdca-devel a52dec-devel libdvbpsi-devel live555
%if %suse_version >= 1010
BuildRequires: avahi-devel libnotify-devel
%endif




%if %suse_version >= 1010
BuildRequires: Mesa-devel
%else
BuildRequires: xorg-x11-Mesa xorg-x11-Mesa-devel
%endif

# The requirements for the Mozilla-Plugin (--enable-mozilla)
# unfortunately, the mozilla-devel get's changed and renamed all the time. So 
# this gave a complete if endif structure.
# for the releases 10.2 and 10.3, xulrunner provides gecko-sdk
%if %suse_version <= 1000
BuildRequires: mozilla-devel
%endif
%if %suse_version >= 1010
BuildRequires: gecko-sdk
%endif

Requires: x264 faac faad2 libmad ffmpeg a52dec libdca xvidcore libdvdcss

%package mozillaplugin
Summary: enables VLC inside Mozilla Browser
Group: Productivity/Multimedia/Video/Players
Requires: %{name} = %{version}

%description mozillaplugin
With this plugin, you enable video content withing the Mozilla Browser Suites

%description
VLC media player is a highly portable multimedia player for various 
audio and video formats (MPEG-1, MPEG-2, MPEG-4, DivX, mp3, ogg, ...) 
as well as DVDs, VCDs, and various streaming protocols. 
It can also be used as a server to stream in unicast or multicast 
in IPv4 or IPv6 on a high-bandwidth network.

%prep
%setup -q


%debug_package
%build 
%if %suse_version <= 1000
export XPIDL=/opt/mozilla/%{_lib}/xpidl
export PATH=${PATH}:/opt/mozilla/bin
export XPIDL_INCL=$( mozilla-config --idlflags )
%endif


#paths for xpidl on SUSE 10.1 and later
%if %suse_version >= 1010
export XPIDL=%{_libdir}/xulrunner-$(xulrunner-config --version)/xpidl
export XPIDL_INCL=$(xulrunner-config --idlflags)
%endif

./bootstrap
./configure \
   --prefix=%{_prefix} \
   --libdir=%{_libdir} \
   --enable-skins2 \
   --disable-pda \
   --disable-macosx \
   --disable-qnx \
   --enable-ncurses \
   --enable-xosd \
   --enable-visual \
   --disable-goom \
   --enable-slp \
   --enable-lirc \
   --disable-joystick \
   --disable-corba \
   --enable-livedotcom \
   --enable-dvdread \
   --enable-dvdnav \
   --disable-dshow \
   --enable-v4l \
   --enable-pvr \
   --enable-vcd \
   --enable-satellite \
   --enable-ogg \
   --enable-mkv \
   --enable-mod \
   --enable-libcdio \
   --enable-vcdx \
   --enable-cddax \
   --enable-libcddb \
   --enable-x11 \
   --enable-xvideo \
   --enable-glx \
   --enable-fb \
   --enable-mga \
   --enable-freetype \
   --enable-fribidi \
   --disable-svg \
   --disable-hd1000v \
   --disable-directx \
   --disable-wingdi \
   --disable-glide \
   --enable-aa \
   --disable-caca \
   --enable-oss \
   --disable-esd \
   --enable-arts \
   --enable-waveout \
   --disable-coreaudio \
   --disable-hd1000a \
   --enable-mad \
   --enable-ffmpeg \
   --enable-faad \
   --enable-a52 \
   --enable-dca \
   --enable-flac \
   --enable-libmpeg2 \
   --enable-vorbis \
   --enable-tremor \
   --enable-speex \
   --disable-tarkin \
   --enable-theora \
   --enable-cmml \
   --enable-utf8 \
   --enable-pth \
   --enable-st \
   --disable-gprof \
   --disable-cprof \
   --disable-testsuite \
   --enable-optimizations \
   --disable-altivec \
   --disable-debug \
   --enable-sout \
   --with-ffmpeg-faac \
   --enable-httpd \
   --disable-jack \
   --enable-mozilla \
   --enable-alsa \
   --enable-real \
   --enable-realrtsp \
   --enable-live555 \
   --with-live555-tree=%{_libdir}/live \
   --enable-dvbpsi
#   --enable-dvb \
#   --with-ffmpeg-mp3lame \
#   --enable-quicktime\ 
#   --enable-sdl \
#   --enable-ggi \
#   --enable-svgalib \
 

make %{?jobs:-j %jobs}


%install
make DESTDIR=%{buildroot} install
mkdir -p %{buildroot}/%{_datadir}/pixmaps
ln -s %{_datadir}/vlc/vlc48x48.png %{buildroot}/%{_datadir}/pixmaps/vlc.png
%if %suse_version <= 1000
export PATH=${PATH}:/opt/mozilla/bin
mkdir -p %{buildroot}/opt/mozilla/%{_lib}/plugins
mv %{buildroot}%{_libdir}/mozilla/plugins/libvlc* %{buildroot}/opt/mozilla/%{_lib}/plugins
%else
mkdir -p %{buildroot}/%{_libdir}/browser-plugins
mv %{buildroot}%{_libdir}/mozilla/plugins/libvlc* %{buildroot}/%{_libdir}/browser-plugins
%endif

%clean
rm -rf "$RPM_BUILD_ROOT"


%files
%defattr(-,root,root)
%doc %{_datadir}/doc/vlc/
%doc NEWS AUTHORS COPYING HACKING THANKS README ChangeLog
%{_datadir}/vlc/
%{_bindir}/*vlc
%{_bindir}/vlc-config
%{_includedir}/vlc/
%{_libdir}/vlc/
#%{_libdir}/libvlc.a
%{_datadir}/applications/vlc.desktop
%{_datadir}/pixmaps/vlc.png
%{_datadir}/locale/
%{_libdir}/libvlc-control.so.0
%{_libdir}/libvlc-control.so.0.0.0
%{_libdir}/libvlc.so.1
%{_libdir}/libvlc.so.1.0.0
%{_libdir}/browser-plugins/libvlcplugin.la
%{_libdir}/libvlc-control.la
%{_libdir}/libvlc-control.so
%{_libdir}/libvlc.la
%{_libdir}/libvlc.so


%files mozillaplugin
%if %suse_version <= 1000
/opt/mozilla/%{_lib}/plugins/libvlc*
%else
/usr/%{_lib}/browser-plugins/libvlcplugin.so
%endif

%changelog
* Sat May 19 2007 - Dominique Leuenberger <dominique-vlc.suse@leuenberger.net>
- Updated to SVN Version 20199
* Fri Apr 13 2007 - Dominique Leuenberger <dominique-vlc.suse@leuenberger.net>
- Added support for Theora Video Files
* Mon Apr 02 2007 - Dominique Leuenberger <dominique-vlc.suse@leuenberger.net>
- initial build of 0.9, named as beta
- disable wxGTK interface
- enable Qt4 interface
* Thu Jan 18 2007 - Dominique Leuenberger <dominique-vlc.suse@leuenberger.net>
- added live555 support
- added --enable-real and --enable-realrtsp to the configure script
* Thu Jan 4 2007 - Dominique Leuenberger <dominique-vlc.suse@leuenberger.net>
- added --enable-dvbpsi to support streaming video
* Wed Jan 3 2007 - Dominique Leuenberger <dominique-vlc.suse@leuenberger.net>
- Upgrade to Version 0.8.6a
- security fix for cdda & vcdx, VideoLAN-SA0701
* Sun Dec 10 2006 - Dominique Leuenberger <dominique-vlc.suse@leuenberger.net>
- upgraded to public release version 0.8.6
* Sun Dec 3 2006 - Dominique Leuenberger <dominique-rpm@leuenberger.net>
- Fixed group memberships for Yast tools
- First public released package
* Fri Oct 20 2006 - Dominique Leuenberger <dominique-rpm@leuenberger.net>
- Initial internal release of v0.8.6
