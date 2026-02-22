#!/bin/sh
shopt -s nullglob

export PATH=/app/jre/bin:$PATH
export JAVA_HOME=/app/jre
export LIBBLURAY_CP=/app/share/java/

for f in /app/share/vlc/extra/*/*.sh; do
  source $f
done

exec /app/bin/vlc.bin $VLC_ARGS "$@"

