# Podcast
vlc_modules += {
    'name' : 'podcast',
    'sources' : files('podcast.c'),
    'dependencies' : [threads_dep]
}

# SAP
vlc_modules += {
    'name' : 'sap',
    'sources' : files('sap.c', '../access/rtp/sdp.c'),
    'dependencies' : [socket_libs, z_lib]
}

# Zeroconf services discovery
avahi_dep = dependency('avahi-client', version: '>= 0.6', required: get_option('avahi'))
if avahi_dep.found()
    vlc_modules += {
        'name' : 'avahi',
        'sources' : files('avahi.c'),
        'dependencies' : [avahi_dep]
    }
endif

# MTP devices support
mtp_dep = dependency('libmtp', version: '>= 1.0.0', required: get_option('mtp'))
if mtp_dep.found()
    vlc_modules += {
        'name' : 'mtp',
        'sources' : files('mtp.c'),
        'dependencies' : [mtp_dep]
    }
endif

# UPnP Plugin (Intel SDK)
upnp_dep = dependency('libupnp', required: get_option('upnp'))
if upnp_dep.found()

    upnp_darwin_deps = []
    if host_system == 'darwin'
        systemconfiguration_dep = dependency('SystemConfiguration', required: true)
        upnp_darwin_deps = [corefoundation_dep, systemconfiguration_dep]
    endif

    vlc_modules += {
        'name' : 'upnp',
        'sources' : files(
            'upnp.cpp',
            'upnp-wrapper.cpp',
            '../stream_out/renderer_common.cpp',
            '../stream_out/dlna/dlna.cpp',
            ),
        'dependencies' : [upnp_dep, upnp_darwin_deps]
    }
endif

# Pulse device list
if pulse_dep.found()
    vlc_modules += {
        'name' : 'pulselist',
        'sources' : files('pulse.c'),
        'link_with' : [libvlc_pulse],
        'dependencies' : [pulse_dep],
    }
endif

# Linux udev device discovery
libudev_dep = dependency('libudev', version: '>= 142', required: get_option('udev'))
if libudev_dep.found()
    vlc_modules += {
        'name' : 'udev',
        'sources' : files('udev.c'),
        'dependencies' : [libudev_dep]
    }
endif

# XCB Apps discovery
if xcb_dep.found()
    vlc_modules += {
        'name' : 'xcb_apps',
        'sources' : files('xcb_apps.c'),
        'dependencies' : [xcb_dep]
    }
endif

# Windows drive discovery
if host_system == 'windows'
    vlc_modules += {
        'name' : 'windrive',
        'sources' : files('windrive.c')
    }
endif

# mDNS using libmicrodns
microdns_dep = dependency('microdns', required: get_option('microdns'))
if microdns_dep.found()
    vlc_modules += {
        'name' : 'microdns',
        'sources' : files('microdns.c'),
        'dependencies' : [microdns_dep, socket_libs]
    }
endif

# mDNS using Bonjour
if host_system == 'darwin'
    vlc_modules += {
        'name' : 'bonjour',
        'sources' : files('bonjour.m'),
        'objc_args' : ['-fobjc-arc'],
        'dependencies' : [foundation_dep]
    }
endif
