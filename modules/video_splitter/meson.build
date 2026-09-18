# Clone video splitter
vlc_modules += {
    'name' : 'clone',
    'sources' : files('clone.c'),
    'plugin_dir' : 'video_splitter',
}

# Wall video splitter
vlc_modules += {
    'name' : 'wall',
    'sources' : files('wall.c'),
    'plugin_dir' : 'video_splitter',
}

if xcb_dep.found() and xcb_randr_dep.found()
    vlc_modules += {
        'name' : 'panoramix',
        'sources' : files('panoramix.c'),
        'dependencies' : [xcb_dep, xcb_randr_dep],
        'plugin_dir' : 'video_splitter',
    }
elif host_system == 'windows' and have_win_desktop
    vlc_modules += {
        'name' : 'panoramix',
        'sources' : files('panoramix.c'),
        'plugin_dir' : 'video_splitter',
    }
endif
