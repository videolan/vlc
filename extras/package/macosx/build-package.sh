#!/bin/sh
#
# build-package.sh
#
# Script that installs libvlc to VLC.app

# We are building VLC.app
#
if test "${ACTION}" = "release-makefile"; then
    echo "running build-package.sh in release-makefile mode"

    FULL_PRODUCT_NAME="${PRODUCT}"
    TARGET_BUILD_DIR="${build_dir}"
    CONTENTS_FOLDER_PATH="${FULL_PRODUCT_NAME}/Contents/MacOS"
    VLC_BUILD_DIR="${build_dir}"
    VLC_SRC_DIR="${src_dir}"
    ACTION="build"
    RELEASE_MAKEFILE="yes"
    use_archs="no"
    main_build_dir="${VLC_BUILD_DIR}"
else
    use_archs="yes"
    main_build_dir="${VLC_BUILD_DIR}/${ARCHS%% *}"
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
num_archs=$(echo `echo $ARCHS | wc -w`)

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

    if test ! -e ${src_lib}; then
        return
    fi

    if ( (test ! -e ${lib_dest}) || test ${src_lib} -nt ${lib_dest} ); then

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
            local shouldUpdate="no"

            # Determine what architectures are available in the destination image
            local fatdest_archs=""
            if [ -e ${fatdest} ]; then
                fatdest_archs=`lipo -info "${fatdest}" 2> /dev/null | sed -E -e 's/[[:space:]]+$//' -e 's/.+:[[:space:]]*//' -e 's/[^[:space:]]+/(&)/g'`

                # Check to see if the destination image needs to be reconstructed
                for arch in $ARCHS; do
                    # Only install if the new image is newer than the one we have installed or the required arch is missing.
                    if test $shouldUpdate = "no"  && (! [[ "$fatdest_archs" =~ \($arch\) ]] || test "$VLC_BUILD_DIR/$arch/$src_dir/$src" -nt "${fatdest}"); then
                        shouldUpdate="yes"
                    fi
                    fatdest_archs=${fatdest_archs//\($arch\)/}
                done

                # Reconstruct the destination image, if the update flag is set or if there are more archs in the desintation then we need
                fatdest_archs=${fatdest_archs// /}
            else
                # If the destination image does not exist, then we have to reconstruct it
                shouldUpdate="yes"
            fi

            # If we should update the destination image or if there were unexpected archs in the destination image, then reconstruct it
            if test "$shouldUpdate" = "yes" || test -n "${fatdest_archs}"; then
                # If the destination image exists, get rid of it so we can copy over the newly constructed image
                if test -e ${fatdest}; then
                    rm "$fatdest"
                fi

                if test "$num_archs" = "1"; then
                    echo "Copying $ARCHS $type $fatdest"
                    local arch_src="$VLC_BUILD_DIR/$ARCHS/$src_dir/$src"
                    vlc_install_object "$arch_src" "$dest_dir" "$type" "$5" ""
                else
                    # Create a temporary destination dir to store each ARCH object file
                    local tmp_dest_dir="$VLC_BUILD_DIR/tmp/$type"
                    rm -Rf "${tmp_dest_dir}/*"
                    mkdir -p "$tmp_dest_dir"

                    # Search for each ARCH object file used to construct a fat image
                    local objects=""
                    for arch in $ARCHS; do
                        local arch_src="$VLC_BUILD_DIR/$arch/$src_dir/$src"
                        vlc_install_object "$arch_src" "$tmp_dest_dir" "$type" "$5" "" ".$arch"
                        local dest="$tmp_dest_dir/$src.$arch"
                        if [ -e ${dest} ]; then
                            objects="${dest} $objects"
                        else
                            echo "Warning: building $arch_src without $arch"
                        fi
                    done;

                    echo "Creating fat $type $fatdest"
                    lipo $objects -output "$fatdest" -create
                fi
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
    vlc_install "bin/${prefix}" "vlc-osx" "${target}" "bin" "@loader_path/lib"
    mv "${target}/vlc-osx" "${target}/VLC"
    chmod +x ${target}/VLC
else
    vlc_install "bin/${prefix}" "vlc" "${target}/bin" "bin" "@loader_path/../lib"
fi

##########################
# Build the plugins folder
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
vlc_install "lib/${prefix}" "libvlc.*.dylib" "${target_lib}" "library"
vlc_install "src/${prefix}" "libvlccore.*.dylib" "${target_lib}" "library"

# copy symlinks
cp -RP "lib/${prefix}/libvlc.dylib" "${target_lib}"
cp -RP "src/${prefix}/libvlccore.dylib" "${target_lib}"

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
