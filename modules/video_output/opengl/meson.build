gl_common_dep = declare_dependency(
    link_with: [libplacebo_utils],
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
    include_directories: [vlc_include_dirs],
    dependencies: [contrib_dep],
)

opengl_vout_commonsources = files(
    'renderer.c',
    'sub_renderer.c',
    'vout_helper.c',
)

if opengl_dep.found() and not (have_ios or have_tvos)
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
        'link_with' : [libvlc_opengl],
        'dependencies' : [gl_common_dep, m_lib]
    }
endif

if opengles2_dep.found()
    libvlc_opengles = static_library('libvlc_opengles',
                                     dependencies: [gl_common_dep, m_lib],
                                     c_args: '-DUSE_OPENGL_ES2')
endif

# interop_sw
if host_system in ['darwin', 'android'] or opengl_dep.found() or opengles2_dep.found()
    interop_sw_deps = [gl_common_dep, m_lib]
    interop_sw_cargs = []

    if have_osx and opengl_dep.found()
        interop_sw_deps += libvlc_opengl
    elif host_system in ['darwin', 'android'] and opengles2_dep.found()
        interop_sw_deps += libvlc_opengles
    endif

    if opengles2_dep.found()
        interop_sw_cargs += '-DUSE_OPENGL_ES2'
    endif

    vlc_modules += {
        'name' : 'interop_sw',
        'sources' : files('interop_sw.c'),
        'dependencies' : interop_sw_deps,
        'c_args' : interop_sw_cargs,
    }
endif

# interop_dxva2
if have_win_desktop
    opengl32_lib = cc.find_library('opengl32', required: false)
    if opengl32_lib.found() and not missing_win_glew
        vlc_modules += {
            'name' : 'interop_dxva2',
            'sources' : files('interop_dxva2.c'),
            'c_args' : [ contrib_inc_args ],
            'dependencies' : [ opengl32_lib ],
            'link_with' : [ d3d9_common_lib ]
        }
    endif
endif
