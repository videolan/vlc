# TODO: daap, goom, libdc1394, libggi, java-vlc.
#
%{!?python_sitearch: %define python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(1)")}


%define with_static_ffmpeg 		1
%define ffmpeg_date	20070503
%define with_static_live555 		0
%define live555_date	2007.04.24a
%define vlc_svn				0
%define vlc_date    20070514
%define with_dirac	 		1
%define with_mozilla	 		1
%define with_java_vlc	 		0
%define rpmfusion			0


Summary:	Multi-platform MPEG, DVD, and DivX player
Name:		vlc
%if %vlc_svn
%define release_tag   0.1
%define _version %{version}-svn
Version:	0.9.0
Release:	%{release_tag}.%{vlc_date}svn%{?dist}
%else
Version:	0.8.6b
%define release_tag   5
%define _version %{version}
Release:	%{release_tag}%{?dist}.2
%endif
License:	GPL
Group:		Applications/Multimedia
URL:		http://www.videolan.org/
%if %vlc_svn	
Source0:        http://nightlies.videolan.org/build/source/vlc-snapshot-%{vlc_date}.tar.bz2
%else
Source0:	http://download.videolan.org/pub/videolan/vlc/%{version}/vlc-%{version}.tar.bz2
## Bugfix sources...
#Source0:	http://nightlies.videolan.org/build/source/vlc-snapshot-branch-0.8.6-bugfix-%{vlc_date}.tar.gz
%endif
%if %with_static_ffmpeg
Source1:	http://rpm.greysector.net/livna/ffmpeg-%{ffmpeg_date}.tar.bz2
%endif
%if %with_static_live555
Source2:	http://www.live555.com/liveMedia/public/live.%{live555_date}.tar.gz
%endif
Patch0: 	vlc-0.8.6-ffmpegX11.patch
Patch1: 	vlc-0.8.6-wx28.patch
Patch2: 	vlc-0.8.6a-faad2.patch
Patch3:		vlc-0.8.6a-automake110.patch
Patch4:		vlc-0.8.6-FLAC_api_change.patch
Patch6:		vlc-trunk-dirac_0_6_0-api.patch
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

BuildRequires:	automake
BuildRequires:	gettext-devel
BuildRequires:	desktop-file-utils
BuildRequires:	libtool

BuildRequires:	a52dec-devel
BuildRequires:	aalib-devel
BuildRequires:	alsa-lib-devel
BuildRequires:	arts-devel
BuildRequires:	avahi-devel
BuildRequires:  cdparanoia-devel
# Don't work now wip
#BuildRequires:  directfb-devel
BuildRequires:	esound-devel
BuildRequires:	faac-devel
BuildRequires:	faad2-devel < 2.5
BuildRequires:	flac-devel
BuildRequires:	fribidi-devel
# wip -  glide.h usability... no
#BuildRequires:  Glide3-devel
#BuildRequires:  Glide3-libGL
BuildRequires:  gnome-vfs2-devel
BuildRequires:	gnutls-devel >= 1.0.17
BuildRequires:	gsm-devel
BuildRequires:  gtk2-devel
BuildRequires:	hal-devel
BuildRequires:	jack-audio-connection-kit-devel
BuildRequires:	lame-devel
BuildRequires:  libavc1394-devel
BuildRequires:	libcaca-devel
BuildRequires:	libcddb-devel
BuildRequires:	libcdio-devel >= 0.77-3
# kwizart this is the same issue with cdio and cddax svcdx configure options.
# http://bugzilla.livna.org/show_bug.cgi?id=1342 or see
# http://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=221359
BuildConflicts:	libcdio-devel = 0.78.2	
BuildRequires:	libdca-devel
BuildRequires:	libdv-devel
BuildRequires:	libdvbpsi-devel
BuildRequires:	libdvdnav-devel
BuildRequires:  libebml-devel
BuildRequires:	libid3tag-devel
BuildRequires:	libGLU-devel
BuildRequires:	libmad-devel
BuildRequires:	libmatroska-devel >= 0.7.6
BuildRequires:	libmodplug-devel
BuildRequires:	libmpcdec-devel
BuildRequires:  libnotify-devel
BuildRequires:	libpng-devel
BuildRequires:  libraw1394-devel
BuildRequires:	librsvg2-devel >= 2.5.0
BuildRequires:	libsysfs-devel
BuildRequires:  libshout-devel
BuildRequires:	libtar-devel
BuildRequires:	libtheora-devel
BuildRequires:  libtiff-devel
BuildRequires:  libupnp-devel
BuildRequires:	libvorbis-devel
BuildRequires:  libxml2-devel
BuildRequires:	lirc-devel
%if %with_static_live555 
BuildConflicts: live-devel
%else
BuildRequires:	live-devel >= 0-0.11.2006.08.07
%endif
BuildRequires:	mpeg2dec-devel >= 0.3.2
BuildRequires:	ncurses-devel
BuildRequires:	openslp-devel
BuildRequires:  ORBit2-devel
# This Seem Broken
#BuildRequires:	portaudio
BuildRequires:	pth-devel
BuildRequires:	python-devel
BuildRequires:  pyorbit-devel
BuildRequires:	SDL_image-devel
BuildRequires:	speex-devel >= 1.1.5
%ifarch %{ix86} x86_64
BuildRequires:  svgalib-devel
%endif
BuildRequires:	twolame-devel
BuildRequires:	vcdimager-devel >= 0.7.21
BuildRequires:	wxGTK-devel >= 2.6
BuildRequires:	x264-devel >= 0-0.8.20061028
BuildRequires:	xosd-devel
BuildRequires:	xvidcore-devel
BuildRequires:	zlib-devel

# X-libs
BuildRequires:	libXt-devel
BuildRequires:	libXv-devel
BuildRequires:  libXxf86vm-devel

%if "%fedora" > "6"
BuildRequires: libsmbclient-devel
%else 
BuildRequires: samba-common
%endif

%if %with_mozilla
BuildRequires:	firefox-devel >= 1.5.0.0
## Will be later replaced by 
#BuildRequires:  xulrunner-devel
##
BuildRequires:	nspr-devel
BuildRequires:	nss-devel
BuildRequires:	js-devel
%endif

%if %with_static_ffmpeg
## Static version already bundle it
BuildConflicts:	ffmpeg-devel
%else
BuildRequires:	ffmpeg-devel >= 0.4.9-0
%endif

%if %with_dirac
# Diract is still experimental in vlc - 0.6.0 is now in Fedora
# Review http://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=221405
BuildRequires:	dirac-devel >= 0.6.0
%endif

%if %with_java_vlc
BuildRequires:	libgcj-devel
%endif

%if %vlc_svn
BuildRequires:  opencv-devel
BuildRequires:  qt4-devel
BuildRequires:  dbus-devel
BuildRequires:  xorg-x11-proto-devel
#BuildRequires:  lua-devel
%endif

%if %rpmfusion
BuildRequires:  libopendaap-devel
BuildRequires:  libgoom2-devel
BuildRequires:  libdc1394-devel
BuildRequires:  libggi-devel
%endif


Provides:	videolan-client = %{version}-%{release}
Provides:	videolan-client-wx = %{version}-%{release}
Provides:	videolan-client-ncurses = %{version}-%{release}
Obsoletes:	videolan-client < 0.8.1
#Obsoletes:	videolan-client-gnome < 0.8.1
#Obsoletes:	videolan-client-kde < 0.8.1
Obsoletes:	videolan-client-ncurses < 0.8.1
Obsoletes:	videolan-client-wx < 0.8.1

%package devel
Summary:	Development package for %{name}
Group:		Development/Libraries
Requires:	%{name} = %{version}-%{release}
Requires:       %{name} = %{version}-%{release}
Provides:	videolan-client-devel = %{version}-%{release}
Obsoletes:	videolan-client-devel < 0.8.1

%description
VLC (initially VideoLAN Client) is a highly portable multimedia player
for various audio and video formats (MPEG-1, MPEG-2, MPEG-4, DivX,
mp3, ogg, ...) as well as DVDs, VCDs, and various streaming protocols.
It can also be used as a server to stream in unicast or multicast in
IPv4 or IPv6 on a high-bandwidth network.

%description devel
This package contains development files for VLC Media Player.

VLC (initially VideoLAN Client) is a highly portable multimedia player
for various audio and video formats (MPEG-1, MPEG-2, MPEG-4, DivX,
mp3, ogg, ...) as well as DVDs, VCDs, and various streaming protocols.
It can also be used as a server to stream in unicast or multicast in
IPv4 or IPv6 on a high-bandwidth network.


%package -n python-vlc
Summary:	VLC Media Player binding for Python
Group:		Applications/Multimedia	
Requires:	%{name} = %{version}-%{release}
Requires:       pyorbit

%description -n python-vlc
VLC Media Player binding for Python 


%if %with_mozilla
%package -n mozilla-vlc
Summary:	VLC Media Player plugin for Mozilla compatible web browsers
Group:		Applications/Multimedia	
Requires:	%{name} = %{version}-%{release}
Requires:	%{_libdir}/mozilla/plugins
Provides:	videolan-client-mozilla = %{version}-%{release}
Obsoletes:	videolan-client-mozilla < 0.8.1

%description -n mozilla-vlc
This package contains a VLC Media Player plugin for Mozilla compatible
web browsers.

VLC (initially VideoLAN Client) is a highly portable multimedia player
for various audio and video formats (MPEG-1, MPEG-2, MPEG-4, DivX,
mp3, ogg, ...) as well as DVDs, VCDs, and various streaming protocols.
It can also be used as a server to stream in unicast or multicast in
IPv4 or IPv6 on a high-bandwidth network.
%endif

%if %with_java_vlc
%package -n java-vlc
Summary:	VLC Media Player binding for Java
Group:		Applications/Multimedia	
Requires:	%{name} = %{version}-%{release}

%description -n java-vlc
VLC Media Player binding for Java
%endif


%prep
%setup -q -n %{name}-%{_version}
%if %with_static_ffmpeg
%setup -q -D -T -a 1 -n %{name}-%{_version}
%endif
%if %with_static_live555
%setup -q -D -T -a 2 -n %{name}-%{_version}
%endif

%patch0 -p1 -b .ffmpegX11
%patch1 -p1 -b .wx28
%if %vlc_svn
#Xvmc quick fix on svn and AMD64
sed -i 's|pop |popl |g' modules/codec/xvmc/*
sed -i 's|push |pushl |g' modules/codec/xvmc/*
%else
%patch2 -p1 -b .faad2
%patch3 -p0 -b .automake110
%patch4 -p1 -b .FLAC_api
%patch6 -p0 -b .dirac6
%endif

%{__perl} -pi -e \
's|/usr/share/fonts/truetype/freefont/FreeSerifBold\.ttf|%{_datadir}/fonts/bitstream-vera/VeraSeBd.ttf|' \
modules/misc/freetype.c
# Fix PLUGIN_PATH path for lib64
%{__perl} -pi -e 's|/lib/vlc|/%{_lib}/vlc|g' vlc-config.in.in configure*
# Fix perms issues
chmod 644 mozilla/control/*
chmod 644 src/control/log.c
sed -i 's/\r//'  mozilla/control/* 

sh bootstrap



%build
%if %with_static_ffmpeg
export CFLAGS="%{optflags}"
# Build bundeled ffmpeg first
pushd ffmpeg-%{ffmpeg_date}
./configure \
	--extra-cflags="-fPIC -DPIC" \
	--enable-static \
	--disable-shared \
	--enable-libmp3lame \
	--enable-libfaac \
	--enable-pp \
	--enable-gpl \
%if %vlc_svn
	--enable-swscaler \
%endif
# Watch http://trac.videolan.org/vlc/ticket/865
# Planned to be enabled for 0.9.x

	make %{?_smp_mflags}
popd
%endif

%if %with_static_live555
# Then bundled live555 - not needed
pushd live
# Force the use of our CFLAGS
%{__perl} -pi -e 's|-O2|%{optflags} -fPIC -DPIC|g' config.linux
# Configure and build
./genMakefiles linux && make
popd
%endif


%if %with_mozilla
# fix mozilla plugin
export XPIDL="$(rpm -ql firefox-devel.%{_target_cpu}|grep '/xpidl$')"
export MOZVER="$(rpm -q --qf=%{VERSION} firefox-devel.%{_target_cpu})"
%endif
export CFLAGS="%{optflags} -fPIC"
export CXXFLAGS="%{optflags} -fPIC"


# Altivec compiler flags aren't set properly (0.8.2)
%ifarch ppc ppc64
export CFLAGS="$CFLAGS -maltivec -mabi=altivec"
%endif

# java-vlc
%if %with_java_vlc
export JAVA_HOME=%{_prefix}/lib/jvm/java
%endif

# cddax & vcdx : doesn't build, at least on fc6 x86_64
# with libcdio > 0.77-3
%configure \
	--disable-dependency-tracking		\
	--disable-rpath				\
	--enable-shout				\
	--enable-release			\
	--enable-live555 			\
%if %with_static_live555 
	--with-live555-tree=live		\
%endif
%if %rpmfusion
	--enable-dc1394				\
	--enable-dv				\
%endif
	--enable-ffmpeg --with-ffmpeg-mp3lame --with-ffmpeg-faac \
%if %with_static_ffmpeg 
 	--with-ffmpeg-tree=ffmpeg-%{ffmpeg_date} \
%endif
	--disable-libtool 			\
	--with-gnu-ld				\
	--disable-static			\
	--enable-shared				\
 	--disable-pth				\
	--enable-dvdread			\
	--enable-v4l				\
	--enable-pvr				\
	--enable-libcdio			\
%ifarch x86_64
	--enable-cddax 				\
%endif
	--enable-vcdx				\
	--enable-dvb				\
	--enable-faad				\
	--enable-twolame			\
	--enable-real				\
	--enable-realrtsp			\
	--enable-flac				\
	--enable-tremor				\
	--enable-speex				\
	--enable-tarkin				\
	--enable-theora				\
%if %with_dirac 
	--enable-dirac				\
%endif 
	--enable-svg				\
	--enable-snapshot			\
%ifarch %{ix86} x86_64
	--enable-svgalib			\
%endif
	--enable-aa				\
	--enable-caca				\
	--enable-esd				\
	--enable-arts				\
	--enable-jack				\
	--enable-ncurses			\
	--enable-xosd				\
%if %rpmfusion
	--enable-goom				\
	--enable-ggi				\
%endif
	--enable-slp				\
	--enable-lirc				\
	--enable-corba				\
	--with-x				\
	--enable-mediacontrol-python-bindings	\
%if %with_java_vlc
	--enable-java-bindings			\
%endif
%ifarch %{ix86}
	--enable-loader				\
%endif
	--without-contrib			\
%if %with_mozilla 
	--enable-mozilla			\
%endif
	--with-x264-tree=%{_includedir}		\
%if %vlc_svn
	--enable-libtool 			\
	--enable-shared				\
	--disable-static			\
	--enable-opencv				\
	--enable-python-bindings		\
	--disable-switcher			\
	--disable-libcdio			\
	--disable-cddax 			\
	--disable-vcdx				\
	--disable-audioscrobbler		\
	--disable-musicbrainz			\
	--enable-taglib				\
	--enable-dbus-control			\
	--enable-qt4				\
	--disable-xvmc				\
%endif


## temp disabled
# --enable-cyberlink				\
# --enable-qte					\
# --enable-ggi					\
# --enable-quicktime				\
# --with-wine-sdk-path=%{_includedir}/wine/windows	\
# 	--enable-directfb			\
#	--with-directfb="no" 			\
# 	--enable-glide				\
#	--with-glide=%{_includedir}/glide3	\


%if %vlc_svn
sed -i -e 's|python $(srcdir)/setup.py install|python $(srcdir)/setup.py install --root $(DESTDIR)|' bindings/python/Makefile
%else
sed -i -e 's|python $(srcdir)/setup.py install|python $(srcdir)/setup.py install --root $(DESTDIR)|' bindings/mediacontrol-python/Makefile
%endif
sed -i -e 's|cflags="${cflags} -I/usr/include/ffmpeg"|cflags="${cflags} -I%{_includedir}/ffmpeg -I%{_includedir}/postproc/"|' vlc-config

%if %with_mozilla
	make %{?_smp_mflags} XPIDL_INCL="-I%{_datadir}/idl/firefox-$MOZVER/" 
%else
	make %{?_smp_mflags}
%endif

%install
rm -rf $RPM_BUILD_ROOT __doc

make install DESTDIR=$RPM_BUILD_ROOT
find $RPM_BUILD_ROOT -name '*.la' -exec rm -f {} ';'
mv $RPM_BUILD_ROOT%{_datadir}/doc/vlc __doc

%if %with_mozilla 
chmod +x $RPM_BUILD_ROOT%{_libdir}/mozilla/plugins/libvlcplugin.so
%endif

install -dm 755 $RPM_BUILD_ROOT%{_mandir}/man1
install -pm 644 doc/vlc*.1 $RPM_BUILD_ROOT%{_mandir}/man1

install -dm 755 $RPM_BUILD_ROOT%{_datadir}/icons/hicolor/48x48/apps
ln -s ../../../../vlc/vlc48x48.png \
	$RPM_BUILD_ROOT%{_datadir}/icons/hicolor/48x48/apps/vlc.png

sed -i -e 's|Version=|Version=%{version}\n#Version=|' $RPM_BUILD_ROOT%{_datadir}/applications/vlc.desktop
desktop-file-install --vendor livna			\
	--dir $RPM_BUILD_ROOT%{_datadir}/applications	\
	--delete-original				\
	--mode 644					\
	$RPM_BUILD_ROOT%{_datadir}/applications/vlc.desktop

%if %vlc_svn
%else
# Fix perms of python things
chmod 755 $RPM_BUILD_ROOT%{_bindir}/vlcwrapper.py
chmod 755 $RPM_BUILD_ROOT%{_libdir}/advene/MediaControl.so
# Fix python shebang
sed -i -e 's|"""Wrapper|#!/usr/bin/python\n"""Wrapper|' $RPM_BUILD_ROOT%{_bindir}/vlcwrapper.py
%endif

%find_lang %{name}


%clean
rm -rf $RPM_BUILD_ROOT __doc


%post
%{_bindir}/gtk-update-icon-cache -qf %{_datadir}/icons/hicolor &>/dev/null
%{_bindir}/update-desktop-database %{_datadir}/applications > /dev/null 2>&1
/sbin/ldconfig || :


%postun
%{_bindir}/update-desktop-database %{_datadir}/applications > /dev/null 2>&1
%{_bindir}/gtk-update-icon-cache -qf %{_datadir}/icons/hicolor &>/dev/null
/sbin/ldconfig || :


%files -f %{name}.lang
%defattr(-,root,root,-)
%doc AUTHORS COPYING ChangeLog NEWS README THANKS __doc/*
%{_bindir}/vlc
%{_bindir}/svlc
%{_datadir}/vlc/
%{_mandir}/man1/vlc.1*
%{_datadir}/applications/*%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/vlc.png
%if %vlc_svn
%{_bindir}/qvlc
#{_datadir}/applications/gnome-vlc-default.sh
%{_libdir}/*.so.*
%dir %{_libdir}/vlc
%{_libdir}/vlc/*
%else
%{_bindir}/wxvlc
%exclude %{_libdir}/vlc/*.a
%exclude %{_libdir}/lib*.a
# Corba plugin is unmaintained and broken in vlc
%exclude %{_libdir}/vlc/control/libcorba_plugin.so
%dir %{_libdir}/vlc
%{_libdir}/vlc/*
%endif

%files devel
%defattr(-,root,root,-)
%doc HACKING
%{_bindir}/vlc-config
%dir %{_includedir}/vlc
%{_includedir}/vlc/*
%{_mandir}/man1/vlc-config.1*
%if %vlc_svn
%{_libdir}/*.so
%else
%{_libdir}/vlc/*.a
%{_libdir}/libvlc.a
%endif

%if %with_mozilla
%files -n mozilla-vlc
%defattr(-,root,root,-)
%{_libdir}/mozilla/plugins/libvlcplugin.so
%endif


%files -n python-vlc
%defattr(-,root,root,-)
%{python_sitearch}/*
%if %vlc_svn
%else
%{_bindir}/vlcwrapper.py
%exclude %{_bindir}/vlcwrapper.py?
%dir %{_libdir}/advene
%{_libdir}/advene/MediaControl.so
%{_datadir}/idl/MediaControl.idl
%endif

%if %with_java_vlc
%files -n java-vlc
%defattr(-,root,root,-)
%endif


%changelog
* Sat May 19 2007 kwizart < kwizart at gmail.com > - 0.8.6b-5
- Removed no more needed Selinux Context:
  fixed in http://bugzilla.redhat.com/237473

* Sun May 13 2007 kwizart < kwizart at gmail.com > - 0.8.6b-4
- Disabled pth (broken) and...
- Build ffmpeg static (since shared ffmpeg is pth enabled).
- Add post & postun update-desktop-database
- Update static ffmpeg to 20070503 (same as shared version)

* Sun May 13 2007 kwizart < kwizart at gmail.com > - 0.8.6b-3.3
- Test static updated live555

* Sat May 12 2007 kwizart < kwizart at gmail.com > - 0.8.6b-3.2
- Update to the new ffmpeg with pth (testing - wip )

* Fri May  4 2007 kwizart < kwizart at gmail.com > - 0.8.6b-3.1
- Add BR libebml-devel
- Add BR Glide3-devel
- Add BR gnome-vfs2-devel
- Add BR libxml2-devel
- Fix BR faad2-devel < 2.5
- Add rpmfusion BR libopendaap-devel
- Add rpmfusion BR libgoom2-devel
- Add rpmfusion BR libdc1394-devel
- Exclude corba plugin (broken)
- Add relatives %%configure options
- Comment Glide3 (don't work now - wip)

* Thu May  3 2007 kwizart < kwizart at gmail.com > - 0.8.6b-3
- Enable --enable-pth with ffmpeg
  bump release in case testing take much time.

* Thu May  3 2007 kwizart < kwizart at gmail.com > - 0.8.6b-1.3
- Fix Selinux remain quiet with semanage

* Tue May  1 2007 kwizart < kwizart at gmail.com > - 0.8.6b-1.2
- Few improvements for svn version
- Add missing BR ORBit2-devel and pyorbit-devel
- Improved post preun postun section with help from Anvil.

* Mon Apr 30 2007 kwizart < kwizart at gmail.com > - 0.8.6b-1.1
- Add missing BR libtiff-devel
- Fix Selinux buglet when Selinux is not activated
  was https://bugzilla.livna.org/show_bug.cgi?id=1484

* Sat Apr 21 2007 kwizart < kwizart at gmail.com > - 0.8.6b-1
- Update to Final 8.6b
- Enable Dirac codec
- Fix mozilla-vlc libXt.so loading 
  (removing mozilla-sdk since using firefox sdk >= 1.5)
- Fix SeLinux context for dmo plugin. Was:
  https://bugzilla.livna.org/show_bug.cgi?id=1404
- Enabled cddax only for x86_64 (broken type).

* Wed Apr 18 2007 kwizart < kwizart at gmail.com > - 0.8.6b-0.3
- Fix BR for libsmbclient-devel for Fedora 7
- Update to 0.8.6-bugfix-20070418
- Add BR libraw1394-devel
- Add BR libavc1394-devel

* Mon Apr 16 2007 kwizart < kwizart at gmail.com > - 0.8.6b-0.2
- Fix svgalib-devel only for x86 x86_64
- Fix firefox-devel headers presence/usability. This remains:
 npapi.h: accepted by the compiler, rejected by the preprocessor!
 npapi.h: proceeding with the compiler's result

* Sat Apr 14 2007 kwizart < kwizart at gmail.com > - 0.8.6b-0.1
- Update to rc 0.8.6b (bugfix)
- Hack configure.ac script (it didn't detect firefox headers)
- Add BR libshout-devel
- Add BR svgalib-devel
- Add BR gtk2-devel
- Add BR directfb-devel (wip)
- Add BR libnotify-devel
- Enabled --enable-speex
- Testing --enable-portaudio not useful (oss is deprecated)
- Enabled --enable-pda
- Testing --enable-directfb (wip)
- Removed patch5 (was format.c)

* Thu Apr  5 2007 kwizart < kwizart at gmail.com > - 0.8.6a-5
- Use system ffmpeg lib (pth and libtool seems to be incompatible with it)
- Dirac seem to compile fine but testing usability for now.
- Cache isn't useful for now (and won't be since using system libs)
- Exclude %%{_bindir}/vlcwrapper.py? since this is the guideline about python for now.

* Mon Apr  2 2007 kwizart < kwizart at gmail.com > - 0.8.6a-4.6
- Fix %%{_libdir}/advene directory ownership from: #1458
- Fix .py? presence and perm (644)
- Remove .la after make install
- Add --disable-pth (broken for release and svn)
  
* Sat Mar 24 2007 kwizart < kwizart at gmail.com > - 0.8.6a-4.5
- Test dirac (disabled mozilla )
- Test Updated static live555 to 2007.02.22
- Clean up svn to release changes

* Tue Mar 22 2007 kwizart < kwizart at gmail.com > - 0.8.6a-4.4
- WIP changes - ld.conf is unuseful...

* Wed Mar 21 2007 kwizart < kwizart at gmail.com > - 0.8.6a-4.3
- Revert back to the static vlc version 
 ( will explore this with ld.conf later )

* Wed Mar 21 2007 kwizart < kwizart at gmail.com > - 0.8.6a-4.2
- Fix .desktop file
- Disable broken libtool 
- Quick fixes for svn/cache prepare
- Patch format_c
- Fix rpmlint error with python-vlc

* Tue Mar 20 2007 kwizart < kwizart at gmail.com > - 0.8.6a-4.1
- Enable cache for static compilation - wip

* Fri Mar  9 2007 kwizart < kwizart at gmail.com > - 0.8.6a-4
- Enable conditionnal build for
	* mozilla-vlc, java-vlc, dirac
	* ffmpeg and live static
- Enable pth
- Enable gnu_ld

* Thu Mar  8 2007 kwizart < kwizart at gmail.com > - 0.8.6a-3.1
- Fix firefox-devel detection when avaible both i386 and x86_64
  http://bugzilla.livna.org/show_bug.cgi?id=1442

* Thu Mar  8 2007 kwizart < kwizart at gmail.com > - 0.8.6a-3
- Recover patch3 from Ville Skyttä
- Fix FLAC api change see
 http://bugzilla.livna.org/show_bug.cgi?id=1433

* Thu Mar  8 2007 kwizart < kwizart at gmail.com > - 0.8.6a-2
- Update ffmpeg to 20070308
- Enabled static build for internal ffmpeg (x264 vlc modules)
- Fixed: some configure options has changed for ffmpeg

* Sat Mar  3 2007 Thorsten Leemhuis <fedora at leemhuis dot info> - 0.8.6a-1.2
- Rebuild

* Sun Feb  4 2007 Ville Skyttä <ville.skytta at iki.fi> - 0.8.6a-1.1
- Fix aclocal/automake fix for automake 1.10 without breaking it for earlier.

* Sun Feb  4 2007 Ville Skyttä <ville.skytta at iki.fi> - 0.8.6a-1
- Build internal copy of ffmpeg with $RPM_OPT_FLAGS.
- Don't hardcode path to firefox headers.
- Drop Application and X-Livna categories from desktop entry.
- Clean up some unneeded cruft from specfile.
- Fix aclocal/automake calls during bootstrap.
- Let rpmbuild strip MediaControl.so.

* Sat Feb  3 2007 kwizart < kwizart at gmail.com > - 0.8.6a-0.4.static
- Internal static build of ffmpeg from Matthias version.

* Fri Jan 19 2007 kwizart < kwizart at gmail.com > - 0.8.6a-0.3
- Re-enabled mozilla-vlc
- use ifarch ix86

* Sat Jan 13 2007 kwizart < kwizart at gmail.com > - 0.8.6a-0.2
- Import patches from Matthias version
- try to fix firefox includes for mozilla-vlc -> disabled

* Wed Jan 10 2007 kwizart < kwizart at gmail.com > - 0.8.6a-0.1
- Try to Fix run with libavformat.so.51
- disabled

* Mon Jan  8 2007 kwizart < kwizart at gmail.com > - 0.8.6-5
- Update to BR bugzilla infos.
- Fix perms with python and debug headers.
- Cleaned obsolete-not-provided

* Fri Jan  5 2007 kwizart < kwizart at gmail.com > - 0.8.6-4
- Use BuildConflics with libcdio
- Enabled --enable-cddax
- Enabled --enable-vcdx
-  waiting --enable-quicktime (build fails)

* Fri Jan  5 2007 kwizart < kwizart at gmail.com > - 0.8.6-3
  with help from Rathan
- Update to 0.8.6a (security update!)
  from http://www.videolan.org/sa0701.html - #1342
- Add version to desktop file
- Fix dual shortcuts / Add MimeType

* Wed Jan  3 2007 kwizart < kwizart at gmail.com > - 0.8.6-2
 with help from Rathan
- Enabled --enable-shout
- Enabled --enable-quicktime (x86 only !) 
- Enabled --enable-loader (x86 only !)
- Enabled --with-wine-sdk-path (x86 only !)
- Enabled --enable-corba
-  testing --enable-dirac (libdirac-devel reviewing in extra)
   http://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=221405
- Enabled --enable-mediacontrol-python-bindings
- Cosmetic changes in BR

* Mon Dec 11 2006 kwizart < kwizart at gmail.com > - 0.8.6-1.fc6
- Update to 8.6 final
- Change deprecated livdotcom to live555
- build shared librairies is default since 8.6
- Enabled --enable-dvdread
- Enabled --enable-faad
- Enabled --enable-twolame
-   waiting --enable-quicktime (problem finding xqtsdk )
- Enabled --enable-real
- Enabled --enable-realrtsp
- Enabled --enable-tremor
- Enabled --enable-tarkin
-   waiting --enable-dirac (TODO libdirac-devel )
- Enabled --enable-snapshot
- Enabled --enable-portaudio
- Enabled --enable-jack
-   waiting --enable-mediacontrol-python-bindings (default install error)
-   waiting --enable-cddax (new version of libcdio 0.78.2)
-   waiting --enable-vcdx (new version of libcdio 0.78.2)

* Mon Dec 04 2006 kwizart < kwizart at gmail.com > - 0.8.6-rc1.1.fc6
- Update to 8.6rc1
- disable components in mozilla-vlc
- disable libvlc_pic.a in devel
- Enable x264-devel for static linking.

* Fri Oct 06 2006 Thorsten Leemhuis <fedora [AT] leemhuis [DOT] info> 0.8.5-6
- rebuilt for unwind info generation, broken in gcc-4.1.1-21

* Mon Sep 25 2006 Dams <anvil[AT]livna.org> - 0.8.5-5
- BuildReq:libtool

* Sun Sep 24 2006 Dams <anvil[AT]livna.org> - 0.8.5-4
- Fixed the mozilla plugin damn build 

* Sat Sep  9 2006 Dams <anvil[AT]livna.org> - 0.8.5-3
- sysfsutils-devel -> libsysfs-devel

* Sat Sep  9 2006 Dams <anvil[AT]livna.org> - 0.8.5-1
- Updated to 0.8.5
- Fixed MOZVER value in case more than one mozilla is installed.
- Dropped patches 1, 2 and 3

* Wed Aug 16 2006 Ville Skyttä <ville.skytta at iki.fi> - 0.8.4a-2
- Adjust for new live package, enable it on all archs.

* Fri Apr 14 2006 Ville Skyttä <ville.skytta at iki.fi> - 0.8.4a-1
- Apply upstream patch to fix linking with newer ffmpeg/postproc.
- Drop no longer needed build conditionals and build dependencies.
- Enable Avahi, Musepack, SLP and sysfs support, fix SDL and Xv.
- Install icon to %%{_datadir}/icons/hicolor.
- Drop zero Epoch remainders.
- Fix -devel obsoletes.
- Specfile cleanups.

* Fri Mar 24 2006 Thorsten Leemhuis <fedora[AT]leemhuis.info> 0.8.4-9.a
- rebuild 

* Tue Mar 21 2006 Andreas Bierfert <andreas.bierfert[AT]lowlatency.de>
0.8.4-8.a
- fix #775

* Mon Mar 20 2006 Andreas Bierfert <andreas.bierfert[AT]lowlatency.de>
0.8.4-7.a
- add -fPIC for all arches

* Mon Mar 20 2006 Andreas Bierfert <andreas.bierfert[AT]lowlatency.de>
0.8.4-6.a
- fix build on ppc/i386

* Thu Mar 16 2006 Andreas Bierfert <andreas.bierfert[AT]lowlatency.de>
0.8.4-5.a
- fix BR

* Wed Mar 15 2006 Andreas Bierfert <andreas.bierfert[AT]lowlatency.de>
0.8.4-4.a
- make vlc build again

* Tue Mar 14 2006 Thorsten Leemhuis <fedora[AT]leemhuis.info> 0.8.4-3.a
- drop "0.lvn" from release

* Tue Feb 28 2006 Andreas Bierfert <andreas.bierfert[AT]lowlatency.de>
- add dist

* Mon Jan 09 2006 Thorsten Leemhuis <fedora[AT]leemhuis.info> - 0.8.4-0.lvn.3.a
- add all BRs the new ffmpeg needs

* Fri Jan 06 2006 Thorsten Leemhuis <fedora[AT]leemhuis.info> - 0.8.4-0.lvn.2.a
- add buildoption "--without mkv" -- ebml in FC3 is to old
- add buildoption "--without svg" -- does not build with svg on FC3-x86-64

* Thu Jan 05 2006 Thorsten Leemhuis <fedora[AT]leemhuis.info> - 0.8.4-0.lvn.1.a
- Update to 0.8.4a [with help from che (Rudolf Kastl)]
- Fix x64
- drop Epoch
- drop vlc-0.8.2-test2-altivec.patch, seems they worked on this
- use " --disable-libcdio" until we update to wxGTK2 2.6
- use "--disable-livedotcom" on x86_64 (does not build)

* Sat Aug  6 2005 Ville Skyttä <ville.skytta at iki.fi> - 0:0.8.2-0.lvn.4
- Fix "--without cddb" build when libcddb-devel is installed.
- BuildRequire live-devel instead of live.

* Wed Aug  3 2005 Dams <anvil[AT]livna.org> - 0:0.8.2-0.lvn.3
- Rebuilt *without* libcddb
- Rebuilt against new libdvbpsi

* Thu Jul 28 2005 Dams <anvil[AT]livna.org> - 0:0.8.2-0.lvn.2
- Rebuilt against new libcddb/libcdio

* Sat Jul  9 2005 Dams <anvil[AT]livna.org> - 0:0.8.2-0.lvn.1
- Updated to final 0.8.2

* Mon Jun  6 2005 Ville Skyttä <ville.skytta at iki.fi> 0:0.8.2-0.lvn.0.1.test2
- Update to 0.8.2-test2, rename to vlc, improve summaries and descriptions.
- Enable many more modules, many small improvements and cleanups here and there
- Use unversioned install dir for the Mozilla plugin, rename to mozilla-vlc.
- Drop < FC3 compatiblity due to unavailability of required lib versions.
- Fold wx and ncurses to the main package (upstream has retired the
  VLC Gnome and KDE UI's, so separate UI packages don't have a purpose
  any more).

* Sat Sep 11 2004 Ville Skyttä <ville.skytta at iki.fi> - 0:0.7.2-0.lvn.7
- Remove dependency on libpostproc-devel, it's now in ffmpeg-devel (bug 255).

* Thu Sep  2 2004 Ville Skyttä <ville.skytta at iki.fi> - 0:0.7.2-0.lvn.6
- BuildRequire alsa-lib-devel, was lost in previous update (bug 258).
- Add libcdio and libmodplug build dependencies.
- Tweak descriptions, remove unnecessary conditional sections.
- Disable dependency tracking to speed up the build.

* Sun Aug 29 2004 Ville Skyttä <ville.skytta at iki.fi> - 0:0.7.2-0.lvn.5
- Use system ffmpeg (>= 0.4.9), and make it, ALSA, and fribidi unconditional.
- Build with theora by default.
- Change default font to Vera serif bold.
- Enable pvr support for Hauppauge card users (thanks to Gabriel L. Somlo).

* Mon Jul  5 2004 Dams <anvil[AT]livna.org> 0:0.7.2-0.lvn.4
- Enabled libcddb support

* Wed Jun 30 2004 Dams <anvil[AT]livna.org> 0:0.7.2-0.lvn.3
- speex now conditional and default disabled since vlc requires
  development version. 

* Wed Jun 30 2004 Dams <anvil[AT]livna.org> 0:0.7.2-0.lvn.2
- Optional Fribidi and libtheora support (default disabled)

* Tue May 25 2004 Dams <anvil[AT]livna.org> 0:0.7.2-0.lvn.1
- Updated to 0.7.2

* Fri May  7 2004 Dams <anvil[AT]livna.org> 0:0.7.1-0.lvn.1
- BuildConflicts:ffmpeg
- Build against private ffmpeg snapshot

* Tue Mar  9 2004 Dams <anvil[AT]livna.org> 0:0.7.1-0.lvn.1
- Updated to 0.7.1
- Added live.com libraries support
- Added matroska support

* Sun Jan  4 2004 Dams <anvil[AT]livna.org> 0:0.7.0-0.lvn.1
- Updated to 0.7.0
- s/fdr/lvn

* Wed Dec 10 2003 Dams <anvil[AT]livna.org> 0:0.6.2-0.fdr.7
- Conditional ffmpeg build option (default enabled)

* Fri Sep  5 2003 Dams <anvil[AT]livna.org> 0:0.6.2-0.fdr.6
- pth support now default disabled 

* Fri Sep  5 2003 Dams <anvil[AT]livna.org> 0:0.6.2-0.fdr.5
- slp support can now be not-build with '--without slp'

* Thu Sep  4 2003 Dams <anvil[AT]livna.org> 0:0.6.2-0.fdr.4
- Added missing defattr for subpackages
- Fixed permissions on mozilla plugin
- fixed build failure due to typos in ncurses changes
- Removed useless explicit 'Requires:' in subpackages declarations

* Tue Sep  2 2003 Dams <anvil[AT]livna.org> 0:0.6.2-0.fdr.3
- Added builddep for libpng-devel and openslp-devel 
- Added gnome (default:enabled) and ncurses (default:disabled)
  subpackages
- Removed macros (mkdir/install/perl)
- Modified descriptions
- Removed gtk/gnome2 build deps
- Added conditionnal (default-disabled) build option for alsa
- Added conditionnal builddep for pth-devel

* Fri Aug 22 2003 Dams <anvil[AT]livna.org> 0:0.6.2-0.fdr.2
- Added missing BuildRequires for gtk+-devel

* Thu Aug 14 2003 Dams <anvil[AT]livna.org> 0:0.6.2-0.fdr.1
- Updated to 0.6.2
- Hopefully fixed 'if' conditions for optional buildrequires

* Tue Jul  8 2003 Dams <anvil[AT]livna.org> 0:0.6.0-0.fdr.3
- Providing vlc 

* Tue Jul  8 2003 Dams <anvil[AT]livna.org> 0:0.6.0-0.fdr.2
- Moved desktop entry from devel to main package (stupid me)

* Mon Apr 28 2003 Dams <anvil[AT]livna.org> 
- Initial build.
