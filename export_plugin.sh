#!/usr/bin/env bash

INPUT_PATH="$1"
OUTPUT_PATH="$2"
BASE_IDENTIFIER="FOOBAR.com.audiblegenius"

if [ -z "$INPUT_PATH" -o -z "${OUTPUT_PATH}" ]; then
    echo "Usage: ./export_plugin.sh [input folder for plugins] [output folder for frameworks]"
    exit 1
fi

mkdir -p "$OUTPUT_PATH"

PLUGIN_LIST=(
    filesystem
    vmem

# Audio plugins
    soxr
    scaletempo
    equalizer
    gain

# Audio mixer plugins
    float_mixer
    integer_mixer

# Audio output plugins
    audiounit_ios

# Codec plugins
    avcodec
    audiotoolboxmidi
    videotoolbox

# Demux
    avformat
    mp4

# Logger
    console_logger
    file_logger

# Packetizer
    packetizer_a52
    packetizer_avparser
    packetizer_dts
    packetizer_flac
    packetizer_h264
    packetizer_hevc
    packetizer_mpeg4audio
    packetizer_mpeg4video

# Service discovery
    bonjour

# Stream filters
    cache_block
    cache_read
    prefetch
    skiptags

# Stream output (necessary?)
    #transcode
    #gather
    #duplicate

# Video chroma
    chain
    cvpx
    i420_nv12
    i420_rgb
    rv32
    swscale
    yuvp

# Video filter
    blend
    adjust
    rotate
    scale
    transform

# Video output plugins
    glconv_cvpx
    vout_ios
)

function should_copy_plugin()
{
    name="$1"
    for plugin in "${PLUGIN_LIST[@]}"; do
        #echo "Comparing ${name} and ${plugin}"
        if [ "${plugin}" = "${name}" ]; then
            echo 1
            return
        fi
    done
}

for plugin in $(find "${INPUT_PATH}" -name  '*plugin.dylib'); do
    name="$(basename "${plugin}")"
    name="${name#lib}"
    name="${name%_plugin.dylib}"

    if [ "$(should_copy_plugin "${name}")" = 1 ]; then
        echo " - Copying ${name}"
    else
        continue
    fi

    out_path="${OUTPUT_PATH}/${name}_plugin.framework/${name}_plugin"
    mkdir -p ${OUTPUT_PATH}/${name}_plugin.framework
    cp "${plugin}" "${out_path}"
    install_name_tool \
        "${out_path}" \
        -id "@rpath/${name}_plugin.framework/${name}_plugin" \
        -change "@rpath/libvlccore.dylib" "@rpath/vlccore.framework/vlccore"

    EXECUTABLE_NAME=${name}_plugin \
    BUNDLE_IDENTIFIER=${BASE_IDENTIFIER}.${name}_plugin \
    bash template.info.plist.sh > "${OUTPUT_PATH}/${name}_plugin.framework/Info.plist"
    plutil -convert binary1 "${OUTPUT_PATH}/${name}_plugin.framework/Info.plist"

    codesign -f -v -s "${CODESIGN_IDENTITY}"\
        -i ${BASE_IDENTIFIER}.${name}_plugin \
        "${OUTPUT_PATH}/${name}_plugin.framework"
done

mkdir -p "${OUTPUT_PATH}/vlccore.framework"
cp "${INPUT_PATH}/lib/libvlccore.dylib" "${OUTPUT_PATH}/vlccore.framework/vlccore"
install_name_tool \
    "${OUTPUT_PATH}/vlccore.framework/vlccore" \
    -id "@rpath/vlccore.framework/vlccore"
codesign -f -v -s "${CODESIGN_IDENTITY}" \
    -i ${BASE_IDENTIFIER}.vlccore \
    "${OUTPUT_PATH}/vlccore.framework"

EXECUTABLE_NAME=vlccore \
BUNDLE_IDENTIFIER=${BASE_IDENTIFIER}.vlccore \
bash template.info.plist.sh > "${OUTPUT_PATH}/vlccore.framework/Info.plist"
plutil -convert binary1 "${OUTPUT_PATH}/vlccore.framework/Info.plist"

mkdir -p "${OUTPUT_PATH}/vlc.framework"
cp "${INPUT_PATH}/lib/libvlc.dylib" "${OUTPUT_PATH}/vlc.framework/vlc"
install_name_tool \
    "${OUTPUT_PATH}/vlc.framework/vlc" \
    -id "@rpath/vlc.framework/vlc" \
    -change "@rpath/libvlccore.dylib" "@rpath/vlccore.framework/vlccore"

EXECUTABLE_NAME=vlc \
BUNDLE_IDENTIFIER=${BASE_IDENTIFIER}.vlc \
bash template.info.plist.sh > "${OUTPUT_PATH}/vlc.framework/Info.plist"
plutil -convert binary1 "${OUTPUT_PATH}/vlc.framework/Info.plist"

codesign -f -v -s "${CODESIGN_IDENTITY}" \
    -i ${BASE_IDENTIFIER}.vlc \
    "${OUTPUT_PATH}/vlc.framework"
