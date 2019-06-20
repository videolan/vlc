# Memory keystore
vlc_modules += {
    'name' : 'memory_keystore',
    'sources' : files('memory.c', 'list_util.c')
}

# File keystore
file_keystore_extra_sources = []
file_keystore_extra_deps = []
if host_system == 'windows'
    file_keystore_extra_sources += 'file_crypt_win32.c'
    file_keystore_extra_deps += cc.find_library('crypt32', required: true)
endif
# TODO: Add Android-specific sources
vlc_modules += {
    'name' : 'file_keystore',
    'sources' : files('file.c', 'list_util.c', file_keystore_extra_sources),
    'dependencies' : file_keystore_extra_deps
}

# libsecret keystore
libsecret_dep = dependency('libsecret-1', version: '>= 0.18', required: get_option('libsecret'))
if libsecret_dep.found()
    vlc_modules += {
        'name' : 'secret',
        'sources' : files('secret.c'),
        'dependencies' : [libsecret_dep]
    }
endif

# KWallet keystore
if dbus_dep.found()
    vlc_modules += {
        'name' : 'kwallet',
        'sources' : files('kwallet.c'),
        'dependencies' : [dbus_dep, m_lib]
    }
endif

# Keychain (for appleOSes)
if host_system == 'darwin'
    vlc_modules += {
        'name' : 'keychain',
        'sources' : files('keychain.m', 'list_util.c'),
        'objc_args' : ['-fobjc-arc'],
        'dependencies' : [foundation_dep, security_dep]
    }
endif
