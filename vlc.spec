%define name 		vlc
%define vlc_ver 	0.3.1
%define version		%vlc_ver

%define cvs     	0
%if %{cvs}
%define cvsdate 	20010619
%define release		0.%{cvsdate}mdk
%define cvs_name 	%{name}-snapshot-%{cvsdate}-00
%else
%define release 	1mdk
%endif

%define	plugin_qt	0

Summary:	VideoLAN is a free MPEG, MPEG2 and DVD software solution.
Name:		%{name}
Version:	%{version}
Release:	%{release}
Packager:	Samuel Hocevar <sam@zoy.org>

%if %{cvs} 
Source0:	http://www.videolan.org/pub/videolan/vlc/snapshots/%{cvs_name}.tar.gz
%else
Source0:	http://www.videolan.org/packages/%{version}/%{name}-%{version}.tar.gz
%endif
License:	GPL
Group:		Video
URL:		http://videolan.org/
BuildRoot:	%_tmppath/%name-%version-%release-root
Buildrequires:	libncurses5-devel
Buildrequires:	libqt2-devel
Buildrequires:	libgtk+1.2-devel
Buildrequires:	gnome-libs-devel
Buildrequires:	db1-devel
Buildrequires:	alsa-lib-devel
Buildrequires:	libarts-devel
Buildrequires:	libggi-devel
Buildrequires:	aalib-devel
Buildrequires:	SDL-devel
Buildrequires:	liba52dec-devel
Buildrequires:	libmad-devel

%description
VideoLAN is a free network-aware MPEG and DVD player.
The VideoLAN Client allows to play MPEG2 Transport Streams from the
network or from a file, as well as direct DVD playback.
VideoLAN is a project of students from the Ecole Centrale Paris.
This version add MPEG1 support, direct DVD support, DVD decryption, 
arbitrary, seeking in the stream, pause, fast forward and slow motion, 
hardware YUV acceleration and a few new interface features 
including drag'n'drop.
You may install vlc-gnome, vlc-gtk and vlc-ncurses.
This package contains no CSS unscrambling functionality.
You need the libdvdcss library available from 
http://www.videolan.org/libdvdcss/ or http://plf.zarb.org/

# intf plugins
%package gtk
Summary: Gtk plug-in for VideoLAN, a DVD and MPEG2 player
Group: Video
Requires: %{name} = %{version}
%description gtk
The vlc-gtk package includes the Gtk plug-in for the VideoLAN client.
If you are going to watch DVD with the Gtk front-end, you should 
install vlc-gtk.

%package gnome
Summary: Gnome plug-in for VideoLAN, a DVD and MPEG2 player
Group: Video
Requires: %{name} = %{version}
%description gnome
The vlc-gnome package includes the Gnome plug-in for the VideoLAN client.
If you are going to watch DVD with the Gnome front-end, you should 
install vlc-gnome.

%package qt
Summary: Qt2 plug-in for VideoLAN, a DVD and MPEG2 player
Group: Video
Requires: %{name} = %{version}
%description qt
The vlc-qt package includes the Qt2 plug-in for the VideoLAN client.
If you are going to watch DVD with the Qt2 front-end, you should
install vlc-qt

%package ncurses
Summary: Ncurses console-based plug-in for VideoLAN, a DVD and MPEG2 player
Group: Video
Requires: %{name} = %{version}
%description ncurses
The vlc-ncurses package includes the ncurses plug-in for the VideoLAN client.
If you are going to watch DVD with the ncurses front-end, you should
install vlc-ncurses

# video plugins
%package aa
Summary: ASCII art video plug-in for VideoLAN, a DVD and MPEG2 player
Group: Video
Requires: %{name} = %{version}
%description aa
The vlc-aa package includes the aalib plug-in for the VideoLAN client.
If you are going to watch DVD with the aa video plugin, you should
install vlc-aa

%package sdl
Summary: Simple DirectMedia Layer video plug-in for VideoLAN, a DVD and MPEG2 player
Group: Video
Requires: %{name} = %{version}
%description sdl
The vlc-sdl package includes the Simple DirectMedia Layer plug-in 
for the VideoLAN client.
If you are going to watch DVD with the sdl video plugin, you should
install vlc-sdl

