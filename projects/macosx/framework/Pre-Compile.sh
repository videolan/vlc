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
fi

# Hack to use that script with the current VLC-release.app
if test "${ACTION}" = "VLC-release.app"; then
    TARGET_BUILD_DIR="${build_dir}"
    FULL_PRODUCT_NAME="VLC-release.app"
    CONTENTS_FOLDER_PATH="${FULL_PRODUCT_NAME}/Contents/MacOS"
    VLC_BUILD_DIR="${build_dir}"
    VLC_SRC_DIR="${src_dir}"
    ACTION="build"
fi

if test "${ACTION}" = "build"; then    
    lib="lib"
    modules="modules"
    share="share"
    target="${TARGET_BUILD_DIR}/${CONTENTS_FOLDER_PATH}"
    target_lib="${target}/${lib}"            # Should we consider using a different well-known folder like shared resources?
    target_modules="${target}/${modules}"    # Should we consider using a different well-known folder like shared resources?
    target_share="${target}/${share}"    # Should we consider using a different well-known folder like shared resources?
    linked_libs=" "
    
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
            install -m 644 ${1} ${lib_dest}

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
                    */vlc_build_dir/*  | *vlc* | */extras/contrib/lib/*)
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

    ##########################
    # Hack for VLC-release.app
    if [ "$FULL_PRODUCT_NAME" = "VLC-release.app" ] ; then
        prefix=".libs/"
        install_library "${VLC_BUILD_DIR}/src/${prefix}vlc" "${target}" "bin" "@loader_path/lib"
        install ${target}/vlc ${target}/VLC
        suffix="dylib"
    else
        prefix=""
        suffix="so"
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
    
    pushd `pwd` > /dev/null 
    cd ${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}
    
    ln -sf Versions/Current/${lib} .
    ln -sf Versions/Current/${modules} .
    
    popd > /dev/null 
    # Create a symbolic link in the root of the framework
    ##########################
    
    ##########################
    # Build the library folder
    echo "Building library folder... ${linked_libs}"
    for linked_lib in ${linked_libs} ; do
        case "${linked_lib}" in
            */extras/contrib/lib/*.dylib)
                if test -e ${linked_lib}; then
                    install_library ${linked_lib} ${target_lib} "library"
                fi
                ;;
            */vlc_install_dir/lib/*.dylib)
                if test -e ${linked_lib}; then
                    install_library ${linked_lib} ${target_lib} "library"
                fi
                ;;
        esac
    done


    install_library "${VLC_BUILD_DIR}/src/${prefix}libvlc-control.dylib" ${target_lib} "library"
    install_library "${VLC_BUILD_DIR}/src/${prefix}libvlc.dylib" ${target_lib} "library"

    ##########################
    # Build the share folder
    echo "Building share folder..."
    pbxcp="/Developer/Library/PrivateFrameworks/DevToolsCore.framework/Resources/pbxcp -exclude .DS_Store -resolve-src-symlinks"
    mkdir -p ${target_share}
    $pbxcp ${VLC_SRC_DIR}/share/lua ${target_share}
fi
