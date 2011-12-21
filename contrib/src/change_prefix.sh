#!/bin/sh
# ***************************************************************************
# change_prefix.sh : allow to transfer a contrib dir
# ***************************************************************************
# Copyright (C) 2003 the VideoLAN team
# $Id$
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
# ***************************************************************************

LANG=C
export LANG

if test "$1" = "-h" -o "$1" = "--help" -o $# -gt 2; then
  echo "Usage: $0 [new_prefix] [old prefix]"
  exit 1
fi

if test -z "$1" -a -z "$2"
then
    new_prefix=`pwd`
    prefix=@@CONTRIB_PREFIX@@
else
    prefix=$1
    new_prefix=$2
fi

process() {
    grep -q "$prefix" "$1" || return
    echo "Fixing up $file"
    cp $file $file.tmp
    sed -e "s,$prefix,$new_prefix,g" < $file > $file.tmp
    mv -f $file.tmp $file
}

for file in `find . -type f`; do
 if ! test -e $file; then
   echo "$file doesn't exist"
   continue
 fi
 if test ".`file $file | grep Mach-O`" != "." ; then
    echo "Changing prefixes of '$file'"
    islib=n
    if test ".`file $file | grep 'dynamically linked shared library'`" != "." ; then
      islib=y
    fi
    libs=`otool -L $file 2>/dev/null | grep $prefix | cut -d\  -f 1`
    first=y
    for i in "" $libs; do
      if ! test -z $i; then
        if test $islib = y -a $first = y; then
            install_name_tool -id `echo $i | sed -e "s,$prefix,$new_prefix,"` $file
            first=n
        else
            install_name_tool -change $i `echo $i | sed -e "s,$prefix,$new_prefix,"` $file
        fi
      fi
    done
  elif test -n "`file $file | grep \"text\|shell\"`" -o -n "`echo $file | grep -E \"(pc|la)\"$`"; then
    process "$file"
  fi
done

exit 0
