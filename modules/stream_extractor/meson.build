libarchive_dep = dependency('libarchive', version: '>= 3.1.0', required: get_option('archive'))
vlc_modules += {
    'name' : 'archive',
    'sources' : files('archive.c'),
    'dependencies' : [libarchive_dep],
    'enabled': libarchive_dep.found(),
}