%package ggi
Summary: GGI video plug-in for VideoLAN, a DVD and MPEG2 player
Group: Video
Requires: %{name} = %{version}
%description ggi
The vlc-ggi package includes the GGI plug-in for the VideoLAN client.
If you are going to watch DVD with the GGI video plugin, you should
install vlc-ggi

# codec plugins
%package mad
Summary: MAD audio codec plug-in for VideoLAN, a DVD and MPEG2 player
Group: Video
Requires: %{name} = %{version}
%description mad
The vlc-mad package includes the MAD (MPEG Audio Decoder) plug-in
for the VideoLAN client.
If you are going to watch DVD with the mad codec plugin, you should
install vlc-mad

# audio plugins
%package esd
Summary: Enlightened Sound Daemon audio plug-in for VideoLAN, a DVD and MPEG2 player
Group: Video
Requires: %{name} = %{version}
%description esd
The vlc-esd package includes the Enlightened Sound Daemon plug-in 
for the VideoLAN client.
If you are going to watch DVD with the esd audio plugin, you should
install vlc-esd

%package arts
Summary: aRts audio plug-in for VideoLAN, a DVD and MPEG2 player
Group: Video
Requires: %{name} = %{version}
%description arts
The vlc-arts package includes the aRts audio plug-in
for the VideoLAN client.
If you are going to watch DVD with the aRts audio plugin, you should
install vlc-arts

%package alsa
Summary: Advanced Linux Sound Architecture audio plug-in for VideoLAN, a DVD and MPGE2 player
Group: Video
Requires: %{name} = %{version}
%description alsa
The vlc-alsa packages includes the Advanced Linux Sound Architecture plug-in 
for the VideoLAN client.
If you are going to watch DVD with the ALSA audio plugin, you should 
install vlc-alsa

%prep
%if %{cvs}
%setup -q -n %{cvs_name}
%else
%setup -q -n %{name}-%{vlc_ver}
%endif

%build
%configure  --enable-release \
            --enable-dvd --without-dvdcss \
            --enable-gtk --enable-gnome --disable-qt --disable-kde --enable-ncurses \
            --enable-x11 --enable-xvideo --enable-ggi --enable-sdl --enable-fb --enable-mga --enable-aa \
            --enable-esd --enable-alsa --enable-arts \
	    --enable-mad
export QTDIR=%{_libdir}/qt2 
%make

%install
%makeinstall_std
install -d %buildroot/%_mandir/man1
install doc/vlc.1 %buildroot/%_mandir/man1

# menu
mkdir -p %buildroot/%{_menudir}
cat > %buildroot/%{_menudir}/vlc << EOF
?package(vlc): command="%{_bindir}/vlc" hotkey="V" needs="X11" longtitle="VideoLAN is a free MPEG, MPEG2 and DVD software solution" section="Multimedia/Video" title="VideoLAN Client" icon="vlc.png" hints="Video"
EOF
cat > %buildroot/%{_menudir}/vlc-gtk << EOF
?package(vlc-gtk): command="%{_bindir}/gvlc" needs="X11" longtitle="VideoLAN is a free MPEG, MPEG2 and DVD software solution" section="Multimedia/Video" title="Gtk VideoLAN Client" icon="gvlc.png" hints="Video"
EOF
cat > %buildroot/%{_menudir}/vlc-gnome << EOF
?package(vlc-gnome): command="%{_bindir}/gnome-vlc" needs="X11" longtitle="VideoLAN is a free MPEG, MPEG2 and DVD software solution" section="Multimedia/Video" title="Gnome VideoLAN Client" icon="gnome-vlc.png" hints="Video"
EOF
cat > %buildroot/%{_menudir}/vlc-qt << EOF
?package(vlc-gnome): command="%{_bindir}/qvlc" needs="X11" longtitle="VideoLAN is a free MPEG, MPEG2 and DVD software solution" section="Multimedia/Video" title="Qt VideoLAN Client" icon="qvlc.png" hints="Video"
EOF

