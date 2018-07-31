#!/bin/sh
# create a patch file based on file extension
# $1 folder to run the find in
# $2 extension to use for the diff
find $1 -name "*$2" -exec bash -c 'filepath="$1" ; dir=$(dirname "$filepath") ; base=$(basename -s $2 "$filepath") ; diff -pur $filepath $dir/$base' -- {} $2 \;