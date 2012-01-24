#!/bin/sh
#
# Pre-Compile.sh
#
# Script that install libvlc and its modules inside VLCKit.
#
# This is for some creepy reasons also used by legacy VLC.app or
# the moz plugin.

#
# We are building VLC.app or the moz plugin
#
if test "${ACTION}" = "release-makefile"; then
    echo "running Pre-Compile.sh in release-makefile mode"

    FULL_PRODUCT_NAME="${PRODUCT}"
    if [ "$FULL_PRODUCT_NAME" = "VLC-Plugin.plugin" ] ; then
        TARGET_BUILD_DIR="${src_dir}"
    else
        TARGET_BUILD_DIR="${build_dir}"
    fi
    CONTENTS_FOLDER_PATH="${FULL_PRODUCT_NAME}/Contents/MacOS"
    VLC_BUILD_DIR="${build_dir}"
    VLC_SRC_DIR="${src_dir}"
    ACTION="build"
    RELEASE_MAKEFILE="yes"
    use_archs="no"
    main_build_dir="${VLC_BUILD_DIR}"
else
    use_archs="yes"
    main_build_dir="${VLC_BUILD_DIR}/x86_64"
    echo "Building for $ARCHS"
fi

if test "${ACTION}" = "clean"; then
    rm -Rf "${VLC_BUILD_DIR}/tmp"
    exit 0
fi

if test "${ACTION}" != "build"; then
    echo "This script is supposed to run from xcodebuild or Xcode"
    exit 1
fi

lib="lib"
plugins="plugins"
share="share"
include="include"
target="${TARGET_BUILD_DIR}/${CONTENTS_FOLDER_PATH}"
target_bin="${target}/bin"
target_lib="${target}/${lib}"            # Should we consider using a different well-known folder like shared resources?
target_plugins="${target}/${plugins}"    # Should we consider using a different well-known folder like shared resources?
target_share="${target}/${share}"        # Should we consider using a different well-known folder like shared resources?
linked_libs=""
prefix=".libs"
suffix="dylib"

