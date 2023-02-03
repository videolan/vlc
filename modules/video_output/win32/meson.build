#
# Win32 Video output modules
#

# Direct3D11 video output
d3d11_sources = files('direct3d11.cpp', 'd3d11_quad.cpp', 'd3d11_shaders.cpp', 'd3d_shaders.c', 'd3d_dynamic_shader.c', 'd3d11_swapchain.cpp', 'dxgi_swapchain.cpp', 'common.c')
d3d11_link_with = [ d3d11_common_lib ]
if get_option('winstore_app')
    d3d11_link_with += [
        cc.find_library('d3d11'),
        cc.find_library('d3dcompiler_47')
    ]
else
    d3d11_sources += files('events.c', 'sensors.cpp', 'win32touch.c')
    d3d11_link_with += [ chroma_copy_lib ]
endif

vlc_modules += {
    'name' : 'direct3d11',
    'sources' : d3d11_sources,
    'link_with' : d3d11_link_with
}

if have_win_desktop
    # Direct3D9 video output
    vlc_modules += {
        'name' : 'direct3d9',
        'sources' : files('direct3d9.c', 'common.c', 'events.c', 'sensors.cpp', 'win32touch.c'),
        'link_with' : [ d3d9_common_lib, chroma_copy_lib ]
    }

    # GDI video output
    vlc_modules += {
        'name' : 'wingdi',
        'sources' : files('wingdi.c', 'common.c', 'events.c', 'win32touch.c'),
        'link_with' : [ chroma_copy_lib ]
    }

    # Desktop inhibit
    vlc_modules += {
        'name' : 'winhibit',
        'sources' : files('inhibit.c')
    }
endif
