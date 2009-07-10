%define name python-vlc
%define version 1.0.0.90
%define unmangled_version 1.0.0.90
%define release 1
%{!?python_sitelib: %global python_sitelib %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib()")}

Summary: VLC bindings for python.
Name: %{name}
Version: %{version}
Release: %{release}
Source0: %{name}-%{unmangled_version}.tar.gz
License: GPL
Group: Development/Libraries
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
Prefix: %{_prefix}
Vendor: Olivier Aubert <olivier.aubert@liris.cnrs.fr>
Url: http://wiki.videolan.org/PythonBinding

%description
VLC bindings for python.

This module provides bindings for the native libvlc API of the VLC
video player. Documentation can be found on the VLC wiki :
http://wiki.videolan.org/ExternalAPI

This module also provides a MediaControl object, which implements an
API inspired from the OMG Audio/Video Stream 1.0 specification.
Documentation can be found on the VLC wiki :
http://wiki.videolan.org/PythonBinding

Example session (for the MediaControl API):

import vlc
mc=vlc.MediaControl(['--verbose', '1'])
mc.playlist_add_item('movie.mpg')

# Start the movie at 2000ms
p=vlc.Position()
p.origin=vlc.RelativePosition
p.key=vlc.MediaTime
p.value=2000
mc.start(p)
# which could be abbreviated as
# mc.start(2000)
# for the default conversion from int is to make a RelativePosition in MediaTime

# Display some text during 2000ms
mc.display_text('Some useless information', 0, 2000)

# Pause the video
mc.pause(0)

# Get status information
mc.get_stream_information()
       

%prep
%setup -n %{name}-%{unmangled_version}

%build
env CFLAGS="$RPM_OPT_FLAGS" python setup.py build

%install
python setup.py install --root=$RPM_BUILD_ROOT --record=INSTALLED_FILES

%clean
rm -rf $RPM_BUILD_ROOT

%files -f INSTALLED_FILES
%defattr(-,root,root)
%{python_sitelib}/vlcwidget.pyo
