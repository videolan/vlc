Name: vlc
Version: 0.1.99d
Release: 1
Copyright: GPL
Url: http://www.videolan.org/
Group: X11/Applications/Graphics
Source0: http://www.videolan.org/packages/0.1.99d/vlc-0.1.99d.tar.gz
Packager: Samuel Hocevar <sam@via.ecp.fr>

Buildroot: /tmp/vlc-build
Summary: VideoLAN Client.

%changelog
* Sun Jun 18 2000 Samuel Hocevar <sam@via.ecp.fr>
Took over the package

* Thu Jun 15 2000 Eric Doutreleau <Eric.Doutreleau@int-evry.fr>
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
./configure --prefix=/usr --enable-mmx --enable-gnome
make
%install
mkdir -p $RPM_BUILD_ROOT/usr/lib
mkdir -p $RPM_BUILD_ROOT/usr/bin
make install prefix=$RPM_BUILD_ROOT/usr

%files
%attr(-, root, root) /usr/bin/vlc
%attr(-, root, root) /usr/share/videolan/vlc
%attr(-, root, root) /usr/lib/videolan
%attr(-, root, root) %doc AUTHORS COPYING INSTALL NEWS README doc
%clean
rm -rf $RPM_BUILD_ROOT