# icons
mkdir -p %{buildroot}/{%{_miconsdir},%{_liconsdir}}
install -m 644 %buildroot/%_datadir/videolan/vlc16x16.png %buildroot/%{_miconsdir}/vlc.png
install -m 644 %buildroot/%_datadir/videolan/vlc32x32.png %buildroot/%{_iconsdir}/vlc.png
install -m 644 %buildroot/%_datadir/videolan/vlc48x48.png %buildroot/%{_liconsdir}/vlc.png
install -m 644 %buildroot/%_datadir/videolan/gnome-vlc16x16.png %buildroot/%{_miconsdir}/gnome-vlc.png
install -m 644 %buildroot/%_datadir/videolan/gnome-vlc32x32.png %buildroot/%{_iconsdir}/gnome-vlc.png
install -m 644 %buildroot/%_datadir/videolan/gnome-vlc48x48.png %buildroot/%{_liconsdir}/gnome-vlc.png
install -m 644 %buildroot/%_datadir/videolan/gvlc16x16.png %buildroot/%{_miconsdir}/gvlc.png
install -m 644 %buildroot/%_datadir/videolan/gvlc32x32.png %buildroot/%{_iconsdir}/gvlc.png
install -m 644 %buildroot/%_datadir/videolan/gvlc48x48.png %buildroot/%{_liconsdir}/gvlc.png
install -m 644 %buildroot/%_datadir/videolan/kvlc16x16.png %buildroot/%{_miconsdir}/kvlc.png
install -m 644 %buildroot/%_datadir/videolan/kvlc32x32.png %buildroot/%{_iconsdir}/kvlc.png
install -m 644 %buildroot/%_datadir/videolan/kvlc48x48.png %buildroot/%{_liconsdir}/kvlc.png
install -m 644 %buildroot/%_datadir/videolan/qvlc16x16.png %buildroot/%{_miconsdir}/qvlc.png
install -m 644 %buildroot/%_datadir/videolan/qvlc32x32.png %buildroot/%{_iconsdir}/qvlc.png
install -m 644 %buildroot/%_datadir/videolan/qvlc48x48.png %buildroot/%{_liconsdir}/qvlc.png

%post
%update_menus
%postun
%update_menus

%clean
rm -fr %buildroot

%files
%defattr(-,root,root)
%doc README COPYING
%{_bindir}/vlc

%dir %{_libdir}/videolan/vlc
%{_libdir}/videolan/vlc/ac3_spdif.so
%{_libdir}/videolan/vlc/dsp.so
%{_libdir}/videolan/vlc/dummy.so
%{_libdir}/videolan/vlc/dvd.so
%{_libdir}/videolan/vlc/fb.so
%{_libdir}/videolan/vlc/file.so
%{_libdir}/videolan/vlc/filter_deinterlace.so
%{_libdir}/videolan/vlc/filter_distort.so
%{_libdir}/videolan/vlc/filter_invert.so
%{_libdir}/videolan/vlc/filter_transform.so
%{_libdir}/videolan/vlc/filter_wall.so
%{_libdir}/videolan/vlc/fx_scope.so
%{_libdir}/videolan/vlc/http.so
%{_libdir}/videolan/vlc/ipv4.so
%{_libdir}/videolan/vlc/ipv6.so
%{_libdir}/videolan/vlc/logger.so
%{_libdir}/videolan/vlc/lpcm_adec.so
%{_libdir}/videolan/vlc/memcpy.so
%{_libdir}/videolan/vlc/mga.so
%{_libdir}/videolan/vlc/mpeg_es.so
%{_libdir}/videolan/vlc/mpeg_ps.so
%{_libdir}/videolan/vlc/mpeg_ts.so
%{_libdir}/videolan/vlc/null.so
%{_libdir}/videolan/vlc/rc.so
%{_libdir}/videolan/vlc/spudec.so
%{_libdir}/videolan/vlc/udp.so
%{_libdir}/videolan/vlc/vcd.so
%{_libdir}/videolan/vlc/x11.so
%{_libdir}/videolan/vlc/xmga.so

