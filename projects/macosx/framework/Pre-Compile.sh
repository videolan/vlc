#!/bin/sh
#
# Pre-Compile.sh
#
# Script that install libvlc and its modules inside VLCKit.
#
# This is for some creepy reasons also used by legacy VLC-release.app or
# the moz plugin.


#
# We are building VLC-release.app or the moz plugin
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
fi

if test "${ACTION}" != "build"; then
    echo "This script is supposed to run from xcodebuild or Xcode"
    exit 1
fi

lib="lib"
modules="modules"
share="share"
include="include"
target="${TARGET_BUILD_DIR}/${CONTENTS_FOLDER_PATH}"
target_lib="${target}/${lib}"            # Should we consider using a different well-known folder like shared resources?
target_modules="${target}/${modules}"    # Should we consider using a different well-known folder like shared resources?
target_share="${target}/${share}"    # Should we consider using a different well-known folder like shared resources?
target_include="${target}/${include}"    # Should we consider using a different well-known folder like shared resources?
linked_libs=""
prefix=".libs/"
suffix="dylib"


##########################
# @function vlc_install(src_lib, dest_dir, type, lib_install_prefix, destination_name)
# @description Installs the specified library into the destination folder, automatically changes the references to dependencies
# @param src_lib     source library to copy to the destination directory
# @param dest_dir    destination directory where the src_lib should be copied to
vlc_install() {

    local src_lib=${1}
    local dest_dir=${2}
    local type=${3}
    local lib_install_prefix=${4}
    local destination_name=${5}

    if [ $type = "library" ]; then
        local install_name="@loader_path/lib"
    elif [ $type = "module" ]; then
        local install_name="@loader_path/modules"
    fi
    if [ "$destination_name" != "" ]; then
        local lib_dest="$dest_dir/$destination_name"
    else
        local lib_dest="$dest_dir/`basename $src_lib`"
    fi

    if [ "$lib_install_prefix" != "" ]; then
        local lib_install_prefix="$lib_install_prefix"
    else
        local lib_install_prefix="@loader_path/../lib"
    fi

    if test -e ${src_lib} && ((! test -e ${lib_dest}) || test ${src_lib} -nt ${lib_dest} ); then

        mkdir -p ${dest_dir}

        # Lets copy the library from the source folder to our new destination folder
        if [ "${type}" = "bin" ]; then
            install -m 755 ${src_lib} ${lib_dest}
        else
            install -m 644 ${src_lib} ${lib_dest}
        fi

        # Update the dynamic library so it will know where to look for the other libraries
        echo "Installing ${type} `basename ${lib_dest}`"

        if [ "${type}" = "lib" ]; then
            # Change the reference of libvlc.1 stored in the usr directory to libvlc.dylib in the framework's library directory
            install_name_tool -id "${install_name}/`basename ${lib_dest}`" ${lib_dest} > /dev/null
        fi

        if [ "${type}" != "data" ]; then
            # Iterate through each installed library and modify the references to other dynamic libraries to match the framework's library directory
            for linked_lib in `otool -L ${lib_dest}  | grep '(' | sed 's/\((.*)\)//'`; do
                local name=`basename ${linked_lib}`
                case "${linked_lib}" in
                    */vlc_build_dir/* | */vlc_install_dir/* | *vlc* | */extras/contrib/lib/*)
                        if test -e ${linked_lib}; then
                            install_name_tool -change "$linked_lib" "${lib_install_prefix}/${name}" "${lib_dest}"
                            linked_libs="${linked_libs} ${ref_lib}"
                            vlc_install ${linked_lib} ${target_lib} "library"
                        fi
                        ;;
                esac
            done
        fi
     fi
}
# @function vlc_install
##########################

##########################
# Hack for VLC-release.app
if [ "$FULL_PRODUCT_NAME" = "VLC-release.app" ] ; then
    vlc_install "${main_build_dir}/bin/${prefix}vlc" "${target}" "bin" "@loader_path/lib"
    mv ${target}/vlc ${target}/VLC
    chmod +x ${target}/VLC
elif [ "$FULL_PRODUCT_NAME" = "VLC-Plugin.plugin" ] ; then
    # install Safari webplugin
    vlc_install "${main_build_dir}/projects/mozilla/${prefix}npvlc.${suffix}" "${target}" "library" "@loader_path/lib"
    mv ${target}/npvlc.${suffix} "${target}/VLC Plugin"
    chmod +x "${target}/VLC Plugin"
else
    vlc_install "${main_build_dir}/bin/${prefix}vlc" "${target}/bin" "bin" "@loader_path/../lib"
fi

##########################
# Build the modules folder (Same as VLCKit.framework/modules in Makefile)
echo "Building modules folder..."
# Figure out what modules are available to install
for module in `find ${main_build_dir}/modules -name *.${suffix}` ; do
    # Check to see that the reported module actually exists
    if test -n ${module}; then
        vlc_install ${module} ${target_modules} "module"
    fi
done

# Install the module cache
vlc_install `ls ${main_build_dir}/modules/plugins-*.dat` ${target_modules} "data"

# Build the modules folder
##########################

##########################
# Create a symbolic link in the root of the framework
mkdir -p ${target_lib}
mkdir -p ${target_modules}

if [ "$RELEASE_MAKEFILE" != "yes" ] ; then
    pushd `pwd` > /dev/null
    cd ${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}

    ln -sf Versions/Current/${lib} .
    ln -sf Versions/Current/${modules} .
    ln -sf Versions/Current/${include} .
    ln -sf Versions/Current/${share} .
    ln -sf Versions/Current/bin .
    ln -sf ../modules Versions/Current/bin
    ln -sf ../share Versions/Current/bin

    popd > /dev/null
fi

##########################
# Build the library folder
echo "Building library folder... ${linked_libs}"
for linked_lib in ${linked_libs} ; do
    case "${linked_lib}" in
        */extras/contrib/lib/*.dylib|*/vlc_install_dir/lib/*.dylib)
            if test -e ${linked_lib}; then
                vlc_install ${linked_lib} ${target_lib} "library"
            fi
            ;;
    esac
done

vlc_install "${main_build_dir}/src/${prefix}libvlc.5.dylib" "${target_lib}" "library"
vlc_install "${main_build_dir}/src/${prefix}libvlccore.4.dylib" "${target_lib}" "library"
pushd `pwd` > /dev/null
cd ${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}/lib
ln -sf libvlc.5.dylib libvlc.dylib
ln -sf libvlccore.4.dylib libvlccore.dylib
popd > /dev/null

##########################
# Build the share folder
echo "Building share folder..."
pbxcp="/Developer/Library/PrivateFrameworks/DevToolsCore.framework/Resources/pbxcp -exclude .DS_Store -resolve-src-symlinks"
mkdir -p ${target_share}
$pbxcp ${main_build_dir}/share/lua ${target_share}


##########################
# Exporting headers
if [ "$FULL_PRODUCT_NAME" = "VLC-release.app" ] ; then
    echo "Exporting headers..."
    mkdir -p ${target_include}/vlc
    $pbxcp ${main_build_dir}/include/vlc/*.h ${target_include}/vlc
else
    echo "Headers not needed for this product"
fi
