#!/bin/bash
TC=/home/gabor/vlc-webos-build/sdk/arm-webos-linux-gnueabi_sdk-buildroot/bin
DEPLOY=/home/gabor/vlc-webos-deploy/media/developer/apps/usr/palm/applications/org.videolan.vlc

# Check Qt platform plugins (wayland is critical for TV)
for plugin in \
  "$DEPLOY/lib/vlc/plugins/gui/libqt_plugin.so" \
  "/home/gabor/qt6-webos/target/usr/plugins/platforms/libqwayland-generic.so" \
  "/home/gabor/qt6-webos/target/usr/plugins/platforms/libqeglfs.so"; do
  [ -f "$plugin" ] || continue
  echo "=== $(basename $plugin) ==="
  ${TC}/arm-webos-linux-gnueabi-readelf -d "$plugin" 2>/dev/null | grep NEEDED | sed "s/.*Shared library: \[//;s/\]//"
  echo ""
done
