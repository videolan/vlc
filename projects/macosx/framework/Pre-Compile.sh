if test "${ACTION}" = ""; then
    # Debug --
    TARGET_BUILD_DIR="."
    FULL_PRODUCT_NAME="VLCKit.framework"
    CONTENTS_FOLDER_PATH="${FULL_PRODUCT_NAME}/Versions/A"
    VLC_BUILD_DIR="../../.."
    VLC_SRC_DIR="../../.."
    ACTION="build"
    rm -fr ${FULL_PRODUCT_NAME}
    # Debug --
# Hack to use that script with the current VLC-release.app
elif test "${ACTION}" = "release-makefile"; then
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
fi

if test "${ACTION}" = "build"; then    
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
    
    ##########################
    # @function install_library(src_lib, dest_dir, type, lib_install_prefix, destination_name)
    # @description Installs the specified library into the destination folder, automatically changes the references to dependencies
    # @param src_lib     source library to copy to the destination directory
    # @param dest_dir    destination directory where the src_lib should be copied to
    install_library() { 
   
        if [ ${3} = "library" ]; then
            local install_name="@loader_path/lib"
        elif [ ${3} = "module" ]; then
            local install_name="@loader_path/modules"
        fi
        if [ "${5}" != "" ]; then
            local lib_dest="${2}/${5}"
        else
            local lib_dest="${2}/`basename ${1}`"
        fi

        if [ "${4}" != "" ]; then
            local lib_install_prefix="${4}"
        else
            local lib_install_prefix="@loader_path/../lib"
        fi

        if test -e ${1} && ((! test -e ${lib_dest}) || test ${1} -nt ${lib_dest} ); then
            
            mkdir -p ${2}

            # Lets copy the library from the source folder to our new destination folder
            if [ "${3}" != "bin" ]; then
                install -m 644 ${1} ${lib_dest}
            else
                install -m 755 ${1} ${lib_dest}
            fi
            
            # Update the dynamic library so it will know where to look for the other libraries
            echo "Installing ${3} `basename ${lib_dest}`"

            if [ "${3}" != "bin" ]; then
                # Change the reference of libvlc.1 stored in the usr directory to libvlc.dylib in the framework's library directory
                install_name_tool -id "${install_name}/`basename ${lib_dest}`" ${lib_dest} > /dev/null
            fi
    
            # Iterate through each installed library and modify the references to other dynamic libraries to match the framework's library directory
            for linked_lib in `otool -L ${lib_dest}  | grep '(' | sed 's/\((.*)\)//'`; do
                local name=`basename ${linked_lib}`
                case "${linked_lib}" in
                    */vlc_build_dir/* | */vlc_install_dir/* | *vlc* | */extras/contrib/lib/*)
                        if test -e ${linked_lib}; then
                            install_name_tool -change "$linked_lib" "${lib_install_prefix}/${name}" "${lib_dest}"
                            linked_libs="${linked_libs} ${ref_lib}"
                            install_library ${linked_lib} ${target_lib} "library"
                        fi
                        ;;
                esac
            done
         fi
    }
    # @function install_library
    ##########################

    prefix=".libs/"
    suffix="dylib"

    ##########################
    # Hack for VLC-release.app
    if [ "$FULL_PRODUCT_NAME" = "VLC-release.app" ] ; then
        install_library "${VLC_BUILD_DIR}/bin/${prefix}vlc" "${target}" "bin" "@loader_path/lib"
        mv ${target}/vlc ${target}/VLC
        chmod +x ${target}/VLC
    elif [ "$FULL_PRODUCT_NAME" = "VLC-Plugin.plugin" ] ; then
        # install Safari webplugin
        install_library "${VLC_BUILD_DIR}/projects/mozilla/${prefix}npvlc.${suffix}" "${target}" "library" "@loader_path/lib"
        mv ${target}/npvlc.${suffix} "${target}/VLC Plugin"
        chmod +x "${target}/VLC Plugin"
    else
        install_library "${VLC_BUILD_DIR}/bin/${prefix}vlc" "${target}/bin" "bin" "@loader_path/../lib"
    fi

    ##########################
    # Build the modules folder (Same as VLCKit.framework/modules in Makefile)
    echo "Building modules folder..."
    # Figure out what modules are available to install
    for module in `find ${VLC_BUILD_DIR}/modules -name *.${suffix}` ; do
        # Check to see that the reported module actually exists
        if test -n ${module}; then
            install_library ${module} ${target_modules} "module"
        fi
    done
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

        popd > /dev/null
    fi

    # remove stuff we don't need
    if [ "$FULL_PRODUCT_NAME" = "VLCKit.framework" ] ; then
        echo "Removing module libmacosx_plugin.dylib"
        rm ${target_modules}/libmacosx_plugin.dylib
    fi

    ##########################
    # Build the library folder
    echo "Building library folder... ${linked_libs}"
    for linked_lib in ${linked_libs} ; do
        case "${linked_lib}" in
            */extras/contrib/lib/*.dylib|*/vlc_install_dir/lib/*.dylib)
                if test -e ${linked_lib}; then
                    install_library ${linked_lib} ${target_lib} "library"
                fi
                ;;
        esac
    done

    install_library "${VLC_BUILD_DIR}/src/${prefix}libvlc.dylib" "${target_lib}" "library"

    ##########################
    # Build the share folder
    echo "Building share folder..."
    pbxcp="/Developer/Library/PrivateFrameworks/DevToolsCore.framework/Resources/pbxcp -exclude .DS_Store -resolve-src-symlinks"
    mkdir -p ${target_share}
    $pbxcp ${VLC_SRC_DIR}/share/lua ${target_share}
    

    ##########################
    # Exporting headers
    if [ "$FULL_PRODUCT_NAME" = "VLC-release.app" ] ; then
        echo "Exporting headers..."
        mkdir -p ${target_include}/vlc
        $pbxcp ${VLC_SRC_DIR}/include/vlc/*.h ${target_include}/vlc
    else
        echo "Headers not needed for this product"
    fi

fi
