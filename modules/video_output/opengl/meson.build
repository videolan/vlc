opengl_headers_c_args = []
if host_system == 'android'
    opengl_headers_c_args += ['-DUSE_OPENGL_ES2']
endif
if host_system == 'darwin'
    opengl_headers_c_args += ['-DGL_SILENCE_DEPRECATION']
endif

opengl32_lib = cc.find_library('opengl32', required: false)
if have_win_desktop and opengl32_lib.found() and not missing_win_glew
    opengl_headers_c_args += contrib_inc_args
endif

gl_common_dep = declare_dependency(
    sources: files(
        'filter.c',
        'filters.c',
        'gl_api.c',
        'gl_util.c',
        'importer.c',
        'interop.c',
        'sampler_loader.c',
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
    libvlc_opengles = static_library('vlc_opengles',
                                     dependencies: [gl_common_dep, m_lib],
                                     c_args: '-DUSE_OPENGL_ES2')
endif

# Unified dependency for modules that work with either OpenGL or OpenGL ES
# NOTE: This does not provide linkage, but only headers for access to the
#       common OpenGL/OpenGLES definitions for modules using the dynamic
#       loading of symbols from the OpenGL provider.
gl_vout_dep = disabler()
if opengl_dep.found() and not (have_ios or have_tvos or host_system == 'android')
    gl_vout_dep = declare_dependency(
        link_with: libvlc_opengl,
    )
elif opengles2_dep.found() or have_ios or have_tvos or host_system == 'android'
    gl_vout_dep = declare_dependency(
        link_with: libvlc_opengles,
        compile_args: ['-DUSE_OPENGL_ES2'],
    )
endif

# Plugins that adapt to either backend link and compile against
# gl_plugin_dep. They are registered depending on whether gl_plugin_dep
# is disabled or not.
# These are GL filters/interops that run inside the current context and
# call GL through the provider's function table, so they need only the
# common library, not the GL/GLES2 link libraries.
gl_plugin_dep = disabler()
if opengl_dep.found() and not (have_ios or have_tvos or host_system == 'android')
    gl_plugin_dep = declare_dependency(
        link_with: libvlc_opengl,
        compile_args: opengl_headers_c_args,
        dependencies: [opengl_dep.partial_dependency(compile_args: true, includes: true)],
    )
elif opengles2_dep.found() or have_ios or have_tvos or host_system == 'android'
    gl_plugin_dep = declare_dependency(
        link_with: libvlc_opengles,
        compile_args: opengl_headers_c_args + ['-DUSE_OPENGL_ES2'],
        dependencies: [opengles2_dep.partial_dependency(compile_args: true, includes: true)],
    )
endif

# gles2 vout display module
vlc_modules += {
    'name' : 'gles2',
    'sources' : [
            files('display.c'),
            opengl_vout_commonsources
        ],
    'link_with' : [libvlc_opengles],
    'dependencies' : [gl_common_dep, m_lib],
    'c_args' : ['-DUSE_OPENGL_ES2'],
    'enabled' : opengles2_dep.found(),
}

# glfilter_draw
vlc_modules += {
    'name' : 'glfilter_draw',
    'sources' : files('filter_draw.c'),
    'dependencies' : [gl_plugin_dep, m_lib],
    'enabled' : gl_vout_dep.found(),
}

# glfilter_mock (test module)
vlc_modules += {
    'name' : 'glfilter_mock',
    'sources' : files('filter_mock.c'),
    'dependencies' : [gl_plugin_dep, m_lib],
    'enabled' : gl_vout_dep.found(),
}

# glsampler_builtin (GLSL core shaders for texture sampling)
glsampler_builtin_cargs = []
glsampler_builtin_deps = [gl_plugin_dep, m_lib]
glsampler_builtin_link = []
if libplacebo_dep.found()
    glsampler_builtin_cargs += '-DHAVE_LIBPLACEBO_GL'
    glsampler_builtin_deps += libplacebo_dep
    glsampler_builtin_link += libplacebo_utils
endif
vlc_modules += {
    'name' : 'glsampler_builtin',
    'sources' : files('sampler.c'),
    'dependencies' : glsampler_builtin_deps,
    'link_with' : glsampler_builtin_link,
    'c_args' : glsampler_builtin_cargs,
    'enabled' : gl_vout_dep.found(),
}

# interop_sw
vlc_modules += {
    'name' : 'glinterop_sw',
    'sources' : files('interop_sw.c'),
    'dependencies' : [gl_common_dep, gl_vout_dep, m_lib],
    'enabled' : gl_vout_dep.found(),
}

# interop_dxva2
dxva2_dep = cc.find_library('dxva2', required: false)
vlc_modules += {
    'name' : 'glinterop_dxva2',
    'sources' : files('interop_dxva2.c'),
    'c_args' : [ contrib_inc_args ],
    'dependencies' : [ opengl32_lib, dxva2_dep ],
    'link_with' : [ d3d9_common_lib ],
    'enabled' : have_win_desktop and opengl32_lib.found() and dxva2_dep.found() and not missing_win_glew,
}

# glinterop_vaapi - VAAPI (E)GL interop
vlc_modules += {
    'name' : 'glinterop_vaapi',
    'sources' : files(
        'interop_vaapi.c',
        '../../hw/vaapi/vlc_vaapi.c',
    ),
    'dependencies' : [gl_common_dep, libva_dep, egl_dep],
    'enabled' : opengl_dep.found() and egl_dep.found() and libva_dep.found(),
}

# egl_display_generic
vlc_modules += {
    'name' : 'egl_display_generic',
    'sources' : files('egl_display_generic.c'),
    'dependencies' : [egl_dep],
    'enabled' : egl_dep.found(),
}

# egl_display_gbm
gbm_dep = dependency('gbm', required: false)
egl_display_gbm_cargs = []
egl_display_gbm_deps = [egl_dep, gbm_dep]
if drm_dep.found()
    egl_display_gbm_cargs += '-DHAVE_KMS=1'
    egl_display_gbm_deps += drm_dep
endif

vlc_modules += {
    'name' : 'egl_display_gbm',
    'sources' : files('egl_display_gbm.c'),
    'dependencies' : egl_display_gbm_deps,
    'c_args' : egl_display_gbm_cargs,
    'enabled' : egl_dep.found() and gbm_dep.found(),
}
