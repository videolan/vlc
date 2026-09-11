lua_dep = dependency(
  [
    'lua', 'Lua',
    'lua-5.4', 'lua54',
    'lua-5.3', 'lua53',
    'lua-5.2', 'lua52',
    'lua-5.1', 'lua51',
  ],
  version: '>=5.1', required: false)

if get_option('lua').enabled() and not lua_dep.found()
  error('Could not find lua. Lua is needed for some interfaces (rc, telnet, http) as well as many other custom scripts. Use -Dlua=disabled to ignore this error.')
endif

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
    'libs/tcp.c',
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
  'dependencies' : [lua_dep, socket_libs],
  'enabled' : lua_dep.found() and get_option('lua').allowed(),
}

if lua_dep.found() and get_option('lua').allowed()
  if host_system == 'windows'
    luadatadir_path = prefix_path + 'lua'
  else
    luadatadir_path = pkglibdir_path + 'lua'
  endif

  luac = find_program('luac', required: true)

  lua_modules = files(
    '../../share/lua/extensions/VLSub.lua',
    '../../share/lua/intf/cli.lua',
    '../../share/lua/intf/dummy.lua',
    '../../share/lua/intf/dumpmeta.lua',
    '../../share/lua/intf/http.lua',
    '../../share/lua/intf/luac.lua',
    '../../share/lua/intf/modules/host.lua',
    '../../share/lua/intf/modules/httprequests.lua',
    '../../share/lua/intf/telnet.lua',
    '../../share/lua/meta/art/02_frenchtv.lua',
    '../../share/lua/meta/art/03_lastfm.lua',
    '../../share/lua/meta/art/01_googleimage.lua',
    '../../share/lua/meta/art/00_musicbrainz.lua',
    '../../share/lua/meta/reader/filename.lua',
    '../../share/lua/modules/common.lua',
    '../../share/lua/modules/dkjson.lua',
    '../../share/lua/modules/sandbox.lua',
    '../../share/lua/modules/simplexml.lua',
    '../../share/lua/playlist/anevia_streams.lua',
    '../../share/lua/playlist/anevia_xml.lua',
    '../../share/lua/playlist/bbc_co_uk.lua',
    '../../share/lua/playlist/cue.lua',
    '../../share/lua/playlist/dailymotion.lua',
    '../../share/lua/playlist/extreme.lua',
    '../../share/lua/playlist/france2.lua',
    '../../share/lua/playlist/jamendo.lua',
    '../../share/lua/playlist/katsomo.lua',
    '../../share/lua/playlist/koreus.lua',
    '../../share/lua/playlist/lelombrik.lua',
    '../../share/lua/playlist/mpora.lua',
    '../../share/lua/playlist/newgrounds.lua',
    '../../share/lua/playlist/pinkbike.lua',
    '../../share/lua/playlist/rockbox_fm_presets.lua',
    '../../share/lua/playlist/soundcloud.lua',
    '../../share/lua/playlist/vimeo.lua',
    '../../share/lua/playlist/vocaroo.lua',
    '../../share/lua/playlist/twitch.lua',
    '../../share/lua/playlist/zapiks.lua',
    '../../share/lua/sd/fmc.lua',
    '../../share/lua/sd/jamendo.lua',
  )

  foreach m : lua_modules
    module_path = m.full_path().replace(vlc_src_root / 'share/lua/', '')
    build_name = module_path.replace('.lua', '').underscorify()
    splitpath = module_path.split('/')
    output = splitpath.get(splitpath.length() - 1).replace('.lua', '.luac')
    subdir = '/'.join(splitpath.slice(0, splitpath.length() - 1))
    custom_target(build_name,
                  input:   [m],
                  output:  [output],
                  build_by_default: true,
                  build_subdir: subdir,
                  install: true,
                  install_dir: luadatadir_path / subdir,
                  command: [luac, '-o', '@OUTPUT@', '@INPUT0@'])
  endforeach

  lua_pkgdata = files(
    '../../share/lua/http/custom.lua',
    '../../share/lua/http/view.html',
    '../../share/lua/http/mobile.html',
    '../../share/lua/http/dialogs/create_stream.html',
    '../../share/lua/http/dialogs/equalizer_window.html',
    '../../share/lua/http/dialogs/mosaic_window.html',
    '../../share/lua/http/dialogs/batch_window.html',
    '../../share/lua/http/dialogs/error_window.html',
    '../../share/lua/http/dialogs/browse_window.html',
    '../../share/lua/http/dialogs/stream_window.html',
    '../../share/lua/http/dialogs/offset_window.html',
    '../../share/lua/http/dialogs/stream_config_window.html',
    '../../share/lua/http/vlm.html',
    '../../share/lua/http/images/buttons.png',
    '../../share/lua/http/images/speaker-32.png',
    '../../share/lua/http/images/vlc-48.png',
    '../../share/lua/http/images/Audio-48.png',
    '../../share/lua/http/images/Folder-48.png',
    '../../share/lua/http/images/Other-48.png',
    '../../share/lua/http/images/Back-48.png',
    '../../share/lua/http/images/Video-48.png',
    '../../share/lua/http/images/vlc16x16.png',
    '../../share/lua/http/mobile_equalizer.html',
    '../../share/lua/http/vlm_export.html',
    '../../share/lua/http/mobile_browse.html',
    '../../share/lua/http/favicon.ico',
    '../../share/lua/http/mobile_view.html',
    '../../share/lua/http/requests/playlist.xml',
    '../../share/lua/http/requests/playlist.json',
    '../../share/lua/http/requests/README.txt',
    '../../share/lua/http/requests/playlist_jstree.xml',
    '../../share/lua/http/requests/browse.xml',
    '../../share/lua/http/requests/browse.json',
    '../../share/lua/http/requests/vlm_cmd.xml',
    '../../share/lua/http/requests/status.xml',
    '../../share/lua/http/requests/status.json',
    '../../share/lua/http/requests/rd.json',
    '../../share/lua/http/requests/rd.xml',
    '../../share/lua/http/requests/vlm.xml',
    '../../share/lua/http/index.html',
    '../../share/lua/http/css/ui-lightness/jquery-ui-1.8.13.custom.css',
    '../../share/lua/http/css/ui-lightness/images/ui-bg_diagonals-thick_20_666666_40x40.png',
    '../../share/lua/http/css/ui-lightness/images/ui-bg_flat_10_000000_40x100.png',
    '../../share/lua/http/css/ui-lightness/images/ui-icons_ffffff_256x240.png',
    '../../share/lua/http/css/ui-lightness/images/ui-bg_gloss-wave_35_f6a828_500x100.png',
    '../../share/lua/http/css/ui-lightness/images/ui-bg_glass_65_ffffff_1x400.png',
    '../../share/lua/http/css/ui-lightness/images/ui-icons_222222_256x240.png',
    '../../share/lua/http/css/ui-lightness/images/ui-bg_glass_100_f6f6f6_1x400.png',
    '../../share/lua/http/css/ui-lightness/images/ui-bg_highlight-soft_100_eeeeee_1x100.png',
    '../../share/lua/http/css/ui-lightness/images/ui-bg_highlight-soft_75_ffe45c_1x100.png',
    '../../share/lua/http/css/ui-lightness/images/ui-bg_diagonals-thick_18_b81900_40x40.png',
    '../../share/lua/http/css/ui-lightness/images/ui-icons_ffd27a_256x240.png',
    '../../share/lua/http/css/ui-lightness/images/ui-icons_228ef1_256x240.png',
    '../../share/lua/http/css/ui-lightness/images/ui-icons_ef8c08_256x240.png',
    '../../share/lua/http/css/ui-lightness/images/ui-bg_glass_100_fdf5ce_1x400.png',
    '../../share/lua/http/css/main.css',
    '../../share/lua/http/css/mobile.css',
    '../../share/lua/http/js/ui.js',
    '../../share/lua/http/js/common.js',
    '../../share/lua/http/js/jquery.jstree.js',
    '../../share/lua/http/js/controllers.js',
  )

  foreach m : lua_pkgdata
    splitpath = m.full_path().replace(vlc_src_root / 'share/lua/', '').split('/')
    subdir = '/'.join(splitpath.slice(0, splitpath.length() - 1))
    install_data(m, install_dir: luadatadir_path / subdir)
  endforeach

endif