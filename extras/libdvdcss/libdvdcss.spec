# This is borrowed and adapted from Mandrake's Cooker
%define name 	libdvdcss
%define version	0.0.3
%define release	1
%define major  	0
%define lib_name %{name}%{major}

Summary:	A library for accessing DVDs like block device, deciphering CSS encryption if needed.
Name:		%{name}
Version:	%{version}
Release:	%{release}

Source0:	http://www.videolan.org/pub/videolan/libdvcss/%{version}/%{name}-%{version}.tar.bz2
License:	GPL
Group:		System/Libraries
URL:		http://videolan.org/
BuildRoot:	%_tmppath/%name-%version-%release-root

%description
libdvdcss is a simple library designed for accessing DVDs like a block device
without having to bother about the decryption. The important features are:
 * Portability. Currently supported platforms are GNU/Linux, FreeBSD, BeOS
   and Windows. The MacOS X version is being worked on as well.
 * Simplicity. There are currently 7 functions in the API, and we intend to
   keep this number low.
 * Freedom. libdvdcss is released under the General Public License, ensuring
   it will stay free, and used only for free software products.
 * Just better. Unlike most similar projects, libdvdcss doesn't require the
   region of your drive to be set.

%package -n %{lib_name}
Summary:        A library for accessing DVDs like block device, deciphering CSS encryption if needed.
Group:          System/Libraries
Provides:       %name

%description -n %{lib_name}
libdvdcss is a simple library designed for accessing DVDs like a block device 
without having to bother about the decryption. The important features are:
 * Portability. Currently supported platforms are GNU/Linux, FreeBSD, BeOS
   and Windows. The MacOS X version is being worked on as well.
 * Simplicity. There are currently 7 functions in the API, and we intend to 
   keep this number low.
 * Freedom. libdvdcss is released under the General Public License, ensuring
   it will stay free, and used only for free software products.
 * Just better. Unlike most similar projects, libdvdcss doesn't require the 
   region of your drive to be set.

%package -n %{lib_name}-devel
Summary:        Development tools for programs which will use the libdvdcss library.
Group:          Development/C
Provides:       %name-devel
Requires:	%{lib_name} = %{version}
 
%description -n %{lib_name}-devel
The %{name}-devel package includes the header files and static libraries
necessary for developing programs which will manipulate DVDs files using
the %{name} library.
 
 If you are going to develop programs which will manipulate DVDs,
 you should install %{name}-devel.  You'll also need to have the %name
 package installed.
 

%prep
%setup -q

%build
%configure
%make

%install
%makeinstall

%clean
rm -fr %buildroot

%post -n %{lib_name} -p /sbin/ldconfig
 
%postun -n %{lib_name} -p /sbin/ldconfig

%files -n %{lib_name}
%defattr(-,root,root,-)
%doc COPYING AUTHORS
%{_libdir}/*.so.*

%files -n %{lib_name}-devel
%defattr(-,root,root)
%doc COPYING
%{_libdir}/*.a
%{_libdir}/*.so
%{_includedir}/*

%changelog
* Tue Oct 02 2001 Christophe Massiot <massiot@via.ecp.fr>
- Imported Mandrake's vlc.spec into the CVS

* Thu Aug 23 2001 Yves Duret <yduret@mandrakesoft.com> 0.0.3-1mdk
- version 0.0.3

* Mon Aug 13 2001 Yves Duret <yduret@mandrakesoft.com> 0.0.2-1mdk
- version 0.0.2

* Tue Jun 19 2001 Yves Duret <yduret@mandrakesoft.com> 0.0.1-1mdk
- first release and first mdk release

#EOF
