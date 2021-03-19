#!/usr/bin/env bash

set -eu

readonly SCRIPT_DIR="$(cd "${BASH_SOURCE%/*}"; pwd)"
readonly BUILD_DIR="$(cd "$1"; pwd)"
readonly APP_NAME="$2"
readonly APP_EXECUTABLE="${3:-${APP_NAME}}"
readonly APP="Payload/${APP_NAME}.app"
readonly IPA="${APP_NAME}_unsigned.ipa"

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

# VLC core test binary compiled for iOS
cp "${BUILD_DIR}/test/${APP_EXECUTABLE}" "${APP}/${APP_NAME}"

# Convert Info.plist from XML to binary
CONVERT_PLIST "${SCRIPT_DIR}/Info.plist" "Payload/vlccore.app/Info.plist"

# Set the bundle type
echo "APPL????" > "$APP/PkgInfo"

# Copy the dylib into the bundle, in Frameworks/. In order to ship on the
# AppStore, each dylib should be bundled into a framework inside the
# Frameworks/ directory, but since it is only designed for development usage
# we can just put them there without further processing.
mkdir -p "$APP/Frameworks"
if [ -f "${BUILD_DIR}/lib/.libs/libvlc.dylib" ]; then
cp "${BUILD_DIR}/lib/.libs/libvlc.dylib" "$APP/Frameworks"
cp "${BUILD_DIR}/src/.libs/libvlccore.dylib" "$APP/Frameworks"
find "${BUILD_DIR}/modules/.libs/" -name "*.dylib" -exec cp {} "$APP/Frameworks" \;
fi
if [ -n "$EXTRA_FILE" ]; then
    cp "${EXTRA_FILE}" ${APP}/
fi

# Archive the bundle into a .ipa file.
zip -r "$IPA" Payload
