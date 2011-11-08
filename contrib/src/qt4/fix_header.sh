#!/bin/sh

mv "$1" "$1.tmp"
sed 's,../../src,../src,' "$1.tmp" > "$1"
rm -f "$1.tmp"
