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

Sign Delete_Preferences.app in the current checkout

OPTIONS:
   -h            Show this help
   -i            Identity to use
   -g            Enable additional magic
EOF

}

while getopts "hi:g" OPTION
do
     case $OPTION in
         h)
             usage
             exit 1
         ;;
         i)
             IDENTITY=$OPTARG
         ;;
         g)
             GK="yes"
         ;;
     esac
done
shift $(($OPTIND - 1))

if [ "x$1" != "x" ]; then
    usage
    exit 1
fi

if test -z "$GK"
then
    info "Signing the executable"
    codesign --force --sign "$IDENTITY" $OPTIONS extras/package/macosx/Delete_Preferences.app/Contents/MacOS/applet

    info "Signing Resources"
    find extras/package/macosx/Delete_Preferences.app/Contents/Resources/* -type f -exec codesign --force -s "$IDENTITY" $OPTIONS '{}' \;
else
    REQUIREMENT="=designated => anchor apple generic  and identifier \"org.videolan.vlc\" and ((cert leaf[field.1.2.840.113635.100.6.1.9] exists) or ( certificate 1[field.1.2.840.113635.100.6.2.6] exists and certificate leaf[field.1.2.840.113635.100.6.1.13] exists  and certificate leaf[subject.OU] = \"75GAHG3SZQ\" ))"

    info "Signing the executable"
    codesign --force --sign "$IDENTITY" $OPTIONS --requirements "$REQUIREMENT" extras/package/macosx/Delete_Preferences.app/Contents/MacOS/applet

    info "Signing Resources"
    find extras/package/macosx/Delete_Preferences.app/Contents/Resources/* -type f -exec codesign --force -s "$IDENTITY" $OPTIONS --requirements "$REQUIREMENT" '{}' \;
fi

info "all items signed, validating..."

info "Validating binary"
codesign --verify extras/package/macosx/Delete_Preferences.app/Contents/MacOS/applet

info "Validating Resources"
find extras/package/macosx/Delete_Preferences.app/Contents/Resources/* -type f -exec codesign --verify '{}' \;

info "Validation complete"
