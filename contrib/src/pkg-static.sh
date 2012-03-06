#! /bin/sh
# Copyright (C) 2012 Rémi Denis-Courmont
# This file is distributed under the same license as the vlc package.

if test -z "$1" || test -n "$2"; then
	echo "Usage: $0 <file.pc>" >&2
	echo "Merges the pkg-config Libs.private stanza into Libs stanza." >&2
	exit 1
fi

exec <"$1" >"$1.tmp" || exit $?

PUBLIC=""
PRIVATE=""

while read LINE; do
	pub="${LINE#Libs:}"
	priv="${LINE#Libs.private:}"
	if test "$pub" != "$LINE"; then
		PUBLIC="$pub"
	elif test "$priv" != "$LINE"; then
		PRIVATE="$priv"
	else
		echo "$LINE"
	fi
done
echo "Libs: $PUBLIC $PRIVATE"

mv -f -- "$1.tmp" "$1"
