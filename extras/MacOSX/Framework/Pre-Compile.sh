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

if test "${ACTION}" = "build"; then    
    vlc_config="${VLC_SRC_DIR}/vlc-config"
    lib="lib"
    modules="modules"
    target="${TARGET_BUILD_DIR}/${CONTENTS_FOLDER_PATH}"
    target_lib="${target}/${lib}"            # Should we consider using a different well-known folder like shared resources?
    target_modules="${target}/${modules}"    # Should we consider using a different well-known folder like shared resources?
    target_share="${target}/${share}"    # Should we consider using a different well-known folder like shared resources?
    linked_libs=" "
    
    ##########################
    # @function install_library(src_lib, dest_dir)
    # @description Installs the specified library into the destination folder, automatically changes the references to dependencies
    # @param src_lib     source library to copy to the destination directory
    # @param dest_dir    destination directory where the src_lib should be copied to
    install_library() {    
        if [ ${3} = "library" ]; then
            install_name="@loader_path/lib"
        else
            install_name="@loader_path/modules"
        fi
        
        if [ "${4}" != "" ]; then
            lib_dest="${2}/${4}"
        else
            lib_dest="${2}/`basename ${1}`"
        fi
        
        if test -e ${1} && ((! test -e ${lib_dest}) || test ${1} -nt ${lib_dest} ); then
            mkdir -p ${2}
            
            # Lets copy the library from the source folder to our new destination folder
            cp ${1} ${lib_dest}

            # Update the dynamic library so it will know where to look for the other libraries
            echo "Installing ${3} `basename ${lib_dest}`"

            # Change the reference of libvlc.1 stored in the usr directory to libvlc.dylib in the framework's library directory
            install_name_tool -change /usr/local/lib/libvlc.1.dylib @loader_path/../lib/libvlc.dylib ${lib_dest}
            install_name_tool -change @executable_path/lib/vlc_libintl.dylib @loader_path/../lib/vlc_libintl.dylib ${lib_dest}
            install_name_tool -id "${install_name}/`basename ${lib_dest}`" ${lib_dest}

            # Iterate through each installed library and modify the references to other dynamic libraries to match the framework's library directory
            for linked_lib in `otool -L ${lib_dest}  | grep '(' | sed 's/\((.*)\)//'`; do
                ref_lib=`echo "${linked_lib}" | sed 's:executable_path/:loader_path/../:'`
                
                if test "${ref_lib}" != "${linked_lib}"; then
                    install_name_tool -change ${linked_lib} ${ref_lib} ${lib_dest}
                fi
                if test `echo "${ref_lib}" | grep "^@loader_path"`; then
                    linked_libs="${linked_libs} ${ref_lib}"
                fi;
            done
        fi
    }
    # @function install_library
    ##########################

    ##########################
    # Build the modules folder (Same as VLCKit.framework/modules in Makefile)
    echo "Building modules folder..."
    # Figure out what modules are available to install
    for module in `top_builddir="${VLC_BUILD_DIR}" ${vlc_config} --target plugin` ; do
        # Check to see that the reported module actually exists
        if test -n ${module}; then
            module_src="`dirname ${module}`/.libs/`basename ${module}`.dylib"
            install_library ${module_src} ${target_modules} "module"
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
    # Build the library folder (Same as VLCKit.framework/lib in Makefile)
    echo "Building library folder..."
    for linked_lib in ${linked_libs} ; do
        case "${linked_lib}" in
            @loader_path/../lib/*)
                ref_lib=`echo ${linked_lib} | sed 's:@loader_path/../lib/::'`
                if test -e ${VLC_BUILD_DIR}/extras/contrib/vlc-lib/${ref_lib}; then
                    src_lib=${VLC_BUILD_DIR}/extras/contrib/vlc-lib/${ref_lib}
                elif test -e ${VLC_BUILD_DIR}/src/.libs/${ref_lib}; then
                    src_lib=${VLC_BUILD_DIR}/src/.libs/${ref_lib}
                fi
                install_library ${src_lib} ${target_lib} "library"
                ;;
        esac
    done
    # Build the share folder
    ##########################
    # Build the library folder (Same as VLCKit.framework/lib in Makefile)
    echo "Building share folder..."
    pbxcp="/Developer/Library/PrivateFrameworks/DevToolsCore.framework/Resources/pbxcp -exclude .DS_Store -exclude CVS -exclude .svn -resolve-src-symlinks"
    $pbxcp ${VLC_BUILD_DIR}/share/luameta ${target_share}
    $pbxcp ${VLC_BUILD_DIR}/share/luaplaylist ${target_share}
fi