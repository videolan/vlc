#!/bin/sh

# Run this script from the contribs directory if you did not extract them
# to /usr/win32

grep '/usr/win32' -rl .|xargs sed -i -s "s#/usr/win32#$(pwd)#g"

