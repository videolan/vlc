Name: vlc
Version: 0.2.81
Release: 2
Copyright: GPL
Url: http://www.videolan.org/
Group: X11/Applications/Graphics
Source0: http://www.videolan.org/packages/0.2.81/vlc-0.2.81.tar.gz
Prefix: /usr
Packager: Samuel Hocevar <sam@zoy.org>

Buildroot: /tmp/vlc-build
Summary: VideoLAN Client.
Summary(fr): Client VideoLAN.

%changelog
* Sat, Jul 28 2001 Samuel Hocevar <sam@zoy.org>
New upstream release (0.2.81)

* Tue Jun 5 2001 Samuel Hocevar <sam@zoy.org>
New upstream release (0.2.80)

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

%description
a free network-aware MPEG and DVD player
 VideoLAN is a free MPEG1/2 software solution.
 .
 The VideoLAN Client allows to play MPEG2 Transport Streams from the
 network or from a file, as well as direct DVD playback.

%description -l fr
Un lecteur MPEG et DVD utilisable en réseau.
VideoLAN est un lecteur MPEG1/2. Le client VideoLAN permet la lecture de
flux MPEG2 depuis le réseau ou depuis un fichier, en plus de la lecture
directe des DVD.

%prep
%setup 

%build
if [ -x %{prefix}/bin/kgcc ] ;
then
  CC=%{prefix}/bin/kgcc ./configure --prefix=%{prefix} --with-sdl --enable-esd --enable-gnome --enable-qt
else
  ./configure --prefix=%{prefix} --with-sdl --enable-esd --enable-gnome --enable-qt
fi
if [ -x %{prefix}/bin/kgcc ] ;
then
  CC=%{prefix}/bin/kgcc make
else
  make
fi
%install
mkdir -p $RPM_BUILD_ROOT%{prefix}/lib
mkdir -p $RPM_BUILD_ROOT%{prefix}/bin
make install prefix=$RPM_BUILD_ROOT%{prefix}

%files
%attr(-, root, root) %{prefix}/bin/vlc
%attr(-, root, root) %{prefix}/share/videolan
%attr(-, root, root) %{prefix}/lib/videolan
%attr(-, root, root) %doc ChangeLog AUTHORS COPYING INSTALL README TODO doc
%clean
rm -rf $RPM_BUILD_ROOT

