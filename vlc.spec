

%define name 		vlc
%define vlc_ver 	0.4.0
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
%define	plugin_lirc	1

Summary:	VideoLAN is a free multimedia software solution.
Name:		%{name}
Version:	%{version}
Release:	%{release}
Packager:	Yves Duret <yduret@mandrakesoft.com>

%if %{cvs} 
Source0:	http://www.videolan.org/pub/videolan/vlc/snapshots/%{cvs_name}.tar.bz2
%else
Source0:	http://www.videolan.org/packages/%{version}/%{name}-%{version}.tar.bz2
%endif
License:	GPL
Group:		Video
URL:		http://www.videolan.org/
Requires:	vlc-gui
# yves 0.4.0-1mdk needed by ffmpeg builtin (i want MPEG4 support out of box)
Requires:	vlc-mad

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
Buildrequires:	liblirc-devel
Buildrequires:	libffmpeg-devel

%description
VideoLAN is a free network-aware MPEG1, MPEG2, MPEG4 (aka DivX)
and DVD player.
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
Summary: Gtk plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
Provides: vlc-gui
%description gtk
VideoLAN is a free multimedia software solution.

This plugin adds a Gtk+ interface to vlc, the VideoLAN Client. To
activate it, use the `--intf gtk' flag or run the `gvlc' program.

%package gnome
Summary: Gnome plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
Provides: vlc-gui
%description gnome
VideoLAN is a free multimedia software solution.

This plugin adds a Gnome interface to vlc, the VideoLAN Client. To
activate it, use the `--intf gnome' flag or run the `gnome-vlc' program.

%package qt
Summary: Qt2 plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
Provides: vlc-gui
%description qt
VideoLAN is a free multimedia software solution.

This plugin adds a Qt interface to vlc, the VideoLAN Client. To
activate it, use the `--intf qt' flag or run the `qvlc' program.

%package ncurses
Summary: Ncurses console-based plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
%description ncurses
VideoLAN is a free multimedia software solution.

This plugin adds a ncurses interface to vlc, the VideoLAN Client. To
activate it, use the `--intf ncurses' flag.

%package lirc
Summary: Lirc plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
%description lirc
VideoLAN is a free multimedia software solution.

This plugin is an infrared lirc interface for vlc, the
VideoLAN Client. To activate it, use the `--intf lirc' flag.


# video plugins
%package aa
Summary: ASCII art video plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
%description aa
VideoLAN is a free multimedia software solution.

This is an ASCII art video output plugin for vlc, the VideoLAN
Client. To activate it, use the `--vout aa' flag or select the `aa'
vout plugin from the preferences menu.


%package sdl
Summary: Simple DirectMedia Layer video plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
%description sdl
VideoLAN is a free multimedia software solution.

This plugin adds support for the Simple DirectMedia Layer library to
vlc, the VideoLAN Client. To activate it, use the `--vout sdl' or
`--aout sdl' flags or select the `sdl' vout or aout plugin from the
preferences menu.

%package ggi
Summary: GGI video plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
%description ggi
VideoLAN is a free multimedia software solution.

This is a GGI plugin for vlc, the VideoLAN Client.  To activate it, use
the `--vout ggi' flag or select the `ggi' vout plugin from the preferences
menu.
     
# codec plugins
%package mad
Summary: MAD audio codec plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
%description mad
VideoLAN is a free multimedia software solution.

This plugin adds support for libmad, the MPEG audio decoder library,
to the VideoLAN Client. MAD is 100% fixed-point based. To activate
this plugin, use the `--mpeg_adec mad' flag or select the `mad' MPEG
decoder from the preferences menu.

# audio plugins
%package esd
Summary: Enlightened Sound Daemon audio plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
%description esd
VideoLAN is a free multimedia software solution.

This plugin adds support for the Enlightened Sound Daemon to vlc, the
VideoLAN Client. To activate it, use the `--aout esd' flag or select
the `esd' aout plugin from the preferences menu.

