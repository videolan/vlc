# stats module
vlc_modules += {
    'name' : 'stats',
    'sources' : files('stats.c')
}

# audioscrobbler module
vlc_modules += {
    'name' : 'audioscrobbler',
    'sources' : files('audioscrobbler.c'),
    'dependencies' : [threads_dep, socket_libs]
}

# export module
vlc_modules += {
    'name' : 'export',
    'sources' : files(
        'playlist/export.c',
        'playlist/html.c',
        'playlist/m3u.c',
        'playlist/xspf.c',
    )
}

# audio fingerprinter module
vlc_modules += {
    'name' : 'fingerprinter',
    'sources' : files(
        'fingerprinter.c',
        'webservices/json.c',
        'webservices/acoustid.c',
    ),
    'dependencies' : [m_lib]
}

# libxml2 module
libxml2_dep = dependency('libxml-2.0', version: '>= 2.5', required: get_option('libxml2'))
if libxml2_dep.found()
    vlc_modules += {
        'name' : 'xml',
        'sources' : files('xml/libxml.c'),
        'dependencies' : [libxml2_dep]
    }
endif

# medialibrary module
medialibrary_dep = dependency('medialibrary', required: get_option('medialibrary'), method: 'pkg-config')
if medialibrary_dep.found()
    vlc_modules += {
        'name' : 'medialibrary',
        'sources' : files(
            'medialibrary/medialibrary.cpp',
            'medialibrary/MetadataExtractor.cpp',
            'medialibrary/entities.cpp',
            'medialibrary/Thumbnailer.cpp',
            'medialibrary/medialibrary.h',
            'medialibrary/fs/device.h',
            'medialibrary/fs/device.cpp',
            'medialibrary/fs/directory.h',
            'medialibrary/fs/directory.cpp',
            'medialibrary/fs/file.h',
            'medialibrary/fs/file.cpp',
            'medialibrary/fs/fs.h',
            'medialibrary/fs/fs.cpp',
            'medialibrary/fs/devicelister.cpp',
            'medialibrary/fs/devicelister.h',
            'medialibrary/fs/util.h',
            'medialibrary/fs/util.cpp',
        ),
        'dependencies' : [medialibrary_dep]
    }
endif

# Securetransport
if host_system == 'darwin'
    vlc_modules += {
        'name' : 'securetransport',
        'sources' : files('securetransport.c'),
        'dependencies' : [security_dep, corefoundation_dep]
    }
endif

# GnuTLS module
gnutls_dep = dependency('gnutls', version: '>= 3.5.0', required: get_option('gnutls'))
if gnutls_dep.found()

    gnutls_darwin_deps = []
    if host_system == 'darwin'
        # In theory these deps should be in the gnutls .pc file,
        # apparently they are missing there.
        gnutls_darwin_deps = [corefoundation_dep, security_dep]
    endif

    vlc_modules += {
        'name' : 'gnutls',
        'sources' : files('gnutls.c'),
        'dependencies' : [gnutls_dep, gnutls_darwin_deps]
    }
endif

# IOKit inhibit module (macOS only)
if have_osx
    vlc_modules += {
        'name' : 'iokit_inhibit',
        'sources' : files('inhibit/iokit-inhibit.c'),
        'dependencies' : [corefoundation_dep, iokit_dep]
    }
endif

if have_ios or have_tvos
    vlc_modules += {
        'name' : 'uikit_inhibit',
        'sources' : files('inhibit/uikit-inhibit.m'),
        'dependencies' : [uikit_dep, foundation_dep],
        'objc_args' : ['-fobjc-arc']
    }
endif

if get_option('addon_manager')
    # Add-on manager module
    vlc_modules += {
        'name' : 'addonsvorepository',
        'sources' : files('addons/vorepository.c')
    }
    # Add-on filesystem storage module
    vlc_modules += {
        'name' : 'addonsfsstorage',
        'sources' : files('addons/fsstorage.c')
    }
endif

# Wayland inhibit module
if have_wayland
    # FIXME: Includes are currently set up in a way that they will not
    # include this file correctly. This needs to be fixed.
    wl_inhibit_client_proto = custom_target(
        'idle-inhibit-client-protocol-generator',
        output: 'idle-inhibit-client-protocol.h',
        input: wayland_protocols_dir / 'unstable/idle-inhibit/idle-inhibit-unstable-v1.xml',
        command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
    )
    wl_inhibit_proto = custom_target(
        'idle-inhibit-protocol-generator',
        output: 'idle-inhibit-protocol.c',
        input: wayland_protocols_dir / 'unstable/idle-inhibit/idle-inhibit-unstable-v1.xml',
        command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
    )

    vlc_modules += {
        'name' : 'wl_idle_inhibit',
        'sources' : [
            files('inhibit/wl-idle-inhibit.c'),
            wl_inhibit_client_proto,
            wl_inhibit_proto,
        ],
        'dependencies' : [wayland_deps],
    }
endif

# XCB screensaver inhibit module
if xcb_dep.found()
    vlc_modules += {
        'name' : 'xdg_screensaver',
        'sources' : files('inhibit/xdg.c'),
        'dependencies' : [xcb_dep]
    }
endif

# DBUS
if dbus_dep.found()
    vlc_modules += {
        'name' : 'dbus_screensaver',
        'sources' : files('inhibit/dbus.c'),
        'dependencies' : [dbus_dep]
    }
endif
