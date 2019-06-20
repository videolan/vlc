# Folder
vlc_modules += {
    'name' : 'folder',
    'sources' : files('folder.c')
}

# Taglib
taglib_dep = dependency('taglib', version: '>= 1.11', required: get_option('taglib'))
if taglib_dep.found()
    vlc_modules += {
        'name' : 'taglib',
        'sources' : files('taglib.cpp', '../demux/xiph_metadata.c'),
        'dependencies' : [taglib_dep, z_lib]
    }
endif