%{_mandir}/man1/*
%{_menudir}/vlc
%{_miconsdir}/vlc.png
%{_iconsdir}/vlc.png
%{_liconsdir}/vlc.png


# intf plugins
%files gtk
%defattr(-,root,root)
%doc README
%{_libdir}/videolan/vlc/gtk.so
%{_bindir}/gvlc
%{_menudir}/vlc-gtk
%{_miconsdir}/gvlc.png
%{_iconsdir}/gvlc.png
%{_liconsdir}/gvlc.png
%post gtk
%update_menus
%postun gtk
%update_menus

%files gnome
%defattr(-,root,root)
%doc README
%{_libdir}/videolan/vlc/gnome.so
%{_bindir}/gnome-vlc
%{_menudir}/vlc-gnome
%{_miconsdir}/gnome-vlc.png
%{_iconsdir}/gnome-vlc.png
%{_liconsdir}/gnome-vlc.png
%post   gnome
%update_menus
%postun gnome
%update_menus

%if %{plugin_qt}
%files qt
%defattr(-,root,root)
%doc README
%{_libdir}/videolan/vlc/qt.so
%{_bindir}/qvlc
%{_menudir}/vlc-qt
%{_miconsdir}/qvlc.png
%{_iconsdir}/qvlc.png
%{_liconsdir}/qvlc.png
%post   qt
%update_menus
%postun qt
%update_menus
%endif

%files ncurses
%defattr(-,root,root)
%doc README
%{_libdir}/videolan/vlc/ncurses.so

# video plugins
%files sdl
%defattr(-,root,root)
%doc README
%{_libdir}/videolan/vlc/sdl.so

%files ggi
%defattr(-,root,root)
%doc README
%{_libdir}/videolan/vlc/ggi.so

%files aa
%defattr(-,root,root)
%doc README
%{_libdir}/videolan/vlc/aa.so

# codec plugin
%files mad
%defattr(-,root,root)
%doc README
%{_libdir}/videolan/vlc/mad.so

#audio plugins
%files esd
%defattr(-,root,root)
%doc README
%{_libdir}/videolan/vlc/esd.so

%files arts
%defattr(-,root,root)
%doc README
%{_libdir}/videolan/vlc/arts.so

%files alsa
%defattr(-,root,root)
%doc README
%{_libdir}/videolan/vlc/alsa.so

%changelog
* Thu Apr 18 2002 Samuel Hocevar <sam@zoy.org> 0.3.1
- version 0.3.1.
- patch0 is no longer needed.

* Wed Apr 17 2002 Yves Duret <yduret@mandrakesoft.com> 0.3.1-1mdk
- version 0.3.1 (hum soon)
- removed old %%ifarch ppc

* Wed Apr 17 2002 Yves Duret <yduret@mandrakesoft.com> 0.3.0-4mdk
- added liba52 support (buildrequires).
- added vlc-alsa audio plugin.
- mad is a codec (audio) plugin. corrected description and summary.

* Wed Apr 10 2002 Yves Duret <yduret@mandrakesoft.com> 0.3.0-3mdk
- added patch0 from CVS: fix crashing GTK popup menus thx Michal Bukovjan <bukovjan@mbox.dkm.cz>

* Wed Apr 10 2002 Yves Duret <yduret@mandrakesoft.com> 0.3.0-2mdk
- added vlc-arts rpm plugin thx blindauer Emmanuel <manu@agat.net>
- better summary for plug-in
- add packager tag to myself

* Sun Apr 07 2002 Yves Duret <yduret@mandrakesoft.com> 0.3.0-1mdk
- added aa (Asci Art) plugin in vlc-aa rpm
- merged with sam's one:
  * using his plugins list into %%files
  * removed libdvdcss from the whole tarball.
  * removed the workaround for vlc's bad /dev/dsp detection.
- few spelling corrections in all %%description
- added buildrequires on SDL-devel

* Thu Apr 06 2002 Samuel Hocevar <sam@zoy.org> 0.3.0
- version 0.3.0.
- removed libdvdcss from the whole tarball.
- removed the workaround for vlc's bad /dev/dsp detection.

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
- added qt2 plug-in (vlc-qt)

* Wed May 16 2001 Yves Duret <yduret@mandrakesoft.com> 0.2.73-1mdk
- version 0.2.73
- you can now get decss threw a plug-in
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
