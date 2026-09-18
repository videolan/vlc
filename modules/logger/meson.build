# Console logger module
vlc_modules += {
    'name' : 'console_logger',
    'sources' : files('console.c'),
    'plugin_dir' : 'logger',
}

# File logger
vlc_modules += {
    'name' : 'file_logger',
    'sources' : files('file.c'),
    'plugin_dir' : 'logger',
}

# Syslog logger
vlc_modules += {
    'name' : 'syslog',
    'sources' : files('syslog.c'),
    'plugin_dir' : 'logger',
    'enabled' : cc.check_header('syslog.h'),
}

# Systemd journal logger
libsystemd_dep = dependency('libsystemd', required: false)
vlc_modules += {
    'name' : 'sd_journal',
    'sources' : files('journal.c'),
    'dependencies' : [libsystemd_dep],
    'plugin_dir' : 'logger',
    'enabled' : libsystemd_dep.found(),
}

vlc_modules += {
    'name' : 'json_tracer',
    'sources' : files('json.c'),
    'plugin_dir' : 'logger',
}

vlc_rust_modules += {
    'name' : 'telegraf_rs',
    'sources' : files('telegraf-rs/src/lib.rs'),
    'cargo_toml' : files('telegraf-rs/Cargo.toml'),
}
