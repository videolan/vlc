#!/bin/bash

function plist_key()
{
    KEY="$1"
    VALUE="$2"
    echo '  <key>'"${KEY}"'</key>'
    echo '  <string>'"${VALUE}"'</string>'
}

function generate_info_plist()
{
    EXECUTABLE_NAME="$1"
    echo '<?xml version="1.0" encoding="UTF-8"?>'
    echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">'
    echo '<plist version="1.0">'
    echo '<dict>'
    plist_key CFBundleDevelopmentRegion "en"
    plist_key CFBundleExecutable "${EXECUTABLE_NAME}"
    plist_key CFBundleIdentifier "org.videolan.vlc.plugins.${EXECUTABLE_NAME}.framework"
    plist_key CFBundleInfoDictionaryVersion "6.0"
    plist_key CFBundlePackageType "FMWK"
    plist_key CFBundleSignature "???"
    echo '</dict>'
    echo '</plist>'
}

# Parse vlc_modules_list with format: plugin_name: relative/path/to/plugin.dylib
PLUGIN_ENTRIES=()
for arch in ${ARCHS}; do
  while IFS= read -r line; do
    [ -z "$line" ] && continue
    # Check if entry already exists (by plugin name)
    plugin="${line%%: *}"
    found=0
    for entry in "${PLUGIN_ENTRIES[@]}"; do
      if [ "${entry%%: *}" = "$plugin" ]; then
        found=1
        break
      fi
    done
    if [ $found -eq 0 ]; then
      PLUGIN_ENTRIES+=( "$line" )
    fi
  done < "${BUILT_PRODUCTS_DIR}/build-${PLATFORM_NAME}-${arch}/build/modules/vlc_modules_list"
done

for entry in "${PLUGIN_ENTRIES[@]}"; do
    plugin="${entry%%: *}"
    plugin_path="${entry#*: }"
    echo "Copying plugin $plugin for platform ${PLATFORM_NAME}"

    INPUT_FILES=()
    for arch in ${ARCHS}; do
        input_file="${BUILT_PRODUCTS_DIR}/build-${PLATFORM_NAME}-${arch}/build/${plugin_path}"
        if [ ! -f "${input_file}" ]; then
            continue
        fi
        echo " - Adding plugin file ${input_file} for lipo"
        INPUT_FILES+=( "$input_file" )
    done
    FRAMEWORK_DIR="${BUILT_PRODUCTS_DIR}/${FRAMEWORKS_FOLDER_PATH}/${plugin}.framework"
    FRAMEWORK_BIN_PATH="${FRAMEWORK_DIR}/${plugin}"

    for file in "${INPUT_FILES[@]}"; do
        if [ -f "${FRAMEWORK_BIN_PATH}" ] && [ "$file" -ot "${FRAMEWORK_BIN_PATH}" ]; then
            continue
        fi
        mkdir -p "${FRAMEWORK_DIR}"
        lipo -create \
            -output "${FRAMEWORK_BIN_PATH}" \
            "${INPUT_FILES[@]}"
        install_name_tool -change "@rpath/libvlccore.dylib" "@rpath/vlccore.framework/vlccore" \
            "${FRAMEWORK_BIN_PATH}"
        dsymutil -o "${FRAMEWORK_DIR}.dSYM" "${FRAMEWORK_BIN_PATH}"
        generate_info_plist "${plugin}" > "${FRAMEWORK_DIR}/Info.plist"
        codesign --force --sign "${EXPANDED_CODE_SIGN_IDENTITY}" "${FRAMEWORK_DIR}"
        break
    done
done
