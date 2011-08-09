# ***************************************************************************
# Makefile : Build vlc-contrib files
# ***************************************************************************
# Copyright (C) 2003-2011 the VideoLAN team
# $Id$
# 
# Authors: Christophe Massiot <massiot@via.ecp.fr>
#          Derk-Jan Hartman <hartman at videolan dot org>
#          Christophe Mutricy <xtophe at videolan dot org>
#          Felix Paul Kühne <fkuehne at videolan dot org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or    
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
# ***************************************************************************

include ./config.mak

BUILDDIRS = hosts build

ifdef HAVE_MACOSX
TARGETALL=using-bin
else
TARGETALL=using-src
endif

all: $(TARGETALL)

using-src:
	$(MAKE) -C build-src tools
	$(MAKE) -C build-src

ifdef HAVE_MACOSX_DARWIN_10
	(cd $(PREFIX)/lib && sed -e 's%/usr/lib/libiconv.la%$(PREFIX)/lib/libiconv.la%g' -i.orig *.la && rm -f *.la.orig)
endif

# shortcut
src: using-src

ifdef HAVE_DARWIN_OS

CONTRIBREV=50

contrib-macosx-$(ARCH)-$(CONTRIBREV).tar.bz2:
	$(WGET) http://downloads.videolan.org/pub/videolan/testing/contrib/macosx/$@

.$(CONTRIBREV): contrib-macosx-$(ARCH)-$(CONTRIBREV).tar.bz2
	@if test -d tmp; then \
		echo "Move away ./tmp, it's in the way" ; \
		exit 1 ; \
	fi
	mkdir tmp
	mkdir -p $(PREFIX)
	(cd tmp && tar jxvf ../$<)
	$(SRCDIR)/change_prefix.sh tmp @@CONTRIB_PREFIX@@  $(PREFIX)
	(cd tmp && find . -type d) | while read dir; do mkdir -p -- "$(PREFIX)/$$dir"; done
	(cd tmp && find . -not -type d) | while read i; do mv -f -- tmp/"$$i" "$(PREFIX)/$$i"; done
	rm -rf tmp
	# install the gecko-sdk, which isn't part of the package for size and speed reasons
	(cd build-src && rm -rf *gecko* && $(MAKE) .gecko)
    # libiconv.la is no longer present on Snow Leopard, so fix possible references to it, which would
    # result in linking issues
ifdef HAVE_MACOSX_DARWIN_10
	(cd $(PREFIX)/lib && sed -e 's%/usr/lib/libiconv.la%$(PREFIX)/lib/libiconv.la%g' -i.orig *.la && rm -f *.la.orig)
	(cd build-src && rm -f .iconv && $(MAKE) .iconv-from-os)
endif
	touch .$(CONTRIBREV)

using-bin: .$(CONTRIBREV) 

endif

clean:
	rm -rf $(BUILDDIRS)
	$(MAKE) -C build-src clean

clean-bin:
	rm -rf $(BUILDDIRS)
	$(MAKE) -C build-src clean-dots

distclean:
	rm -rf $(BUILDDIRS) config.mak distro.mak build-src toolchain.cmake Makefile

bin: using-bin

package-macosx:
	@if test -d tmp; then \
		echo "Move away ./tmp, it's in the way" ; \
		exit 1 ; \
	fi
	mkdir tmp
	(cd $(PREFIX); tar cf - Sparkle Growl BGHUDAppKit bin sbin include lib share/aclocal* share/autoconf* \
		share/automake* share/gettext* share/libtool*) | (cd tmp; tar xf -)
	./change_prefix.sh tmp $(PREFIX) @@CONTRIB_PREFIX@@
	(cd tmp; tar cf - .) | bzip2 -c > contrib-macosx.tar.bz2
	rm -rf tmp
	rm -f contrib-macosx-$(ARCH)-$(CONTRIBREV).tar.bz2
	mv contrib-macosx.tar.bz2 contrib-macosx-$(ARCH)-$(CONTRIBREV).tar.bz2

DISTDIR = usr/win$*

package-win%:
	@if test -d tmp; then \
		echo "Move away ./tmp, it's in the way" ; \
		exit 1 ; \
	fi
	mkdir -p tmp/$(DISTDIR)
	(cd $(PREFIX); tar cf - --dereference bin sbin include lib share/aclocal*\
		share/autoconf* share/qt4* \
		share/automake* share/gettext* gecko-sdk)\
		| (cd tmp/$(DISTDIR); tar xpf -)
#kludge for live.com
	mkdir -p tmp/$(DISTDIR)/live.com
	for i in groupsock liveMedia UsageEnvironment BasicUsageEnvironment; do \
		mkdir -p  tmp/$(DISTDIR)/live.com/$$i/include; \
		cp -r build-src/live/$$i/include tmp/$(DISTDIR)/live.com/$$i; \
		cp build-src/live/$$i/lib$${i}.a  tmp/$(DISTDIR)/live.com/$$i; \
	done;
# Change Prefix.
	./change_prefix.sh tmp $(PREFIX) $(DISTDIR)
# Remove unused and potentially harmful files (but skip qt4 executables)
	(cd tmp/$(DISTDIR)/bin && rm -fv `find . -name 'uic.exe' -o -name 'rcc.exe' -o -name 'moc.exe' -o -name '*.exe' -printf '%p '` && chmod a+x * || true)
# Tar it.
	(cd tmp; tar cf - $(DISTDIR)) | bzip2 -c > contrib-`date +%Y%m%d`-win$*-bin-gcc-`$(CC) --version|head -n 1|cut -f 3 -d ' '`-runtime-`/bin/echo -e "#include <_mingw.h>\n#define CONCAT2(a,b) a##b\n#define CONCAT(a,b) CONCAT2(a,b)\n#ifdef __MINGW64_VERSION_MAJOR\nCONCAT(CONCAT(__MINGW64_VERSION_MAJOR,.),__MINGW64_VERSION_MINOR)\n#else\n__MINGW32_VERSION\n#endif"|$(CC) -E -|tail -1`-only.tar.bz2
	rm -rf tmp

.PHONY: all clean-src clean-bin clean package-macosx
