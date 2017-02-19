#!/bin/sh
#
# build-package.sh
#
# Script that installs libvlc to VLC.app

# We are building VLC.app
#
echo "running build-package.sh in release-makefile mode"

FULL_PRODUCT_NAME="${PRODUCT}"
VLC_BUILD_DIR="${build_dir}"

target="${build_dir}/${FULL_PRODUCT_NAME}/Contents/MacOS"
target_bin="${target}/bin"
target_lib="${target}/lib"            # Should we consider using a different well-known folder like shared resources?
target_plugins="${target}/plugins"    # Should we consider using a different well-known folder like shared resources?
linked_libs=""
prefix=".libs"
suffix="dylib"

##########################
# @function vlc_install_object(src_lib, dest_dir, type, lib_install_prefix )
# @description Installs the specified library into the destination folder, automatically changes the references to dependencies
# @param src_lib     source library to copy to the destination directory
# @param dest_dir    destination directory where the src_lib should be copied to
vlc_install_object() {
    local src_lib=${1}
    local dest_dir=${2}
    local type=${3}
    local lib_install_prefix=${4}

    if [ $type = "library" ]; then
        local install_name="@loader_path/lib"
    elif [ $type = "module" ]; then
        local install_name="@loader_path/plugins"
    fi

    local lib_dest="$dest_dir/`basename $src_lib`"
    local lib_name=`basename $src_lib`

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

    vlc_install_object "$VLC_BUILD_DIR/$src_dir/$src" "$dest_dir" "$type" $5
}
# @function vlc_install
##########################

##########################
# Create a symbolic link in the root of the framework
mkdir -p ${target_lib}
mkdir -p ${target_plugins}
mkdir -p ${target_bin}

##########################
# Hack for VLC.app
vlc_install "bin/${prefix}" "vlc-osx" "${target}" "bin" "@loader_path/lib"
mv "${target}/vlc-osx" "${target}/VLC"
chmod +x ${target}/VLC

##########################
# Build the plugins folder
echo "Building plugins folder..."
# Figure out what plugins are available to install
for module in `find ${VLC_BUILD_DIR}/modules -path "*dylib.dSYM*" -prune -o -name "lib*_plugin.dylib" -print | sed -e s:${VLC_BUILD_DIR}/::` ; do
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
