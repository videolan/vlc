vlc_modules += {
    'name' : 'cache_read',
    'sources' : files('cache_read.c')
}

if host_system != 'windows' and not have_tvos
  vlc_modules += {
      'name' : 'decomp',
      'sources' : files('decomp.c'),
      'dependencies' : [threads_dep]
  }
endif

if z_dep.found()
  vlc_modules += {
      'name' : 'inflate',
      'sources' : files('inflate.c'),
      'dependencies' : [z_dep]
  }
endif

vlc_modules += {
    'name' : 'prefetch',
    'sources' : files('prefetch.c'),
    'enabled' : not have_win_store
}

vlc_modules += {
    'name' : 'hds',
    'sources' : files('hds/hds.c')
}

vlc_modules += {
    'name' : 'record',
    'sources' : files('record.c')
}

vlc_modules += {
    'name' : 'adf',
    'sources' : files('adf.c')
}

vlc_modules += {
    'name' : 'skiptags',
    'sources' : files('skiptags.c')
}

aribb25_dep = dependency('aribb25', version:'>= 0.2.6', required: get_option('aribb25'))
vlc_modules += {
    'name' : 'aribcam',
    'sources' : files('aribcam.c'),
    'dependencies' : [aribb25_dep],
    'enabled': aribb25_dep.found(),
}
