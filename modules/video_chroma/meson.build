# chroma copy helper library
chroma_copy_lib_srcs = files('copy.c')
chroma_copy_lib = static_library(
    'chroma_copy',
    chroma_copy_lib_srcs,
    include_directories: [vlc_include_dirs],
    install: false,
    pic: true
)

vlc_modules += {
    'name' : 'chain',
    'sources' : files('chain.c')
}

swscale_dep = dependency('libswscale', version: '>= 0.5.0', required: get_option('swscale'))
vlc_modules += {
    'name' : 'swscale',
    'sources' : files(
      'swscale.c',
      '../codec/avcodec/chroma.c'
    ),
    'dependencies' : [swscale_dep, m_lib],
    'link_args' : symbolic_linkargs,
    'enabled' : swscale_dep.found(),
}

vlc_modules += {
    'name' : 'grey_yuv',
    'sources' : files('grey_yuv.c')
}

vlc_modules += {
    'name' : 'i420_rgb',
    'sources' : files(
        'i420_rgb.c',
        'i420_rgb8.c',
        'i420_rgb16.c',
    )
}

vlc_modules += {
    'name' : 'i420_yuy2',
    'sources' : files('i420_yuy2.c'),
}

vlc_modules += {
    'name' : 'i420_nv12',
    'sources' : files('i420_nv12.c'),
    'link_with' : [chroma_copy_lib],
}

vlc_modules += {
    'name' : 'i422_i420',
    'sources' : files('i422_i420.c')
}

vlc_modules += {
    'name' : 'i422_yuy2',
    'sources' : files('i422_yuy2.c'),
}

vlc_modules += {
    'name' : 'rv32',
    'sources' : files('rv32.c')
}

vlc_modules += {
    'name' : 'yuy2_i420',
    'sources' : files('yuy2_i420.c')
}

vlc_modules += {
    'name' : 'yuy2_i422',
    'sources' : files('yuy2_i422.c')
}

vlc_modules += {
    'name' : 'yuvp',
    'sources' : files('yuvp.c')
}

vlc_modules += {
    'name' : 'i420_rgb_sse2',
    'sources' : files(
        'i420_rgb.c',
        'i420_rgb16_x86.c'
    ),
    'c_args' : ['-DPLUGIN_SSE2'],
    'enabled' : have_sse2,
}

vlc_modules += {
    'name' : 'i420_yuy2_sse2',
    'sources' : files('i420_yuy2.c'),
    'c_args' : ['-DPLUGIN_SSE2'],
    'enabled' : have_sse2,
}

vlc_modules += {
    'name' : 'i422_yuy2_sse2',
    'sources' : files('i422_yuy2.c'),
    'c_args' : ['-DPLUGIN_SSE2'],
    'enabled' : have_sse2,
}

vlc_modules += {
    'name' : 'orient',
    'sources' : files('orient.c'),
}

d3d11_common_lib = []
if host_system == 'windows'
    d3d11_common_lib = static_library('d3d11_common',
        files(
            'd3d11_fmt.cpp',
            'dxgi_fmt.c',
            ),
        include_directories: [vlc_include_dirs],
        cpp_args : libcom_cppflags,
        pic: true,
        install: false,
        dependencies: [
            cc.find_library('dxguid'),
            cc.find_library('wbemuuid'),
            cc.find_library('uuid'),
            cc.find_library('d3d11'),
        ]
    )
    if have_win_desktop
    d3d9_common_lib = static_library('d3d9_common',
        files(
            'd3d9_fmt.c',
            'dxgi_fmt.c',
            ),
        include_directories: [vlc_include_dirs],
        dependencies: [ cc.find_library('d3d9') ],
        pic: true,
        install: false,
    )
    endif
endif

# CVPX chroma converter
# TODO: Set minimum versions for tvOS and iOS
vlc_modules += {
    'name' : 'cvpx',
    'sources' : files(
        '../codec/vt_utils.c',
        'cvpx.c',
    ),
    'link_with' : [chroma_copy_lib],
    'dependencies' : [
        frameworks['VideoToolbox'],
        frameworks['Foundation'],
        frameworks['CoreMedia'],
        frameworks['CoreVideo'],
    ],
    'enabled' : host_system == 'darwin',
}

## Tests

# Chroma copy SSE test
vlc_tests += {
    'name': 'chroma_copy_sse_test',
    'sources': chroma_copy_lib_srcs,
    'suite' : ['video_chroma'],
    'c_args': ['-DCOPY_TEST'],
    'link_with': [vlc_libcompat],
    'dependencies': [libvlccore_dep],
    'include_directories': [vlc_include_dirs]
}

# Chroma copy test
vlc_tests += {
    'name': 'chroma_copy_test',
    'sources': chroma_copy_lib_srcs,
    'suite' : ['video_chroma'],
    'c_args': ['-DCOPY_TEST', '-DCOPY_TEST_NOOPTIM'],
    'link_with': [vlc_libcompat],
    'dependencies': [libvlccore_dep],
    'include_directories': [vlc_include_dirs]
}
