# MSI packaging

fs = import('fs')

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

msi_required = get_option('wix').allowed() and not get_option('wix').auto()

python3 = find_program('python3', required: msi_required)
cygpath = find_program('cygpath', required: msi_required)
x86_runner = find_program('wine', required: msi_required)
candlex86 = find_program('candle.exe', required: msi_required)
lightx86 = find_program('light.exe', required: msi_required)

W_MSIDIR = run_command(cygpath, '-w', meson.current_source_dir(), check: true).stdout().strip()
W_MSIBUILDDIR = run_command(cygpath, '-w', meson.current_build_dir(), check: true).stdout().strip()
W_TOPBUILDDIR = run_command(cygpath, '-w', vlc_build_root, check: true).stdout().strip()
W_PREFIX = run_command(cygpath, '-w', prefix_path / '', check: true).stdout().strip()

wix_conf = cdata
wix_conf.set('BUILD_LUA_TRUE', lua_dep.found() and get_option('lua').allowed() ? '' : '#')
wix_conf.set('BUILD_SKINS_TRUE', skins2_feasible ? '' : '#')
wix_conf.set('HAVE_NLS_TRUE', wix_conf.get('ENABLE_NLS', 0) == 1 ? '' : '#')
wix_conf.set('WINDOWS_ARCH', WIX_WINDOWS_ARCH)
wix_conf.set('PREFIX_PATH', W_PREFIX)

vlc_wix = configure_file(
    input: 'config.wxi.in',
    output: 'config.wxi',
    configuration: wix_conf,
    install: true,
    install_dir: '.',
)

heat_dirs = ['Plugins']
if wix_conf.get('ENABLE_NLS', 0) == 1
    heat_dirs += 'Locale'
endif
if lua_dep.found() and get_option('lua').allowed()
    heat_dirs += 'Lua'
endif
if skins2_feasible
    heat_dirs += 'Skins'
endif

wxs_files = [
    [ files('product.wxs'),    W_MSIDIR ],
    [ files('extensions.wxs'), W_MSIDIR ],
]
foreach heat : heat_dirs
    dirname = heat.to_lower()
    groupname = 'Comp' + heat + 'Group'
    wxsname = heat + '.fragment.wxs'

    wxs = custom_target(
        command: [python3, files('msi-heat.py'), '--dir', prefix_path / dirname, '-cg', groupname,
            '-gg', '-scom', '-sreg', '-sfrag', '-dr', 'APPLICATIONFOLDER',
            '-out', '@OUTPUT@'
        ],
        output: wxsname,
        depend_files: [files('msi-heat.py')],
        build_always_stale: true, # always rescan since we can't depend on a folder
        build_by_default: false,
        console: true,
    )
    wxs_files += [ [ wxs, W_MSIBUILDDIR ] ]
endforeach

compiled_wxs = []
compiled_winpath = []
foreach wxsobj : wxs_files
    outfile = fs.replace_suffix(wxsobj[0], '.wixobj')
    outname = fs.name(outfile)
    out_winpath = run_command(cygpath, '-w', outfile, check: true).stdout().strip()

    wixobj = custom_target(
        command: [x86_runner, candlex86.full_path(), '-nologo',
            '-out', out_winpath, '-arch', WIX_WINDOWS_ARCH, '-I' + W_TOPBUILDDIR, '-ext', 'WiXUtilExtension',
            wxsobj[1] + '\\' + '@PLAINNAME@'
        ],
        output: outname,
        input: wxsobj[0],
        depend_files: [vlc_wix],
        build_always_stale: true,
        build_by_default: false,
        console: true,
    )
    compiled_wxs += wixobj
    compiled_winpath += out_winpath
endforeach

msi_name = 'vlc-' + vlc_version + '-' + winstaller_suffix + '.msi'
msi_output = custom_target(
    command: [x86_runner, lightx86, '-nologo',
        '-sval', '-spdb', '-ext', 'WixUIExtension', '-ext', 'WixUtilExtension', '-cultures:en-us',
        '-out', msi_name, compiled_winpath
    ],
    input: compiled_wxs,
    output: msi_name, # real name but root build folder
    build_always_stale: true,
    build_by_default: false,
    console: true,
)

alias_target('package-msi', msi_output)
