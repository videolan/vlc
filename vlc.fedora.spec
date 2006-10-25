%define mozver 1.7.3
%define ffmpeg_date 20040520

Summary: The VideoLAN client, also a very good standalone video player.
Name: vlc
Version: 0.8.0
Release: test2
Group: Applications/Multimedia
License: GPL
URL: http://www.videolan.org/
Source0: http://www.videolan.org/pub/videolan/vlc/vlc-%{version}-%{release}.tar.bz2
Source1: http://download.videolan.org/pub/videolan/vlc/%{version}/contrib/ffmpeg-%{ffmpeg_date}.tar.bz2

Buildroot: %{_tmppath}/%{name}-root
Packager: Jason Luka <jason@geshp.com>
Buildrequires: desktop-file-utils, libpostproc >= 1.0
%{!?_without_dvd:Buildrequires: libdvdcss-devel >= 1.2.8}
%{!?_without_dvdread:Buildrequires: libdvdread-devel >= 0.9.4}
%{?_with_dvdplay:Buildrequires: libdvdplay-devel >= 1.0.1}
%{!?_without_dvdnav:Buildrequires: libdvdnav >= 0.1.10}
%{!?_without_dvbpsi:Buildrequires: libdvbpsi-devel >= 0.1.3}
%{!?_without_ogg:Buildrequires: libogg-devel}
%{!?_without_mad:Buildrequires: libmad-devel >= 0.15.0b}
%{?_with_xvid:Buildrequires: xvidcore-devel >= 0.9.2}
%{!?_without_a52:Buildrequires: a52dec-devel}
%{?_with_dv:Buildrequires: libdv-devel >= 0.99}
%{!?_without_flac:Buildrequires: flac-devel >= 1.1.0}
%{!?_without_vorbis:Buildrequires: libvorbis-devel}
%{!?_without_sdl:Buildrequires: SDL-devel}
%{!?_without_aa:Buildrequires: aalib-devel}
%{!?_without_esd:Buildrequires: esound-devel}
%{!?_without_arts:Buildrequires: arts-devel}
%{!?_without_alsa:Buildrequires: alsa-lib-devel}
%{?_with_gtk:Buildrequires: gtk+-devel}
%{?_with_gnome:Buildrequires: gnome-libs-devel}
%{!?_without_lirc:Buildrequires: lirc}
%{?_with_qt:Buildrequires: qt-devel}
%{?_with_kde:Buildrequires: kdelibs-devel}
%{!?_without_ncurses:Buildrequires: ncurses-devel >= 5}
%{!?_without_xosd:Buildrequires: xosd-devel >= 2.2.5}
%{!?_without_id3tag:BuildRequires: libid3tag-devel}
%{!?_without_mpeg2dec:BuildRequires: mpeg2dec-devel >= 0.3.2}
%{!?_without_wxwindows:BuildRequires: wxGTK-devel >= 2.4.2}
%{!?_without_mozilla:BuildRequires: mozilla-devel >= %{mozver}}
%{!?_without_mozilla:BuildRequires: mozplugger >= 1.3.2}
%{!?_without_speex:BuildRequires: speex-devel >= 1.0.3}
%{!?_without_aa:BuildRequires: aalib >= 1.4}
%{!?_without_mkv:BuildRequires: libmatroska-devel}
%{!?_without_fribidi:BuildRequires: fribidi-devel}
%{!?_without_caca:BuildRequires: libcaca-devel}

Obsoletes: videolan-client, matroska, libebml, libmatroska

