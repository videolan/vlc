vlc_modules += {
    'name' : 'cache_read',
    'sources' : files('cache_read.c')
}

vlc_modules += {
    'name' : 'cache_block',
    'sources' : files('cache_block.c')
}

if host_system != 'windows' and not have_tvos
  vlc_modules += {
      'name' : 'decomp',
      'sources' : files('decomp.c'),
      'dependencies' : [threads_dep]
  }
endif

if z_lib.found()
  vlc_modules += {
      'name' : 'inflate',
      'sources' : files('inflate.c'),
      'dependencies' : [z_lib]
  }
endif

# TODO: Add !HAVE_WINSTORE check
vlc_modules += {
    'name' : 'prefetch',
    'sources' : files('prefetch.c')
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
