# ***************************************************************************
# Makefile : Build vlc-contrib files
# ***************************************************************************
# Copyright (C) 2003 VideoLAN
# $Id: Makefile,v 1.2 2003/11/15 13:09:02 massiot Exp $
# 
# Authors: Christophe Massiot <massiot@via.ecp.fr>
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

BUILDDIRS = bin doc etc include info lib man sbin share vlc-lib

all: FORCE
	@echo "Do you mean to make src or make bin ?"

src: FORCE
	$(MAKE) -C src all

contrib-macosx.tar.bz2: FORCE
	$(WGET) $(CONTRIB_URL)

bin: contrib-macosx.tar.bz2
	bunzip2 -c $< | tar xf -

clean-src: FORCE
	$(MAKE) -C src clean-src

clean-bin: FORCE
	rm -rf $(BUILDDIRS)
	$(MAKE) -C src clean-dots

clean: FORCE
	@echo "Do you mean to clean-src or clean-bin ?"

package: FORCE
	tar cf - bin include lib vlc-lib share/aclocal* share/autoconf* \
		share/automake* share/gettext* \
		| bzip2 -c > contrib.tar.bz2

FORCE:
