# nvdec
nvdec_dep = dependency('ffnvcodec', required: get_option('nvdec'))

vlc_modules += {
    'name' : 'nvdec',
    'sources' : files('nvdec.c', 'hw_pool.c'),
    'link_with' : [hxxxhelper_lib],
    'dependencies' : [nvdec_dep, dl_lib],
    'enabled' : nvdec_dep.found(),
}

vlc_modules += {
    'name' : 'nvdec_chroma',
    'sources' : files('chroma.c'),
    'dependencies' : [nvdec_dep],
    'enabled' : nvdec_dep.found(),
}

vlc_modules += {
    'name' : 'glinterop_nvdec',
    'sources' : files('nvdec_gl.c'),
    'dependencies' : [ nvdec_dep, gl_common_dep, dl_lib ],
    'enabled' : nvdec_dep.found() and gl_common_dep.found(),
}
