#  nsis packaging

nsis_required = get_option('nsis').allowed() and not get_option('nsis').auto()
makensis = find_program('makensis', required: nsis_required)

nsi_conf = cdata
nsi_conf.set('BUILD_LUA_TRUE', lua_dep.found() and get_option('lua').allowed() ? '' : '#')
nsi_conf.set('BUILD_SKINS_TRUE', skins2_feasible ? '' : '#')
nsi_conf.set('HAVE_WIN64_TRUE',  have_win64 ? '' : '#')
nsi_conf.set('HAVE_WIN64_FALSE', have_win64 ? '#' : '')
nsi_conf.set('HAVE_ARM64_TRUE',  host_machine.cpu_family() == 'aarch64' or host_machine.cpu() == 'arm64' ? '' : '#')
nsi_conf.set('HAVE_ARM64_FALSE', host_machine.cpu_family() == 'aarch64' or host_machine.cpu() == 'arm64' ? '#' : '')
nsi_conf.set('PROGRAMFILES', have_win64 ? 'PROGRAMFILES64' : 'PROGRAMFILES')

nsProcess_cargs = ['-D_UNICODE=1', '-DUNICODE=1']
if not have_win64
    nsProcess_cargs += '-D_WIN32_IE=0x0601'
endif
nsProcess_link_args = ['-static-libgcc']

if have_win64
    # nsProcess.dll needs to be a 32-bit DLL
    # with clang we can select it via -target i686-w64-mingw32
    if cc.has_argument(['--target=i686-w64-mingw32']) and cc.has_link_argument(['--target=i686-w64-mingw32'])
        nsProcess_cargs     += '--target=i686-w64-mingw32'
        nsProcess_link_args += '--target=i686-w64-mingw32'
    endif
endif

nsProcess = shared_library('nsProcess',
                           'nsProcess/nsProcess.c', 'nsProcess/pluginapi.c',
                           c_args: nsProcess_cargs,
                           link_args: nsProcess_link_args,
                           dependencies : [ cc.find_library('ole32', required: true) ],
                           name_prefix: '',
                           install: true,
                           install_dir: 'NSIS')

vlc_nsi = configure_file(
  input: 'vlc.win32.nsi.in',
  output: 'vlc.win32.nsi',
  configuration: nsi_conf,
  install: true,
  install_dir: '.',
)

spad_nsi = configure_file(
  input: 'spad.nsi.in',
  output: 'spad.nsi',
  configuration: nsi_conf,
  install: true,
  install_dir: '.',
)

install_data('nsProcess.nsh',    install_dir : 'NSIS')
install_data('vlc_branding.bmp', install_dir : 'NSIS')
install_subdir('languages',      install_dir : '.')
install_subdir('helpers',        install_dir : '.')

if have_win_store
  nsi_script_dir = 'vlc-' + winstaller_suffix + '-uwp'
elif cc.compiles(ucrt_version_test, name: 'compiles with Universal C Runtime')
  nsi_script_dir = 'vlc-' + winstaller_suffix + '-ucrt'
else
  nsi_script_dir = 'vlc-' + winstaller_suffix
endif

if makensis.found()
    spad_nsis = custom_target('spad-nsis',
        command: [makensis, nsi_script_dir / 'spad.nsi'],
        input: spad_nsi,
        output: 'spad-setup.exe', # real name but located in installed folder
        build_by_default: false,
        console: true)

    vlc_nsis = custom_target('vlc-nsis',
        command: [makensis, nsi_script_dir / 'vlc.win32.nsi'],
        input: vlc_nsi,
        output: 'vlc-' + vlc_version + '-' + winstaller_suffix + '.exe', # real name but root build folder
        build_by_default: false,
        console: true,
        depends: nsProcess)

    alias_target('nsis', spad_nsis, vlc_nsis)
endif
