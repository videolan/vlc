# Clone video splitter
vlc_modules += {
    'name' : 'clone',
    'sources' : files('clone.c')
}

# Wall video splitter
vlc_modules += {
    'name' : 'wall',
    'sources' : files('wall.c')
}

if xcb_dep.found() and xcb_randr_dep.found()
    vlc_modules += {
        'name' : 'panoramix',
        'sources' : files('panoramix.c'),
        'dependencies' : [xcb_dep, xcb_randr_dep]
    }
elif host_system == 'windows'
    vlc_modules += {
        'name' : 'panoramix',
        'sources' : files('panoramix.c')
    }
endif
