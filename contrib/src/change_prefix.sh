#!/bin/sh
# ***************************************************************************
# change_prefix.sh : allow to transfer a contrib dir
# ***************************************************************************
# Copyright © 2012 VideoLAN and its authors
#
# Authors: Rafaël Carré
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

set -e

LANG=C
export LANG

if test "$1" = "-h" -o "$1" = "--help" -o $# -gt 2; then
  echo "Usage: $0 [old prefix] [new prefix]

Without arguments, this script assumes old prefix = @@CONTRIB_PREFIX@@,
and new prefix = current directory.
"
fi

if [ $# != 2 ]
then
    old_prefix=@@CONTRIB_PREFIX@@
    new_prefix=`pwd`
else
    old_prefix=$1
    new_prefix=$2
fi

# process [dir] [filemask] [text only]
process() {
    for file in `find $1 \( ! -name \`basename $1\` -o -type f \) -prune -type f -name "$2"`
    do
        if [ -n "$3" ]
        then
            file $file | sed "s/^.*: //" | grep -q 'text\|shell' || continue
        fi
        echo "Fixing up $file"
        sed -i.orig -e "s,$old_prefix,$new_prefix,g" $file
        rm -f $file.orig
    done
}

process bin/ "*" check
process lib/ "*.la"
process lib/pkgconfig/ "*.pc"
