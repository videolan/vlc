#!/bin/sh
APP=/media/developer/apps/usr/palm/applications/org.videolan.vlc.webos
for lib in libxkbcommon.so.0 libGLESv2.so.2 libEGL.so.1 libatomic.so.1 libpcre2-16.so.0; do
  found=""
  for dir in ${APP}/vlc/lib /usr/lib /lib; do
    if [ -e "${dir}/${lib}" ]; then
      found="${dir}/${lib}"
      break
    fi
  done
  if [ -n "$found" ]; then
    echo "OK: $lib ($found)"
  else
    echo "MISSING: $lib"
  fi
done
