# MSI packaging

if have_win64
    if host_machine.cpu_family() == 'aarch64'
        WIX_WINDOWS_ARCH = 'arm64'
    else
        WIX_WINDOWS_ARCH = 'x64'
    endif
else
    if host_machine.cpu_family() == 'arm'
        WIX_WINDOWS_ARCH = 'arm'
    else
        WIX_WINDOWS_ARCH = 'x86'
    endif
endif

wix_conf = cdata
wix_conf.set('BUILD_LUA_TRUE', lua_dep.found() and get_option('lua').allowed() ? '' : '#')
wix_conf.set('BUILD_SKINS_TRUE', skins2_feasible ? '' : '#')
wix_conf.set('HAVE_NLS_TRUE', wix_conf.get('ENABLE_NLS', 0) == 1 ? '' : '#')
wix_conf.set('WINDOWS_ARCH', WIX_WINDOWS_ARCH)

vlc_wix = configure_file(
  input: 'config.wxi.in',
  output: 'config.wxi',
  configuration: wix_conf,
  install: true,
  install_dir: '.',
)
