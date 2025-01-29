#
# Direct3d9 modules
#

if have_win_desktop
vlc_modules += {
    'name' : 'direct3d9_filters',
    'sources' : files(
        'd3d9_filters.c',
        'dxva2_deinterlace.c',
        'dxa9.c',
        'd3d9_device.c',
        ),
    'link_with' : [ d3d9_common_lib, chroma_copy_lib, deinterlacecommon_lib ],
    'dependencies' : [ cc.find_library('dxva2') ],
    'include_directories' : [vlc_include_dirs],
    'enabled' : get_option('directx').allowed()
}
endif
