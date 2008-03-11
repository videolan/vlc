#!/bin/sh
# This script install all 3rd party dependencies for ubuntu
# from the medibuntu repository.

LIST="\
libcaca0 \
libflac7 \
libfreetype6 \
libfribidi0 \
libgcrypt11 \
libgpg-error0 \
libgnutls13 \
libgtk2.0-0 \
libid3tag0 \
libmad0 \
libmpcdec3 \
libnspr4 \
libnss3 \
libnotify1 \
libdbus-1-3 \
libhal1 \
libogg0 \
libsdl1.2debian \
libsdl-image1.2 \
libsdl-net1.2 \
libsdl-mixer1.2 \
libvorbis0a \
libvorbisenc2 \
libshout3 \
libspeex1 \
libtheorai0 \
libsmbclient \
libxml2 \
libmodplug0c2 \
libdvdnav4 \
libdvdcss2 \
libebml-dev \
libfaac0 \
libfaad2-0 \
liblame0 \
libmatroska-dev \
libmpeg2-4 \
liba52-0.7.4 \
libwxbase2.6-0 \
libwxgtk2.6-0 \
libx264-dev \
libtwolame0 \
libdts-dev \
libdirac0c2a"

# Are we been run on ubuntu feiste ?
if ! test -f /etc/debian_version; then
   echo "ERROR: no /etc/debian_version file found"
   echo "ERROR: this is a non debian based system."
   echo "ERROR: this script is meant to be run on ubuntu feisty 7.04"
   exit 1
fi

version=`cat /etc/debian_version`
if ! test "${version}" = "4.0"; then
   echo "ERROR: wrong version number ${version}"
   echo "ERROR: this script is meant to be run on ubuntu feisty 7.04"
   exit 1
fi

if ! test "`whoami`" = "root"; then 
   echo "ERROR: run this script as root user (eg: using sudo $*)"
   exit 1
fi

# Test if medibuntu repos is installed.
if ! test -f /etc/apt/sources.list.d/medibuntu.list; then
   echo "Installing medibuntu repository for dependencies"
   wget http://www.medibuntu.org/sources.list.d/feisty.list -O /etc/apt/sources.list.d/medibuntu.list
   wget -q http://packages.medibuntu.org/medibuntu-key.gpg -O- | sudo apt-key add - && sudo apt-get update
   echo "done"
fi

apt-get install ${LIST}

exit $?