Requires: desktop-file-utils
%{!?_without_dvd:Requires: libdvdcss >= 1.2.8}
%{!?_without_dvdread:Requires: libdvdread >= 0.9.4}
%{?_with_dvdplay:Requires: libdvdplay >= 1.0.1}
%{!?_without_dvbpsi:Requires: libdvbpsi >= 0.1.3}
%{!?_without_ogg:Requires: libogg}
%{!?_without_mad:Requires: libmad >= 0.15.0b}
%{!?_without_xvid:Requires: xvidcore >= 0.9.2}
%{!?_without_a52:Requires: a52dec}
%{?_with_dv:Requires: libdv >= 0.99}
%{!?_without_flac:Requires: flac >= 1.1.0}
%{!?_without_vorbis:Requires: libvorbis}
%{!?_without_sdl:Requires: SDL}
%{!?_without_aa:Requires: aalib >= 1.4}
%{!?_without_esd:Requires: esound}
%{!?_without_arts:Requires: arts}
%{!?_without_alsa:Requires: alsa-lib}
%{?_with_gtk:Requires: gtk+}
%{?_with_gnome:Requires: gnome-libs}
%{?_with_qt:Requires: qt}
%{?_with_kde:Requires: kdelibs}
%{?_with_ncurses:Requires: ncurses}
%{?_with_xosd:Requires: xosd >= 2.2.5}
%{!?_without_lirc:Requires: lirc}
%{!?_without_mozilla:Requires: mozilla >= %{mozver}}
%{!?_without_speex:Requires: speex >= 1.0.3}
%{!?_without_wxwindows:Requires: wxGTK >= 2.4.2}
%{!?_without_fribidi:Requires: fribidi}

%description
VideoLAN Client (VLC) is a highly portable multimedia player for various
audio and video formats (MPEG-1, MPEG-2, MPEG-4, DivX, mp3, ogg, ...) as
well as DVDs, VCDs, and various streaming protocols.

Available rpmbuild rebuild options :
--without dvd dvdread dvdplay dvbpsi dv v4l avi asf aac ogg rawdv mad ffmpeg xvid
          mp4 a52 vorbis mpeg2dec flac aa esd arts alsa gtk gnome xosd lsp lirc
          pth id3tag dv qt kde ncurses faad wxwindows mkv fribidi theora

Options that would need not yet existing add-on packages :
--with tremor tarkin ggi glide svgalib mga


%package devel
Summary: Header files and static library from the Videolan Client.
Group: Development/Libraries
Requires: %{name} = %{version}

%description devel
VideoLAN Client (VLC) is a highly portable multimedia player for various
audio and video formats (MPEG-1, MPEG-2, MPEG-4, DivX, mp3, ogg, ...) as
well as DVDs, VCDs, and various streaming protocols.

Install this package if you need to build Videolan Client plugins or intend
to link statically to it.


%prep
%setup -q -n vlc-%{version} -a 1

# Build bundeled ffmpeg first
pushd ffmpeg-%{ffmpeg_date}
    %configure \
        --disable-shared \
        --enable-gpl \
        --enable-pp \
        %{!?_without_lame: --enable-mp3lame} \
        %{!?_without_vorbis: --enable-vorbis} \
        %{!?_without_faad: --enable-faad} \
        %{!?_without_faac: --enable-faac} \
        %{!?_without_a52dec: --enable-a52}
    %{__make} %{?_smp_mflags}
popd

export XPIDL=%{_libdir}/mozilla-%mozver/xpidl
export XPIDL_INCL=-I%{_includedir}/mozilla-%mozver
./bootstrap

%build
cp %{_libdir}/mozilla/plugins/mozplugger.so %{_libdir}/mozilla-%{mozver}/plugins/mozplugger.so.bak -f
mv %{_libdir}/mozilla-%{mozver}/plugins/mozplugger.so.bak %{_libdir}/mozilla-%{mozver}/plugins/mozplugger.so -f
rm %{_libdir}/mozilla -fr
ln %{_libdir}/mozilla-%{mozver} %{_libdir}/mozilla -sf
ln /usr/share/idl/mozilla-%{mozver} /usr/share/idl/mozilla -sf
#ln %{_libdir}/libxvidcore.so.2 %{_libdir}/libxvidcore.so -sf