##########################
# @function vlc_install_object(src_lib, dest_dir, type, lib_install_prefix, destination_name, suffix)
# @description Installs the specified library into the destination folder, automatically changes the references to dependencies
# @param src_lib     source library to copy to the destination directory
# @param dest_dir    destination directory where the src_lib should be copied to
vlc_install_object() {
    local src_lib=${1}
    local dest_dir=${2}
    local type=${3}
    local lib_install_prefix=${4}
    local destination_name=${5}
    local suffix=${6}

    if [ $type = "library" ]; then
        local install_name="@loader_path/lib"
    elif [ $type = "module" ]; then
        local install_name="@loader_path/plugins"
    fi
    if [ "$destination_name" != "" ]; then
        local lib_dest="$dest_dir/$destination_name$suffix"
        local lib_name=`basename $destination_name`
    else
        local lib_dest="$dest_dir/`basename $src_lib`$suffix"
        local lib_name=`basename $src_lib`
    fi

    if [ "x$lib_install_prefix" != "x" ]; then
        local lib_install_prefix="$lib_install_prefix"
    else
        local lib_install_prefix="@loader_path/../lib"
    fi

    if ! test -e ${src_lib}; then
        return
    fi

    if ( (! test -e ${lib_dest}) || test ${src_lib} -nt ${lib_dest} ); then

        mkdir -p ${dest_dir}

        # Lets copy the library from the source folder to our new destination folder
        if [ "${type}" = "bin" ]; then
            install -m 755 ${src_lib} ${lib_dest}
        else
            install -m 644 ${src_lib} ${lib_dest}
        fi

        # Update the dynamic library so it will know where to look for the other libraries
        echo "Installing ${type} `basename ${lib_dest}`"

        if [ "${type}" = "library" ]; then
            # Change the reference of libvlc.1 stored in the usr directory to libvlc.dylib in the framework's library directory
            install_name_tool -id "${install_name}/${lib_name}" ${lib_dest} > /dev/null
        fi

        if [ "${type}" != "data" ]; then
            # Iterate through each installed library and modify the references to other dynamic libraries to match the framework's library directory
            for linked_lib in `otool -L ${lib_dest}  | grep '(' | sed 's/\((.*)\)//'`; do
                local name=`basename ${linked_lib}`
                case "${linked_lib}" in
                    */vlc_build_dir/* | */vlc_install_dir/* | *vlc* | */extras/contrib/*)
                        if test -e ${linked_lib}; then
                            install_name_tool -change "$linked_lib" "${lib_install_prefix}/${name}" "${lib_dest}"
                            linked_libs="${linked_libs} ${ref_lib}"
                            vlc_install_object ${linked_lib} ${target_lib} "library"
                        fi
                        ;;
                esac
            done
        fi
     fi
}
# @function vlc_install_object
##########################

##########################
# @function vlc_install(src_lib_dir, src_lib_name, dest_dir, type, lib_install_prefix)
# @description Installs the specified library into the destination folder, automatically changes the references to dependencies
# @param src_lib     source library to copy to the destination directory
# @param dest_dir    destination directory where the src_lib should be copied to
vlc_install() {
    local src_dir=$1
    local src=$2
    local dest_dir=$3
    local type=$4

    if test "$use_archs" = "no"; then
        vlc_install_object "$VLC_BUILD_DIR/$src_dir/$src" "$dest_dir" "$type" $5
    else
        if test $type = "data"; then
            vlc_install_object "$main_build_dir/$src_dir/$src" "$dest_dir" "$type" $5
        else
            local fatdest="$dest_dir/$2"
            local shouldUpdateFat="no"

            local objects=""

            # Create a temporary destination dir to store each ARCH object file
            local tmp_dest_dir="$VLC_BUILD_DIR/tmp/$type"
            rm -Rf "${tmp_dest_dir}/*"
            mkdir -p "$tmp_dest_dir"

            for arch in $ARCHS; do
                local arch_src="$VLC_BUILD_DIR/$arch/$src_dir/$src"

                # Only install if the new image is newer than the one we have installed.
                if ( (! test -e ${fatdest}) || test ${arch_src} -nt ${fatdest} ); then
                    vlc_install_object "$arch_src" "$tmp_dest_dir" "$type" "$5" "" ".$arch"
                    local dest="$tmp_dest_dir/$src.$arch"
                    if test -e ${dest}; then
                        if (! test "$dest_dir/$arch_src" -nt "${dest}"); then
                            shouldUpdateFat="yes"
                        fi
                        objects="${dest} $objects"
                    else
                        echo "Warning: building $arch_src without $arch"
                    fi
                fi
            done;

            if test "$shouldUpdateFat" = "yes"; then
                echo "Creating fat $type $fatdest"
                lipo $objects -output "$fatdest" -create
            fi
        fi
    fi
}
# @function vlc_install
##########################

##########################
# Create a symbolic link in the root of the framework
mkdir -p ${target_lib}
mkdir -p ${target_plugins}
mkdir -p ${target_bin}

if [ "$RELEASE_MAKEFILE" != "yes" ] ; then
    pushd `pwd` > /dev/null
    cd ${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}

    ln -sf Versions/Current/${lib} .
    ln -sf Versions/Current/${plugins} .
    ln -sf Versions/Current/${include} .
    ln -sf Versions/Current/${share} .
    ln -sf Versions/Current/bin .
    ln -sf ../plugins Versions/Current/bin
    ln -sf ../share Versions/Current/bin

    popd > /dev/null
fi

##########################
# Hack for VLC.app
if [ "$FULL_PRODUCT_NAME" = "VLC.app" ] ; then
    vlc_install "bin/${prefix}" "vlc" "${target}" "bin" "@loader_path/lib"
    mv ${target}/vlc ${target}/VLC
    chmod +x ${target}/VLC
elif [ "$FULL_PRODUCT_NAME" = "VLC-Plugin.plugin" ] ; then
    # install Safari webplugin
    vlc_install "projects/mozilla/${prefix}" "npvlc.${suffix}" "${target}" "lib" "@loader_path/lib"
    mv ${target}/npvlc.${suffix} "${target}/VLC Plugin"
    chmod +x "${target}/VLC Plugin"
else
    vlc_install "bin/${prefix}" "vlc" "${target}/bin" "bin" "@loader_path/../lib"
fi

##########################
# Build the plugins folder (Same as VLCKit.framework/plugins in Makefile)
echo "Building plugins folder..."
# Figure out what plugins are available to install
for module in `find ${main_build_dir}/modules -path "*dylib.dSYM*" -prune -o -name "lib*_plugin.dylib" -print | sed -e s:${main_build_dir}/::` ; do
    # Check to see that the reported module actually exists
    if test -n ${module}; then
        vlc_install `dirname ${module}` `basename ${module}` ${target_plugins} "module"
    fi
done

##########################
# Build the lib folder
vlc_install "lib/${prefix}" "libvlc.5.dylib" "${target_lib}" "library"
vlc_install "src/${prefix}" "libvlccore.5.dylib" "${target_lib}" "library"
pushd `pwd` > /dev/null
cd ${target_lib}
ln -sf libvlc.5.dylib libvlc.dylib
ln -sf libvlccore.5.dylib libvlccore.dylib
popd > /dev/null

##########################
# Build the share folder
if [ $PRODUCT != "VLC.app" ]; then
    echo "Building share folder..."
    pbxcp="/Developer/Library/PrivateFrameworks/DevToolsCore.framework/Resources/pbxcp -exclude .DS_Store -resolve-src-symlinks -v -V"
    mkdir -p ${target_share}
    if test "$use_archs" = "no"; then
        $pbxcp ${VLC_BUILD_DIR}/share/lua ${target_share}
    else
        $pbxcp ${main_build_dir}/share/lua ${target_share}
    fi
fi
