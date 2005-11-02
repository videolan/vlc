# ***************************************************************************
# Makefile : Build vlc-contrib files
# ***************************************************************************
# Copyright (C) 2003-2005 the VideoLAN team
# $Id$
# 
# Authors: Christophe Massiot <massiot@via.ecp.fr>
#          Derk-Jan Hartman <hartman at videolan dot org>
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
# ***************************************************************************

include ./config.mak

BUILDDIRS = bin doc etc include info lib man sbin share vlc-lib tmp

all: FORCE
#	@echo "Do you mean to make src or make bin ?"
	@echo "    'make bin' isn't functional at the moment,"
	@echo "    running 'make src' instead..."
	$(MAKE) src

src: FORCE
	$(MAKE) -C src all

contrib-macosx.tar.bz2: FORCE
	$(WGET) $(CONTRIB_URL)

bin: FORCE
	@echo "make bin isn't in a functional state at this moment. Please use make src"

#The disabled make bin
#bin: contrib-macosx.tar.bz2
#	@if test -d tmp; then \
#		echo "Move away ./tmp, it's in the way" ; \
#		exit 1 ; \
#	fi
#	mkdir tmp
#	bunzip2 -c $< | (cd tmp; tar xf -)
#	./change_prefix.sh tmp @@CONTRIB_PREFIX@@ $(PREFIX)
#	(cd tmp; tar cf - .) | tar xf -
#	rm -rf tmp
#	ranlib lib/*\.a

clean-src: FORCE
	rm -rf $(BUILDDIRS)
	$(MAKE) -C src clean-src

clean-bin: FORCE
	rm -rf $(BUILDDIRS)
	$(MAKE) -C src clean-dots

clean: FORCE
#	@echo "    Do you mean to clean-src or clean-bin ?"
	@echo "    'make bin' isn't available at the moment"
	@echo "    you probably wanna run 'make clean-src' then"
	@echo "    you can still run 'make clean-bin' manually if needed"
	$(MAKE) clean-src

package: FORCE
	@if test -d tmp; then \
		echo "Move away ./tmp, it's in the way" ; \
		exit 1 ; \
	fi
	mkdir tmp
	tar cf - bin sbin include lib vlc-lib share/aclocal* share/autoconf* \
		share/automake* share/gettext* \
		| (cd tmp; tar xf -)
	./change_prefix.sh tmp $(PREFIX) @@CONTRIB_PREFIX@@
	(cd tmp; tar cf - .) | bzip2 -c > contrib-macosx.tar.bz2
	rm -rf tmp

FORCE:
