nvdec_dep = disabler()
if get_option('nvdec').allowed()
if cc.check_header('ffnvcodec/dynlink_loader.h')
    nvdec_dep = enabler()
elif cc.check_header('ffnvcodec/dynlink_loader.h', dependencies: contrib_dep )
    nvdec_dep = contrib_dep
endif
endif
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
