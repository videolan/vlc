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

usage() {
  echo "Usage: $0 [prefix]

 * If a prefix is provided, this script will replaces any of its occurences
   with its own internal value to be able to replace the prefixes when using a
   prebuild package.
   If the .pc file contains a prefix which doesn't match the provided one, this
   script will error out
 * If no prefix is provided, this script will replace its internal value with
   the current working directory
"
}

if test "$1" = "-h" -o "$1" = "--help" ; then
    usage
    exit 0;
elif [ $# -gt 2 ]; then
    usage
    exit 1
elif [ $# != 1 ]; then
    old_prefix=@@CONTRIB_PREFIX@@
    new_prefix=`pwd`
else
    old_prefix=$1
    new_prefix=@@CONTRIB_PREFIX@@
    CHECK_PREFIX=1
fi

# process [dir] [filemask] [text_only|check]
process() {
    for file in `find $1 \( ! -name \`basename $1\` -o -type f \) -prune -type f -name "$2"`
    do
        if [ -n "$3" -a "$3" = "text_only" ]
        then
            file $file | sed "s/^.*: //" | grep -q 'text\|shell' || continue
        fi
        echo "Fixing up $file"
        if [ -n "$3" -a "$3" = "check" -a ! -z "$CHECK_PREFIX" ]
        then
            # Ensure the file we're checking contains a prefix
            if grep -q '^prefix=' $file; then
                # Skip .pc files with a relocatable prefix
                if grep -q '${pcfiledir}' $file; then
                    continue
                fi
                # And if it does, ensure it's correctly pointing to the configured one
                if ! grep -q $old_prefix $file; then
                    echo "Can't find the old_prefix ($old_prefix) in file $file:"
                    cat $file
                    exit 1;
                fi
            fi
        fi
        sed -i.orig -e "s,$old_prefix,$new_prefix,g" $file
        rm -f $file.orig
    done
}

process bin/ "*" text_only
process lib/ "*.la"
process lib/pkgconfig/ "*.pc" check
process share/meson/cross/ "*.ini"
