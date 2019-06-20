# subsdelay
vlc_modules += {
    'name' : 'subsdelay',
    'sources' : files('subsdelay.c')
}

# audiobargraph_v
vlc_modules += {
    'name' : 'audiobargraph_v',
    'sources' : files('audiobargraph_v.c'),
    'dependencies' : [m_lib]
}

# logo
vlc_modules += {
    'name' : 'logo',
    'sources' : files('logo.c')
}

# marq
vlc_modules += {
    'name' : 'marq',
    'sources' : files('marq.c')
}

# mosaic
vlc_modules += {
    'name' : 'mosaic',
    'sources' : files('mosaic.c'),
    'dependencies' : [m_lib]
}

# rss
vlc_modules += {
    'name' : 'rss',
    'sources' : files('rss.c')
}

if host_system != 'windows'
    # dynamicoverlay
    vlc_modules += {
        'name' : 'dynamicoverlay',
        'sources' : files(
            'dynamicoverlay/dynamicoverlay_buffer.c',
            'dynamicoverlay/dynamicoverlay_queue.c',
            'dynamicoverlay/dynamicoverlay_list.c',
            'dynamicoverlay/dynamicoverlay_commands.c',
            'dynamicoverlay/dynamicoverlay.c'
        )
    }
endif
