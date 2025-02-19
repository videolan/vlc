vlc_libcompat = []

if libcompat_sources.length() > 0
    vlc_libcompat = static_library(
        'compat',
        libcompat_sources,
        include_directories: [vlc_include_dirs],
        dependencies: [m_lib],
        pic: true,
        install: true,
        install_dir: get_option('libdir') / 'vlc'
    )
endif

heap_alloc_env = environment({'ASAN_OPTIONS': 'allocator_may_return_null=1'})
vlc_tests += {
    'name': 'heap',
    'sources': files('test/heap.c'),
    'suite': ['compat'],
    'link_with': [vlc_libcompat],
    'env': heap_alloc_env,
}

vlc_tests += {
    'name': 'strnstr',
    'sources': files('test/strnstr.c'),
    'suite': ['compat'],
    'link_with': [vlc_libcompat],
}

vlc_tests += {
    'name': 'ckd_ckd',
    'sources': files('test/ckd.c'),
    'c_args': ['-DTEST_CKD'],
    'suite': ['compat'],
    'link_with': [vlc_libcompat],
}

ckd_builtin_cargs = ['-DTEST_BUILTIN']
if cdata.has('HAVE_STDCKDINT_H')
    ckd_builtin_cargs += '-DHAVE_CKD'
endif
vlc_tests += {
    'name': 'ckd_builtin',
    'sources': files('test/ckd.c'),
    'c_args': ckd_builtin_cargs,
    'suite': ['compat'],
    'link_with': [vlc_libcompat],
}

vlc_tests += {
    'name': 'ckd_compat',
    'sources': files('test/ckd.c'),
    'c_args': ['-DTEST_COMPAT'],
    'suite': ['compat'],
    'link_with': [vlc_libcompat],
}

#TODO: add clock_nanosleep test for Darwin