%package arts
Summary: aRts audio plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
%description arts
VideoLAN is a free multimedia software solution.

This plugin adds support for the aRts Sound System to vlc, the
VideoLAN Client. To activate it, use the `--aout arts' flag or
select the `arts' aout plugin from the preferences menu.

%package alsa
Summary: Advanced Linux Sound Architecture audio plugin for the VideoLAN client
Group: Video
Requires: %{name} = %{version}
%description alsa
VideoLAN is a free multimedia software solution.

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
# ffmpeg: no plugin posible on ia64 due to the static linking (can not put .a in a .so)
%configure  --enable-release \
            --enable-dvd --without-dvdcss \
            --enable-gtk --enable-gnome --disable-qt --disable-kde --enable-ncurses --enable-lirc \
            --enable-x11 --enable-xvideo --enable-ggi --enable-sdl --enable-fb --enable-mga --enable-aa \
            --enable-esd --enable-alsa --enable-arts \
	    --enable-mad --enable-ffmpeg --with-ffmpeg=/usr
export QTDIR=%{_libdir}/qt2 
%make

%install
%makeinstall_std
install -d %buildroot/%_mandir/man1
install doc/vlc.1 %buildroot/%_mandir/man1

# menu
mkdir -p %buildroot/%{_menudir}
cat > %buildroot/%{_menudir}/vlc << EOF
?package(vlc): command="%{_bindir}/vlc" hotkey="V" needs="X11" longtitle="VideoLAN is a free multimedia software solution" section="Multimedia/Video" title="VideoLAN Client" icon="vlc.png" hints="Video"
EOF
cat > %buildroot/%{_menudir}/vlc-gtk << EOF
?package(vlc-gtk): command="%{_bindir}/gvlc" needs="X11" longtitle="VideoLAN is a free multimedia software solution" section="Multimedia/Video" title="Gtk VideoLAN Client" icon="gvlc.png" hints="Video"
EOF
cat > %buildroot/%{_menudir}/vlc-gnome << EOF
?package(vlc-gnome): command="%{_bindir}/gnome-vlc" needs="X11" longtitle="VideoLAN is a free multimedia software solution" section="Multimedia/Video" title="Gnome VideoLAN Client" icon="gnome-vlc.png" hints="Video"
EOF
cat > %buildroot/%{_menudir}/vlc-qt << EOF
?package(vlc-gnome): command="%{_bindir}/qvlc" needs="X11" longtitle="VideoLAN is a free multimedia software solution" section="Multimedia/Video" title="Qt VideoLAN Client" icon="qvlc.png" hints="Video"
EOF

# icons
mkdir -p %{buildroot}/{%{_miconsdir},%{_liconsdir}}
install -m 644 %buildroot/%_datadir/vlc/vlc16x16.png %buildroot/%{_miconsdir}/vlc.png
install -m 644 %buildroot/%_datadir/vlc/vlc32x32.png %buildroot/%{_iconsdir}/vlc.png
install -m 644 %buildroot/%_datadir/vlc/vlc48x48.png %buildroot/%{_liconsdir}/vlc.png
install -m 644 %buildroot/%_datadir/vlc/gnome-vlc16x16.png %buildroot/%{_miconsdir}/gnome-vlc.png
install -m 644 %buildroot/%_datadir/vlc/gnome-vlc32x32.png %buildroot/%{_iconsdir}/gnome-vlc.png
install -m 644 %buildroot/%_datadir/vlc/gnome-vlc48x48.png %buildroot/%{_liconsdir}/gnome-vlc.png
install -m 644 %buildroot/%_datadir/vlc/gvlc16x16.png %buildroot/%{_miconsdir}/gvlc.png
install -m 644 %buildroot/%_datadir/vlc/gvlc32x32.png %buildroot/%{_iconsdir}/gvlc.png
install -m 644 %buildroot/%_datadir/vlc/gvlc48x48.png %buildroot/%{_liconsdir}/gvlc.png
install -m 644 %buildroot/%_datadir/vlc/kvlc16x16.png %buildroot/%{_miconsdir}/kvlc.png
install -m 644 %buildroot/%_datadir/vlc/kvlc32x32.png %buildroot/%{_iconsdir}/kvlc.png
install -m 644 %buildroot/%_datadir/vlc/kvlc48x48.png %buildroot/%{_liconsdir}/kvlc.png
install -m 644 %buildroot/%_datadir/vlc/qvlc16x16.png %buildroot/%{_miconsdir}/qvlc.png
install -m 644 %buildroot/%_datadir/vlc/qvlc32x32.png %buildroot/%{_iconsdir}/qvlc.png
install -m 644 %buildroot/%_datadir/vlc/qvlc48x48.png %buildroot/%{_liconsdir}/qvlc.png

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

