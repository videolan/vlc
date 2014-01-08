#! /bin/sh

top_srcdir="${srcdir}/.."

set -xe

cd ${top_srcdir}

grep -v '^#' po/POTFILES.in | \
while read f
do
	test -n "$f" || continue
	if test ! -f "$f"
	then
		echo "$f: source file missing!" >&2
		exit 1
	fi
done
