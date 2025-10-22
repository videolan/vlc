#! /bin/sh
# Copyright (C) 2012 Rémi Denis-Courmont
# This file is distributed under the same license as the vlc package.

if test -z "$1" || test -n "$2"; then
	echo "Usage: $0 <file.pc>" >&2
	echo "Merges the pkg-config {Requires/Libs/Cflags}.private stanza into {Requires/Libs/Cflags} stanzas." >&2
	exit 1
fi

exec <"$1" >"$1.tmp" || exit $?

LIBS_PUBLIC=""
LIBS_PRIVATE=""
REQUIRES_PUBLIC=""
REQUIRES_PRIVATE=""
CFLAGS_PUBLIC=""
CFLAGS_PRIVATE=""

while read LINE; do
	lpub="${LINE#Libs:}"
	lpriv="${LINE#Libs.private:}"
	rpub="${LINE#Requires:}"
	rpriv="${LINE#Requires.private:}"
	cpub="${LINE#Cflags:}"
	cpriv="${LINE#Cflags.private:}"
	if test "$lpub" != "$LINE"; then
		LIBS_PUBLIC="$lpub"
	elif test "$lpriv" != "$LINE"; then
		LIBS_PRIVATE="$lpriv"
	elif test "$rpub" != "$LINE"; then
		REQUIRES_PUBLIC="$rpub"
	elif test "$rpriv" != "$LINE"; then
		REQUIRES_PRIVATE="$rpriv"
	elif test "$cpub" != "$LINE"; then
		CFLAGS_PUBLIC="$cpub"
	elif test "$cpriv" != "$LINE"; then
		CFLAGS_PRIVATE="$cpriv"
	else
		echo "$LINE"
	fi
done
echo "Libs: $LIBS_PUBLIC $LIBS_PRIVATE"
echo "Cflags: $CFLAGS_PUBLIC $CFLAGS_PRIVATE"
echo "Requires: $REQUIRES_PUBLIC $REQUIRES_PRIVATE"

mv -f -- "$1.tmp" "$1"
