gl_common_dep = declare_dependency(
    link_with: [libplacebo_utils],
    sources: files(
        'filter.c',
        'filters.c',
        'gl_api.c',
        'gl_util.c',
        'importer.c',
        'interop.c',
        'interop_sw.c',
        'picture.c',
        'sampler.c',
        ),
    include_directories: [vlc_include_dirs],
    dependencies: [contrib_dep],
)

opengl_vout_commonsources = files(
    'renderer.c',
    'sub_renderer.c',
    'vout_helper.c',
)

if opengl_dep.found()
    libvlc_opengl = static_library('vlc_opengl',
                                   dependencies: [
                                    gl_common_dep,
                                    m_lib,
                                   ])

    vlc_modules += {
        'name' : 'libgl',
        'sources' : [
                files('display.c'),
                opengl_vout_commonsources
            ],
        'link_with' : [libvlc_opengl]
    }
endif

if opengles2_dep.found()
    libvlc_opengles = static_library('libvlc_opengles',
                                     dependencies: [gl_common_dep, m_lib],
                                     c_args: '-DUSE_OPENGL_ES2')
endif

