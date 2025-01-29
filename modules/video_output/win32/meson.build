#
# Win32 Video output modules
#

# Direct3D11 video output
d3d11_sources = files('direct3d11.cpp', 'd3d11_quad.cpp', 'd3d11_scaler.cpp',
                      'd3d11_shaders.cpp', 'd3d11_tonemap.cpp',
                      'd3d_shaders.c', 'd3d_dynamic_shader.c')
d3d11_link_with = [ d3d11_common_lib, chroma_copy_lib ]
d3d11_cargs = []
d3d11_cxxargs = [ libcom_cppflags ]
d3d11_deps = [ cc.find_library('dxgi'), cc.find_library('d3d11'), cc.find_library('d3dcompiler') ]
if not have_win_store
    d3d11_sources += files('d3d11_swapchain.cpp', 'dxgi_swapchain.cpp', 'common.c', 'events.c', 'sensors.cpp', 'win32touch.c')
    d3d11_cargs += '-DHAVE_WIN32_SENSORS'
    d3d11_deps += [
        cc.find_library('user32')
    ]
endif
if get_option('amf_scaler').allowed()
    if cc.check_header('AMF/core/PropertyStorage.h', args: [contrib_inc_args]) \
    and cc.check_header('AMF/components/HQScaler.h', args: [contrib_inc_args])
        cdata.set('HAVE_AMF_SCALER', 1)
        d3d11_sources += files('../../hw/amf/amf_helper.c')
        d3d11_cxxargs += contrib_inc_args
        d3d11_cargs += contrib_inc_args
    endif
endif

if get_option('directx').allowed()
    vlc_modules += {
        'name' : 'direct3d11',
        'sources' : d3d11_sources,
        'link_with' : d3d11_link_with,
        'c_args' : d3d11_cargs,
        'cpp_args' : d3d11_cxxargs,
        'dependencies' : d3d11_deps
    }
endif

if have_win_desktop
    if get_option('directx').allowed()
        # Direct3D9 video output
        vlc_modules += {
            'name' : 'direct3d9',
            'sources' : files('direct3d9.c', 'common.c', 'events.c', 'sensors.cpp', 'win32touch.c'),
            'link_with' : [ d3d9_common_lib, chroma_copy_lib ],
            'dependencies' : [
                cc.find_library('user32'),
                cc.find_library('dxva2'),
            ]
        }
    endif

    # GDI video output
    vlc_modules += {
        'name' : 'wingdi',
        'sources' : files('wingdi.c', 'common.c', 'events.c', 'win32touch.c'),
        'link_with' : [ chroma_copy_lib ],
        'dependencies' : [
            cc.find_library('user32')
        ]
    }

    opengl32_lib = cc.find_library('opengl32', required: false)
    if opengl32_lib.found() and not missing_win_glew
        # glwin32
        vlc_modules += {
            'name' : 'glwin32',
            'sources' : [
                files('glwin32.c', 'common.c', 'events.c', 'sensors.cpp', 'win32touch.c'),
                opengl_vout_commonsources
            ],
            'c_args' : [ contrib_inc_args, '-DHAVE_WIN32_SENSORS' ],
            'dependencies' : [ gl_common_dep, opengl32_lib,
                cc.find_library('user32')
            ]
        }

        # WGL
        vlc_modules += {
            'name' : 'wgl',
            'sources' : files('wgl.c'),
            'c_args' : [ contrib_inc_args ],
            'dependencies' : [ opengl32_lib ]
        }
    endif

    # Desktop inhibit
    vlc_modules += {
        'name' : 'winhibit',
        'sources' : files('inhibit.c')
    }

    # Window
    vlc_modules += {
        'name' : 'win32_window',
        'sources' : files('window.c')
    }

    # Windows drawable
    vlc_modules += {
        'name' : 'drawable',
        'sources' : files('drawable.c')
    }
endif
