#
# Video output modules
#

# Declared here, as its used by both the
# libplacebo and opengl subfolder build file.
if have_win_store
    # even if it's there, don't use it
    missing_win_glew = true
elif host_system == 'windows'
    missing_win_glew = not cdata.has('HAVE_GL_WGLEW_H')
else
    missing_win_glew = false
endif

if missing_win_glew
    # Our OpenGL code requires glew.h to be present
    opengl_dep = disabler()
else
    opengl_dep = dependency('gl', required: false)
endif
opengles2_dep = dependency('glesv2', required: get_option('gles2'))

if host_system == 'darwin'
    subdir('apple')
endif

subdir('libplacebo')
subdir('opengl')

if host_system == 'windows'
    subdir('win32')
endif

# Dummy video output
vlc_modules += {
    'name' : 'vdummy',
    'sources' : files('vdummy.c')
}

# Dummy window provider
vlc_modules += {
    'name' : 'wdummy',
    'sources' : files('wdummy.c')
}

# Video splitter
vlc_modules += {
    'name' : 'video_splitter',
    'sources' : files('splitter.c')
}

# YUV video output
vlc_modules += {
    'name' : 'yuv',
    'sources' : files('yuv.c')
}

# Flaschen video output
vlc_modules += {
    'name' : 'flaschen',
    'sources' : files('flaschen.c'),
    'dependencies' : [socket_libs]
}

# vmem
vlc_modules += {
    'name' : 'vmem',
    'sources' : files('vmem.c')
}

# wextern
vlc_modules += {
    'name' : 'wextern',
    'sources' : files('wextern.c')
}

# vgl
vlc_modules += {
    'name' : 'vgl',
    'sources' : files('vgl.c')
}

vlc_modules += {
    'name' : 'decklinkoutput',
    'sources' : files('decklink.cpp',
                    '../stream_out/sdi/Ancillary.cpp',
                    '../stream_out/sdi/Ancillary.hpp',
                    '../stream_out/sdi/DBMHelper.cpp',
                    '../stream_out/sdi/DBMHelper.hpp',
                    '../stream_out/sdi/SDIGenerator.cpp',
                    '../stream_out/sdi/SDIGenerator.hpp',
                    '../stream_out/sdi/V210.cpp',
                    '../stream_out/sdi/V210.hpp'),
    'dependencies' : [dl_lib, decklink_dep],
    'cpp_args' : decklink_cpp_args,
    'enabled' : decklink_dep.found(),
}

# Kernel Mode Setting
drm_dep = dependency('libdrm', version: '>= 2.4.83', required: get_option('drm'))
if drm_dep.found()
    vlc_modules += {
        'name' : 'kms',
        'sources' : files('kms.c'),
        'dependencies' : [drm_dep]
    }
endif

# Coloured ASCII art (Caca)
caca_dep = dependency('caca', version: '>= 0.99.beta14', required: get_option('caca'))
caca_deps = [ caca_dep ]
if not (host_system in ['windows', 'darwin'])
    caca_deps += x11_dep
endif
if caca_dep.found()
    vlc_modules += {
        'name' : 'caca',
        'sources' : files('caca.c'),
        'dependencies' : [caca_deps],
    }
endif

#
# x11 video outputs
#

if xcb_dep.found()
    vlc_xcb_events_lib = library('vlc_xcb_events',
        files('xcb/events.c'),
        include_directories: [vlc_include_dirs],
        dependencies: [xcb_dep, libvlccore_dep])

    if xcb_randr_dep.found() and xproto_dep.found()
        vlc_modules += {
            'name' : 'xcb_window',
            'sources' : files('xcb/window.c'),
            'dependencies' : [xcb_dep, xcb_randr_dep, xproto_dep]
        }
    endif

    if xcb_render_dep.found() and xcb_shm_dep.found()
        vlc_modules += {
            'name' : 'xcb_render',
            'sources' : files(
              'xcb/pictures.c',
              'xcb/render.c'
            ),
            'dependencies' : [xcb_dep, xcb_render_dep, xcb_shm_dep, m_lib],
            'link_with' : [vlc_xcb_events_lib]
        }
    endif

    if xcb_shm_dep.found()
        vlc_modules += {
            'name' : 'xcb_x11',
            'sources' : files(
              'xcb/pictures.c',
              'xcb/x11.c'
            ),
            'dependencies' : [xcb_dep, xcb_shm_dep],
            'link_with' : [vlc_xcb_events_lib]
        }
    endif
endif

egl_dep = dependency('egl', required: false)

if x11_dep.found() and egl_dep.found()
    vlc_modules += {
        'name' : 'egl_x11',
        'sources' : files('opengl/egl.c'),
        'c_args' : ['-DUSE_PLATFORM_X11=1'],
        'dependencies' : [egl_dep, x11_dep]
    }
endif

if x11_dep.found() and opengl_dep.found()
    vlc_modules += {
        'name' : 'glx',
        'sources' : files('glx.c'),
        'dependencies' : [x11_dep, opengl_dep]
    }
endif
