#!/usr/bin/env bash

set -eu

readonly SCRIPT_DIR="$(cd "${BASH_SOURCE%/*}"; pwd)"
readonly BUILD_DIR="$(cd "$1"; pwd)"

APP="Payload/vlccore.app"
IPA="vlccore_unsigned.ipa"

# CONVERT_PLIST <input file> <output file>
# Convert a plist file into binary1 format in order to put it
# into an Apple bundle.
CONVERT_PLIST()
{
    INPUT=$1
    OUTPUT=$2
    if [ which plutil > /dev/null 2>&1 ]; then
        plutil -convert binary1 -o "$OUTPUT" -- "$INPUT"
    else
        plistutil -o "$OUTPUT" -i "$INPUT"
    fi
}

# Cleanup previous archive
rm -f "$IPA"
rm -rf "Payload/"
mkdir -p "$APP"

# Find install_name tool in order to set rpath on executable
INSTALL_NAME_TOOL=$(which install_name_tool || echo "")
if [ -z "$INSTALL_NAME_TOOL" ]; then
    echo "install_name_tool not found, aborting..."
    exit 1
fi

# VLC core test binary compiled for iOS
cp "${BUILD_DIR}/test/.libs/vlc-ios" "$APP/vlccore"
${INSTALL_NAME_TOOL} "$APP/vlccore" -add_rpath "@executable_path/Frameworks"

# Convert Info.plist from XML to binary
CONVERT_PLIST "${SCRIPT_DIR}/Info.plist" "Payload/vlccore.app/Info.plist"

# Set the bundle type
echo "APPL????" > "$APP/PkgInfo"

# Copy the dylib into the bundle, in Frameworks/. In order to ship on the
# AppStore, each dylib should be bundled into a framework inside the
# Frameworks/ directory, but since it is only designed for development usage
# we can just put them there without further processing.
mkdir -p "$APP/Frameworks"
cp "${BUILD_DIR}/lib/.libs/libvlc.dylib" "$APP/Frameworks"
cp "${BUILD_DIR}/src/.libs/libvlccore.dylib" "$APP/Frameworks"
find "${BUILD_DIR}/modules/.libs/" -name "*.dylib" -exec cp {} "$APP/Frameworks" \;

# Archive the bundle into a .ipa file.
zip -r "$IPA" Payload
