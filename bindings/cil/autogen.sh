#!/bin/sh
# SVN package rebuild script
# $Id$
#
# ***********************************************************************
# *  Copyright © 2002-2005 Rémi Denis-Courmont.                         *
# *  This program is free software; you can redistribute and/or modify  *
# *  it under the terms of the GNU General Public License as published  *
# *  by the Free Software Foundation; version 2 of the license.         *
# *                                                                     *
# *  This program is distributed in the hope that it will be useful,    *
# *  but WITHOUT ANY WARRANTY; without even the implied warranty of     *
# *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.               *
# *  See the GNU General Public License for more details.               *
# *                                                                     *
# *  You should have received a copy of the GNU General Public License  *
# *  along with this program; if not, you can get it from:              *
# *  http://www.gnu.org/copyleft/gpl.html                               *
# ***********************************************************************

set -xe

cd "$(dirname "$0")"

test -f src/exception.cs || {
	echo "You must run this script from your cil directory.">&2
	exit 1
}

mkdir -p admin
autoreconf -sfi

set +x
echo ""
echo "Type \`./configure' to configure the package for your system"
echo "(type \`./configure -- help' for help)."
echo "Then you can use the usual \`make', \`make install', etc."