%configure \
	--enable-release \
	--enable-vcd \
	--enable-x11 \
	--enable-xvideo \
	--disable-qte \
	--disable-directx \
	--enable-fb \
	%{!?_without_dvdread:--enable-dvdread} \
	%{!?_without_dvdnav:--enable-dvdnav} \
	%{!?_without_dvbpsi:--enable-dvbpsi} \
	%{!?_without_v4l:--enable-v4l} \
        %{!?_without_ffmpeg:--enable-ffmpeg} \
        %{!?_without_ffmpeg:--with-ffmpeg-tree=ffmpeg-%{ffmpeg_date}} \
	%{!?_without_flac:--enable-flac} \
	%{!?_without_theora:--enable-theora} \
	%{!?_without_mad:--enable-mad} \
	%{!?_without_faad:--enable-faad} \
	%{!?_without_aa:--enable-aa} \
	%{!?_without_caca:--enable-caca} \
	%{!?_without_dvb:--enable-dvb} \
	%{!?_without_pvr:--enable-pvr} \
	%{!?_without_livedotcom:--enable-livedotcom --with-livedotcom-tree=%{_libdir}/live} \
	%{!?_without_alsa:--enable-alsa} \
	%{!?_without_esd:--enable-esd} \
	%{!?_without_arts:--enable-arts} \
	%{!?_without_fribidi:--enable-fribidi} \
	%{!?_without_freetype:--enable-freetype} \
	%{!?_without_wxwindows:--enable-wxwindows} \
	%{!?_without_ncurses:--enable-ncurses} \
	%{!?_without_lirc:--enable-lirc} \
	%{!?_without_mozilla:--enable-mozilla} \
	%{?_with_xvid:--enable-xvid} \
	%{?_with_dv:--enable-dv} \
	%{!?_without_sdl:--enable-sdl} \
	%{?_with_xosd:--enable-xosd} \
	%{?_with_slp:--enable-slp} \
        %{?_without_mkv:--disable-mkv} \
	%{?_with_tremor:--enable-tremor} \
	%{?_with_tarkin:--enable-tarkin} \
	%{?_without_mp4:--disable-mp4} \
	%{?_without_a52:--disable-a52} \
	%{?_without_cinepak:--disable-cinepak} \
	%{?_without_mpeg2dec:--disable-libmpeg2} \
	%{?_without_vorbis:--disable-vorbis} \
	%{?_with_mga:--enable-mga} \
	%{?_with_svgalib:--enable-svgalib} \
	%{?_with_ggi:--enable-ggi} \
	%{?_with_glide:--enable-glide} \
	--without-wingdi \
	--enable-oss \
        --disable-waveout \
	%{?_with_gtk:--enable-gtk} \
	--disable-familiar \
	%{?_with_gnome:--enable-gnome} \
	%{?_with_qt:--enable-qt} \
	%{?_with_kde:--enable-kde} \
	--disable-opie \
	--disable-macosx \
	--disable-qnx \
	--disable-intfwin \
	%{?_with_pth:--enable-pth} \
	--disable-st \
        %{?_without_speex:--disable-speex} \
	--disable-testsuite \
	%{?_with_dvdplay:--enable-dvdplay} \
	%{?_without_dvd:--disable-dvd} \
	%{?_without_avi:--disable-avi} \
	%{?_without_asf:--disable-asf} \
	%{?_without_aac:--disable-aac} \
	%{?_without_ogg:--disable-ogg} \
	%{?_without_rawdv:--disable-rawdv}

make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%makeinstall
find  %{buildroot}%{_libdir}/vlc -name "*.so" | xargs strip
%find_lang vlc

cat > %{name}.desktop << EOF
[Desktop Entry]
Name=VideoLAN Media Player
Comment=%{summary}
Icon=%{_datadir}/vlc/vlc48x48.png
Exec=vlc
Terminal=0
Type=Application
EOF

mv %{buildroot}%{_libdir}/mozilla %{buildroot}%{_libdir}/mozilla-%{mozver} -f

mkdir -p %{buildroot}%{_datadir}/applications
desktop-file-install --vendor gnome --delete-original             \
  --dir %{buildroot}%{_datadir}/applications                      \
  --add-category X-Red-Hat-Base                                   \
  --add-category Application                                      \
  --add-category AudioVideo                                       \
  %{name}.desktop

