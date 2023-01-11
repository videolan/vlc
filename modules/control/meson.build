# Dummy interface module
vlc_modules += {
    'name' : 'dummy',
    'sources' : files('dummy.c'),
}

# Gestures
vlc_modules += {
    'name' : 'gestures',
    'sources' : files('gestures.c')
}

# Hotkeys
vlc_modules += {
    'name' : 'hotkeys',
    'sources' : files('hotkeys.c'),
    'dependencies' : [m_lib]
}

# XXX: netsync disabled, move current code to new playlist/player and add a
#      way to control the output clock from the player
# vlc_modules += {
#     'name' : 'netsync',
#     'sources' : files('netsync.c'),
#     'dependencies' : [socket_libs]
# }

# Remote control
vlc_modules += {
    'name' : 'rc',
    'sources' : files(
        'cli/player.c',
        'cli/playlist.c',
        'cli/cli.c',
        'cli/cli.h'
    ),
    'dependencies' : [socket_libs, m_lib]
}

# XCB hotkeys
if xcb_dep.found() and xcb_keysyms_dep.found()
    vlc_modules += {
        'name' : 'xcb_hotkeys',
        'sources' : files('globalhotkeys/xcb.c'),
        'dependencies' : [xcb_dep, xcb_keysyms_dep]
    }
endif

# libvlc_motion
vlc_motion_deps = []
vlc_motion_cflags = []
vlc_motion_sources = [
    'motionlib.c',
]

if host_system == 'darwin'
  vlc_motion_cflags += ['-fconstant-cfstrings']
  vlc_motion_sources += ['unimotion.c']
  vlc_motion_deps += [corefoundation_dep, iokit_dep]
endif

vlc_motion_lib = static_library('vlc_motion',
    vlc_motion_sources,
    c_args: vlc_motion_cflags,
    include_directories: vlc_include_dirs,
    dependencies: vlc_motion_deps
)

# DBUS
dbus_dep = dependency('dbus-1', version: '>= 1.6.0', required: get_option('dbus'))
if dbus_dep.found()
    vlc_modules += {
        'name' : 'dbus',
        'sources' : files(
            'dbus/dbus_root.c',
            'dbus/dbus_player.c',
            'dbus/dbus_tracklist.c',
            'dbus/dbus.c',
        ),
        'dependencies' : [dbus_dep, m_lib]
    }
endif

if host_system == 'windows'

    # NT service control
    vlc_modules += {
        'name' : 'ntservice',
        'sources' : files('ntservice.c')
    }

    # Windows hotkeys control
    vlc_modules += {
        'name' : 'win_hotkeys',
        'sources' : files('globalhotkeys/win32.c')
    }

    # Windows message control
    vlc_modules += {
        'name' : 'win_msg',
        'sources' : files('win_msg.c')
    }

endif
