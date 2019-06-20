# Graphical user interface (GUI) modules

# ncurses text-based interface
ncursesw_dep = dependency('ncursesw', required: get_option('ncurses'))
if ncursesw_dep.found()
    vlc_modules += {
        'name' : 'ncurses',
        'sources' : files('ncurses.c'),
        'dependencies' : [m_lib, threads_dep, ncursesw_dep]
    }
endif

# Minimal macOS interface
if get_option('minimal_macosx').require(have_osx).allowed()
    vlc_modules += {
        'name' : 'minimal_macosx',
        'sources' : files(
            'minimal_macosx/intf.m',
            'minimal_macosx/misc.m',
            'minimal_macosx/VLCMinimalVoutWindow.m',
            'minimal_macosx/macosx.c'
        ),
        'dependencies' : [cocoa_dep],
        'objc_args' : ['-fobjc-arc', '-fobjc-exceptions']
    }
endif

# Qt interface module
subdir('qt')