%post
ln /dev/cdrom /dev/dvd -sf

%postun
rm -f /dev/dvd
rm /usr/share/idl/mozilla -fr

%clean
rm -rf %{buildroot}
rm /usr/share/idl/mozilla -fr
rm /usr/lib/mozilla -fr
mkdir /usr/lib/mozilla/plugins -p --mode=755
mv %{_libdir}/mozilla-%{mozver}/plugins/mozplugger.so %{_libdir}/mozilla/plugins -f

%files -f vlc.lang
%defattr(-, root, root)
%doc AUTHORS COPYING ChangeLog MAINTAINERS README THANKS
%doc doc/fortunes.txt doc/intf-vcd.txt
%doc doc/bugreport-howto.txt
%exclude %{_datadir}/doc/vlc/*
%{_bindir}/*vlc
%{_libdir}/vlc
%{_libdir}/libvlc_pic.a
%{_libdir}/mozilla-%{mozver}/components/vlcintf.xpt
%{_libdir}/mozilla-%{mozver}/plugins/libvlcplugin.so
%{_datadir}/applications/gnome-%{name}.desktop
%{_datadir}/vlc

%files devel
%defattr(-, root, root)
%doc HACKING 
%{_bindir}/vlc-config
%{_includedir}/vlc
%{_libdir}/libvlc.a

%changelog
* Sun Oct 10 2004 Jason Luka
- Update to 0.8.0-test2
- Inserted static ffmpeg routine
- Removed outdated kde, qt, gnome, and gtk+ interfaces
- Added livedotcom dependancy
- Openslp is broken, temporarily removed
- Added EXPORTs and bootstrap
- Removed ffmpeg dependancy as the static lib works better for now

* Sun Sep 19 2004 Jason Luka
- Update to 0.8.0-test1
- Added --enable-gpl
- Updated Mozilla version for FC2

* Fri Mar 19 2004 Jason Luka
- Removed dependancy on XFree86 as FC2 now calls the same package xorg

* Mon Mar 15 2004 Jason Luka
- Update to 0.7.1

* Tue Dec 2 2003 Jason Luka
- Added fribidi support
- Added fribidi and mkv options to configure

* Sat Nov 29 2003 Jason Luka
- Fixed Matroska/EBML problem
- Updated script for mozilla plugin installation

* Fri Nov 28 2003 Jason Luka
- Update to 0.7.0-test1
- Updated version numbers on dependancies
- Removed ALSA support until RH/FC turns to kernel 2.6
- Added --enable-speex and --enable-pp
- Mozilla plugin now built for 1.4.1
- Currently broken (Matroska/EBML problems)

* Mon Aug 25 2003 Jason Luka
- Added matroska support
- Corrected some symlinking problems with the mozilla plugin

* Fri Aug 22 2003 Jason Luka <jason@geshp.com>
- Update to 0.6.2
- Changed menu item name to VideoLAN Media Player
- Added openslp support
- Added libtar support (needed for skins)
- Added symlink to libxvidcore.so, thanks to new version of that software

* Fri Aug 1 2003 Jason Luka <jason@geshp.com>
- Update to 0.6.1
- Fixed file structure problems I created to accomodate the mozilla plugin
- Changed vendor name for desktop install
- Moved vlc to base menu
- Moved plugins from /usr/lib/mozilla to /usr/lib/mozilla-x.x.x
- Added custom patch to accomodate mozilla plugin
- Added execution of bootstrap since Makefile.am was altered

* Tue Jul 8 2003 Jason Luka <jason@geshp.com>
- Update to 0.6.0
- Add id3lib, dv, faad, qt, kde, and mozilla plugin support
- Added script to symlink mozilla-1.2.1 directories to mozilla so build can complete

* Sat Apr 5 2003 Jason Luka <jason@geshp.com>
- Rebuilt for Red Hat 9
- Changed dependencies for ffmpeg's new name
- Required lirc support at build-time 

* Sat Mar 25 2003 Jason Luka <jason@geshp.com>
- Fixed Buildrequire statements to require all plugins at compile-time
- Fixed Require statements so users don't have to install every plugin

* Thu Mar 23 2003 Jason Luka <jason@geshp.com>
- Renamed ffmpeg to libffmpeg
- Rebuilt for videolan site
- Autolinked /dev/dvd to /dev/cdrom

* Tue Mar 11 2003 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Update to 0.5.2.
- Fix the dv build dependency, thanks to Alan Hagge.
- Added flac support.
- Fixed the libdvbpsi requirements.

* Mon Feb 24 2003 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Rebuilt against the new xosd lib.

* Wed Feb 19 2003 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Update to 0.5.1.
- Major spec file update.

* Fri Nov 15 2002 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Update to 0.4.6.

* Tue Oct 22 2002 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Update to 0.4.5.
- Minor --with / --without adjustments.

* Sun Oct  6 2002 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Rebuilt for Red Hat Linux 8.0.
- New menu entry.
- Added all --without options and --with qt.

* Mon Aug 12 2002 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Update to 0.4.4.

* Fri Jul 26 2002 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Update to 0.4.3.

* Fri Jul 12 2002 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Update to 0.4.2.

* Wed Jun  5 2002 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Update to 0.4.1.

* Fri May 24 2002 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Update to 0.4.0.
- Disabled qt interface, it's hell to build with qt2/3!
- Use %%find_lang and %%{?_smp_mflags}.

* Fri Apr 19 2002 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Update to 0.3.1.

* Mon Apr  8 2002 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Update to 0.3.0.

* Sat Jan 12 2002 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Removed the dependency on libdvdcss package, use the built in one instead,
  because 1.x.x is not as good as 0.0.3.ogle3.

* Tue Jan  1 2002 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Update to 0.2.92.
- Build fails with libdvdcss < 1.0.1.

* Tue Nov 13 2001 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Update to 0.2.91 and now requires libdvdcss 1.0.0.

* Mon Oct 22 2001 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Split libdvdcss into a separate package since it's also needed by the
  xine menu plugin.

* Thu Oct 11 2001 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Updated to 0.2.90.
- Removed ggi, svgalib and aalib since they aren't included in Red Hat 7.2.

* Mon Aug 27 2001 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Updated to 0.2.83.

* Sat Aug 11 2001 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Updated to 0.2.82.

* Mon Jul 30 2001 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Updated to 0.2.81.
- Added all the new split libdvdcss.* files to the %%files section.

* Tue Jun  5 2001 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Updated to the latest release, 0.2.80.

* Wed May 30 2001 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Updated to today's CVS version, works great! :-)
- Fixed the desktop menu entry.

* Tue May 22 2001 Matthias Saou <matthias.saou@est.une.marmotte.net>
- Spec file cleanup to make it look more like others do.
- Added the use of many macros.
- Disabled automatic requires and provides (the package always needed qt,
  gtk+, gnome etc. otherwise).
- Added a system desktop menu entry.

* Mon Apr 30 2001 Arnaud Gomes-do-Vale <arnaud@glou.org>
Added relocation support and compile fixes for Red Hat 7.x.

* Sat Apr 28 2001 Henri Fallon <henri@videolan.org>
New upstream release (0.2.73)

* Mon Apr 16 2001 Samuel Hocevar <sam@zoy.org>
New upstream release (0.2.72)

* Fri Apr 13 2001 Samuel Hocevar <sam@zoy.org>
New upstream release (0.2.71)

* Sun Apr 8 2001 Christophe Massiot <massiot@via.ecp.fr>
New upstream release (0.2.70)

* Fri Feb 16 2001 Samuel Hocevar <sam@via.ecp.fr>
New upstream release

* Tue Aug  8 2000 Samuel Hocevar <sam@via.ecp.fr>
Added framebuffer support

* Sun Jun 18 2000 Samuel Hocevar <sam@via.ecp.fr>
Took over the package

* Thu Jun 15 2000 Eric Doutreleau <Eric.Doutreleau@int-evry.fr>
Initial package

