#! /bin/sh
set -xe

SCRIPT_PATH="$( cd "$(dirname "$0")" ; pwd -P )"
cd "${SCRIPT_PATH}/.."

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
