opengl_headers_c_args = []
if host_system == 'android'
    opengl_headers_c_args += ['-DUSE_OPENGL_ES2']
endif

gl_common_dep = declare_dependency(
    sources: files(
        'filter.c',
        'filters.c',
        'gl_api.c',
        'gl_util.c',
        'importer.c',
        'interop.c',
        'picture.c',
        'sampler.c',
        ),
    compile_args: opengl_headers_c_args,
    include_directories: [vlc_include_dirs],
    dependencies: [contrib_dep, opengl_dep],
)

opengl_vout_commonsources = files(
    'renderer.c',
    'sub_renderer.c',
    'vout_helper.c',
)

libvlc_opengl = []
if opengl_dep.found() and not (have_ios or have_tvos or host_system == 'android')
    libvlc_opengl = static_library('vlc_opengl',
                                   dependencies: [
                                    gl_common_dep,
                                    m_lib,
                                   ])
endif

vlc_modules += {
    'name' : 'gl',
    'sources' : [
            files('display.c'),
            opengl_vout_commonsources
        ],
    'link_with' : [libvlc_opengl],
    'dependencies' : [gl_common_dep, m_lib],
    'enabled' : opengl_dep.found() and not (have_ios or have_tvos or host_system == 'android')
}

libvlc_opengles = []
if opengles2_dep.found()
    libvlc_opengles = static_library('libvlc_opengles',
                                     dependencies: [gl_common_dep, m_lib],
                                     c_args: '-DUSE_OPENGL_ES2')
endif

# interop_sw
interop_sw_deps = [gl_common_dep, m_lib]
interop_sw_libs = []
interop_sw_cargs = []
if host_system in ['darwin', 'android'] or opengl_dep.found() or opengles2_dep.found()
    if have_osx and opengl_dep.found()
        interop_sw_libs += libvlc_opengl
    elif host_system in ['darwin', 'android'] and opengles2_dep.found()
        interop_sw_libs += libvlc_opengles
    endif

    if opengles2_dep.found() or host_system == 'android'
        interop_sw_cargs += '-DUSE_OPENGL_ES2'
    endif
endif

vlc_modules += {
    'name' : 'glinterop_sw',
    'sources' : files('interop_sw.c'),
    'dependencies' : interop_sw_deps,
    'c_args' : interop_sw_cargs,
    'link_with' : interop_sw_libs,
    'enabled' : host_system in ['darwin', 'android'] or opengl_dep.found() or opengles2_dep.found(),
}

# interop_dxva2
opengl32_lib = cc.find_library('opengl32', required: false)
dxva2_dep = cc.find_library('dxva2', required: false)
vlc_modules += {
    'name' : 'interop_dxva2',
    'sources' : files('interop_dxva2.c'),
    'c_args' : [ contrib_inc_args ],
    'dependencies' : [ opengl32_lib, dxva2_dep ],
    'link_with' : [ d3d9_common_lib ],
    'enabled' : have_win_desktop and opengl32_lib.found() and dxva2_dep.found() and not missing_win_glew,
}
