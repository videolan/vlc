Name: vlc
Version: 0.1.99c
Release: 1
Copyright: GPL
Url: http://www.videolan.org/
Group: X11/Applications/Graphics
Source0: http://www.videolan.org/packages/vlc-0.1.99c.tar.gz
Packager: Eric Doutreleau <Eric.doutreleau@int-evry.fr>

Buildroot: /tmp/vlc-build
Summary: VideoLAN Client.

%changelog
* Thu Jun 15 2000 Eric Doutreleau < Eric.Doutreleau@int-evry.fr>
Initial package

%description
a free network-aware MPEG and DVD player
 VideoLAN is a free MPEG2 software solution.
 .
 The VideoLAN Client allows to play MPEG2 Transport Streams from the
 network or from a file, as well as direct DVD playback.

%prep
%setup 

%build
./configure --prefix=/usr --enable-ppro --enable-mmx --enable-gnome
make
%install
mkdir -p $RPM_BUILD_ROOT/usr/lib
mkdir -p $RPM_BUILD_ROOT/usr/bin
make install prefix=$RPM_BUILD_ROOT/usr

%files
/usr/bin/vlc
/usr/share/videolan/vlc
/usr/lib/videolan
%doc AUTHORS COPYING INSTALL NEWS README doc
%clean
rm -rf $RPM_BUILD_ROOT

