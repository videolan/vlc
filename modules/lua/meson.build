lua_dep = dependency(
  [
    'lua',
    'lua-5.4', 'lua54',
    'lua-5.3', 'lua53',
    'lua-5.2', 'lua52',
    'lua-5.1', 'lua51',
  ],
  version: '>=5.1', required: false)

if not get_option('lua').disabled() and not lua_dep.found()
  error('Could not find lua. Lua is needed for some interfaces (rc, telnet, http) as well as many other custom scripts. Use -Dlua=disabled to ignore this error.')
endif

if lua_dep.found() and get_option('lua').enabled()
  lua_sources = files(
    'extension.c',
    'extension.h',
    'extension_thread.c',
    'intf.c',
    'meta.c',
    'stream_filter.c',
    'services_discovery.c',
    'vlc.c',
    'vlc.h',
    'libs.h',
    'libs/configuration.c',
    'libs/equalizer.c',
    'libs/gettext.c',
    'libs/dialog.c',
    'libs/httpd.c',
    'libs/input.c',
    'libs/input.h',
    'libs/messages.c',
    'libs/misc.c',
    'libs/misc.h',
    'libs/net.c',
    'libs/objects.c',
    'libs/osd.c',
    'libs/playlist.c',
    'libs/sd.c',
    'libs/stream.c',
    'libs/strings.c',
    'libs/variables.c',
    'libs/variables.h',
    'libs/video.c',
    'libs/vlm.c',
    'libs/volume.c',
    'libs/xml.c',
    'libs/io.c',
    'libs/errno.c',
    'libs/rand.c',
    'libs/renderers.c',
    'libs/medialibrary.c',
  )

  if host_system == 'windows'
    lua_sources += files('libs/win.c')
  endif

  vlc_modules += {
    'name' : 'lua',
    'sources' : lua_sources,
    'dependencies' : [lua_dep, socket_libs]
  }
endif
