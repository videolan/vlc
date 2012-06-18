#!/bin/sh
# Copyright @ 2012 Felix Paul Kühne <fkuehne at videolan dot org>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.

info()
{
    local green="\033[1;32m"
    local normal="\033[0m"
    echo "[${green}codesign${normal}] $1"
}

usage()
{
cat << EOF
usage: $0 [options]

Sign VLC.app in the current directory

OPTIONS:
   -h            Show this help
   -i            Identity to use
   -t            Entitlements file to use
EOF

}

while getopts "hi:t:" OPTION
do
     case $OPTION in
         h)
             usage
             exit 1
         ;;
         i)
             IDENTITY=$OPTARG
         ;;
         t)
             OPTIONS="--entitlements $OPTARG"
         ;;
     esac
done
shift $(($OPTIND - 1))

if [ "x$1" != "x" ]; then
    usage
    exit 1
fi

info "Signing the executable"

codesign -s "$IDENTITY" $OPTIONS VLC.app/Contents/MacOS/VLC

info "Signing the modules"
find VLC.app/Contents/MacOS/plugins/* -type f -exec codesign -s "$IDENTITY" $OPTIONS '{}' \;

info "Signing the libraries"
find VLC.app/Contents/MacOS/lib/* -type f -exec codesign -s "$IDENTITY" $OPTIONS '{}' \;

info "Signing the lua stuff"
find VLC.app/Contents/MacOS/share/lua/* -type f -exec codesign -s "$IDENTITY" $OPTIONS '{}' \;

info "all items signed, validating..."

info "Validating binary"
codesign --verify VLC.app/Contents/MacOS/VLC

info "Validating modules"
find VLC.app/Contents/MacOS/plugins/* -type f -exec codesign --verify '{}' \;

info "Validating libraries"
find VLC.app/Contents/MacOS/lib/* -type f -exec codesign --verify '{}' \;

info "Validating lua stuff"
find VLC.app/Contents/MacOS/share/lua/* -type f -exec codesign --verify '{}' \;

info "Validation complete"