%dir %{_libdir}/vlc
%{_libdir}/vlc/ac3_spdif.so
%{_libdir}/vlc/avi.so
%{_libdir}/vlc/dsp.so
%{_libdir}/vlc/dummy.so
%{_libdir}/vlc/dvd.so
%{_libdir}/vlc/fb.so
%{_libdir}/vlc/file.so
%{_libdir}/vlc/filter_clone.so
%{_libdir}/vlc/filter_crop.so
%{_libdir}/vlc/filter_deinterlace.so
%{_libdir}/vlc/filter_distort.so
%{_libdir}/vlc/filter_invert.so
%{_libdir}/vlc/filter_transform.so
%{_libdir}/vlc/filter_wall.so
%{_libdir}/vlc/fx_scope.so
%{_libdir}/vlc/http.so
%{_libdir}/vlc/ipv4.so
%{_libdir}/vlc/ipv6.so
%{_libdir}/vlc/logger.so
%{_libdir}/vlc/lpcm_adec.so
%{_libdir}/vlc/memcpy.so
%{_libdir}/vlc/mga.so
%{_libdir}/vlc/mpeg_es.so
%{_libdir}/vlc/mpeg_ps.so
%{_libdir}/vlc/mpeg_ts.so
%{_libdir}/vlc/null.so
%{_libdir}/vlc/rc.so
%{_libdir}/vlc/spudec.so
%{_libdir}/vlc/udp.so
%{_libdir}/vlc/vcd.so
%{_libdir}/vlc/x11.so
#%{_libdir}/vlc/xmga.so

%{_mandir}/man1/*
%{_menudir}/vlc
%{_miconsdir}/vlc.png
%{_iconsdir}/vlc.png
%{_liconsdir}/vlc.png


# intf plugins
%files gtk
%defattr(-,root,root)
%doc README
%{_libdir}/vlc/gtk.so
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
%{_libdir}/vlc/gnome.so
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
%{_libdir}/vlc/qt.so
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
%{_libdir}/vlc/ncurses.so

%if %plugin_lirc
%files lirc
%defattr(-,root,root)
%doc README
%{_libdir}/vlc/lirc.so
%endif

# video plugins
%files sdl
%defattr(-,root,root)
%doc README
%{_libdir}/vlc/sdl.so

%files ggi
%defattr(-,root,root)
%doc README
%{_libdir}/vlc/ggi.so

%files aa
%defattr(-,root,root)
%doc README
%{_libdir}/vlc/aa.so

# codec plugin
%files mad
%defattr(-,root,root)
%doc README
%{_libdir}/vlc/mad.so

#audio plugins
%files esd
%defattr(-,root,root)
%doc README
%{_libdir}/vlc/esd.so

%files arts
%defattr(-,root,root)
%doc README
%{_libdir}/vlc/arts.so

%files alsa
%defattr(-,root,root)
%doc README
%{_libdir}/vlc/alsa.so

%changelog
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
