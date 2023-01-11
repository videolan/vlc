# Console logger module
vlc_modules += {
    'name' : 'console_logger',
    'sources' : files('console.c'),
}

# File logger
vlc_modules += {
    'name' : 'file_logger',
    'sources' : files('file.c')
}

# Syslog logger
if cc.check_header('syslog.h')
    vlc_modules += {
        'name' : 'syslog',
        'sources' : files('syslog.c')
    }
endif

# Systemd journal logger
libsystemd_dep = dependency('libsystemd', required: false)
if libsystemd_dep.found()
    vlc_modules += {
        'name' : 'sd_journal',
        'sources' : files('journal.c'),
        'dependencies' : [libsystemd_dep]
    }
endif

vlc_modules += {
    'name' : 'json_tracer',
    'sources' : files('json.c')
}
