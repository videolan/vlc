#
# Direct3d11 modules
#

if get_option('directx').allowed()
    vlc_modules += {
        'name' : 'direct3d11_filters',
        'sources' : files(
            'd3d11_filters.c',
            'd3d11_deinterlace.c',
            'd3d11_decoder.cpp',
            'd3d11_device.c',
            'd3d11_surface.c',
            'd3d11_processor.c',
            ),
        'link_with' : [ d3d11_common_lib, chroma_copy_lib, deinterlacecommon_lib ],
        'cpp_args' : libcom_cppflags,
        'include_directories' : [vlc_include_dirs],
    }
endif
