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

usage="Usage: $0 <directory> <prefix> <new_prefix>"

LANG=C
export LANG

if test .$1 = .-h -o .$1 = .--help -o $# != 3; then
  echo $usage
  exit 1
fi

top_dir=`cd $1; pwd`
prefix=$2
new_prefix2=$3
new_prefix=/$new_prefix2

if test -z $prefix -o -z $new_prefix; then
  echo $usage
  exit 1
fi

cd $top_dir
pwd
files=`find . -type f`
for file in $files; do
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
  elif test ".`file $file | grep \"text\|shell\"`" != "." -o ".`echo $file | grep pc$`" != "."; then
   echo "Fixing up shell/text/pc file "$file""
    cp $file $file.tmp
    sed -e "s,$prefix,$new_prefix,g" < $file > $file.tmp
    mv -f $file.tmp $file
  else
    echo "Not doing anything with $file"
  fi
done

files=`find . -name *.la`
for file in $files; do
   echo "Fixing up .la $file"
   cp $file $file.tmp
   sed -e "s,$prefix,$new_prefix,g" < $file > $file.tmp
   mv -f $file.tmp $file
done

