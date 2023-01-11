if get_option('screen').allowed()
  # XCB screen grab
  if xcb_dep.found() and xcb_shm_dep.found() and xcb_composite_dep.found()
      vlc_modules += {
          'name' : 'xcb_screen',
          'sources' : files('xcb.c'),
          'dependencies' : [xcb_dep, xcb_shm_dep, xcb_composite_dep]
      }
  endif

  # screen
  if host_system == 'windows' or have_osx
    screen_files = files(
      'screen.c',
      'screen.h'
    )

    screen_deps = []
    screen_cppargs = []
    screen_link_with = []

    if host_system == 'windows'
      screen_files += files('win32.c', 'dxgi.cpp')
      gdi32_dep = cc.find_library('gdi32')
      screen_deps += [gdi32_dep]
      screen_cppargs += libcom_cppflags
      screen_link_with += d3d11_common_lib
    else
      screen_files += files('mac.c')
      applicationservices_dep = dependency('ApplicationServices', required: true)
      screen_deps += [applicationservices_dep]
    endif

    vlc_modules += {
        'name' : 'screen',
        'sources' : screen_files,
        'dependencies' : screen_deps,
        'cpp_args' : screen_cppargs,
        'link_with' : screen_link_with,
    }
  endif
endif
