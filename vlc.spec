# This is borrowed and adapted from Mandrake's Cooker
%define name 	vlc
%define vlc_ver 0.2.90
%define version	%vlc_ver

# libdvdcss
%define major   0
%define lib_ver 0.0.3
%define lib_name libdvdcss%{major}

%define cvs     0

%if %{cvs}
%define cvsdate 20011002
%define release	0.%{cvsdate}
%define cvs_name %{name}-snapshot-%{cvsdate}-00
%else
%define release 1
%endif

Summary:	VideoLAN is a free MPEG, MPEG-2 and DVD software solution.
Name:		%{name}
Version:	%{version}
Release:	%{release}

%if %{cvs} 
Source0:	http://www.videolan.org/pub/videolan/vlc/snapshots/%{cvs_name}.tar.bz2
%else
Source0:	http://www.videolan.org/pub/videolan/vlc/%{version}/%{name}-%{version}.tar.bz2
%endif
License:	GPL
Group:		Video
URL:		http://videolan.org/
BuildRoot:	%_tmppath/%name-%version-%release-root
#This is for Mandrake :
#Buildrequires:	libncurses5-devel
#Buildrequires:	libqt2-devel
#Buildrequires:	libgtk+1.2-devel
#Buildrequires:	gnome-libs-devel
#Buildrequires:	db1-devel
#This is for RedHat :
Buildrequires: ncurses-devel
Buildrequires: qt-devel
Buildrequires: gtk+-devel
Buildrequires: gnome-libs-devel
Buildrequires: SDL-devel
Buildrequires: db1

%description
VideoLAN is a free network-aware MPEG and DVD player.
The VideoLAN Client allows to play MPEG-2 Transport Streams from the
network or from a file, as well as direct DVD playback.
VideoLAN is a project of students from the Ecole Centrale Paris.
This version add MPEG-1 support, direct DVD support, DVD decryption, 
arbitrary, seeking in the stream, pause, fast forward and slow motion, 
hardware YUV acceleration and a few new interface features 
including drag'n'drop.
You may install vlc-gnome, vlc-gtk and vlc-qt vlc-gnome vlc-ncurses.

%package gtk
Summary: Gtk plug-in for VideoLAN, a DVD and MPEG-2 player
Group: Video
Requires: %{name} = %{version}
%description gtk
The vlc-gtk packages includes the Gtk plug-in for the VideoLAN client.
If you are going to watch DVD with the Gtk front-end, you should 
install vlc-gtk.


%package gnome
Summary: Gnome plug-in for VideoLAN, a DVD and MPEG-2 player
Group: Video
Requires: %{name} = %{version}
%description gnome
The vlc-gnome packages includes the Gnome plug-in for the VideoLAN client.
If you are going to watch DVD with the Gnome front-end, you should 
install vlc-gnome.

%package qt
Summary: Qt2 plug-in for VideoLAN, a DVD and MPEG-2 player
Group: Video
Requires: %{name} = %{version}
%description qt
The vlc-qt packages includes the Qt2 plug-in for the VideoLAN client.
If you are going to watch DVD with the Qt2 front-end, you should
install vlc-qt

%package ncurses
Summary: Ncurses console-based plug-in for VideoLAN, a DVD and MPEG-2 player
Group: Video
Requires: %{name} = %{version}
%description ncurses
The vlc-ncurses packages includes the ncurses plug-in for the VideoLAN client.
If you are going to watch DVD with the ncurses front-end, you should
install vlc-ncurses


%prep
%if %{cvs}
%setup -q -n %{cvs_name}
%else
%setup -q -n %{name}-%{vlc_ver}
%endif

%build
export QTDIR=%{_libdir}/qt-2.3.0/
%configure --with-dvdcss=local-shared \
	   --enable-gnome --enable-gtk \
	   --enable-x11 --enable-qt --enable-ncurses \
	   --enable-esd --disable-alsa \
	   --enable-fb \
	   --enable-xvideo \
	   --enable-sdl 
make

%install
%makeinstall
install -d %buildroot/%_mandir/man1
install doc/vlc.1 %buildroot/%_mandir/man1

%clean
rm -fr %buildroot

%files
%defattr(-,root,root,-)
%doc README COPYING
%{_bindir}/vlc

%{_libdir}/videolan/vlc/dsp.so
%{_libdir}/videolan/vlc/esd.so
%{_libdir}/videolan/vlc/fb.so
%{_libdir}/videolan/vlc/sdl.so
%{_libdir}/videolan/vlc/x*.so

%dir %{_datadir}/videolan
%{_datadir}/videolan/*
%{_mandir}/man1/*

%files gtk
%defattr(-,root,root)
%{_libdir}/videolan/vlc/gtk.so
%{_bindir}/gvlc

%files gnome
%defattr(-,root,root)
%{_libdir}/videolan/vlc/gnome.so
%{_bindir}/gnome-vlc

%files qt
%defattr(-,root,root)
%{_libdir}/videolan/vlc/qt.so
%{_bindir}/qvlc

%files ncurses
%defattr(-,root,root)
%{_libdir}/videolan/vlc/ncurses.so


%changelog
* Wed Oct 10 2001 Christophe Massiot <massiot@via.ecp.fr> 0.2.90-1
- version 0.2.90

* Tue Oct 02 2001 Christophe Massiot <massiot@via.ecp.fr>
- Imported Mandrake's vlc.spec into the CVS

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
