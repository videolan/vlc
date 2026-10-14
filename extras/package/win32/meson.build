install_data('../../../AUTHORS',   rename : 'AUTHORS.txt', install_dir : '.')
install_data('../../../COPYING',   rename : 'COPYING.txt', install_dir : '.')
install_data('../../../NEWS',      rename : 'NEWS.txt',    install_dir : '.')
install_data('../../../README.md', rename : 'README.txt',  install_dir : '.')
install_data('../../../THANKS',    rename : 'THANKS.txt',  install_dir : '.')

install_data('../../../share/icons/vlc.ico',  install_dir : '.')

have_win64 = host_machine.cpu_family() == 'x86_64' or host_machine.cpu_family() == 'aarch64' or host_machine.cpu() == 'arm64'
if have_win64
    if host_machine.cpu_family() == 'aarch64'
        winstaller_suffix = 'winarm64'
    else
        winstaller_suffix = 'win64'
    endif
else
    if host_machine.cpu_family() == 'arm'
        winstaller_suffix = 'winarm'
    else
        winstaller_suffix = 'win32'
    endif
endif

if get_option('nsis').disable_auto_if(
    host_system != 'windows' or have_win_store).allowed()
    subdir('NSIS')
endif

if get_option('wix').disable_auto_if(
    host_system != 'windows' or have_win_store).allowed()
    subdir('msi')
endif

sevenz = find_program('7z', required: false)
if sevenz.found()
    sevenz_opts = ['-t7z', '-m0=lzma', '-mx=9', '-mfb=64', '-md=32m', '-ms=on']
    release_7z = 'vlc-' + vlc_version + '-' + winstaller_suffix + '.7z'
    debug_7z   = 'vlc-' + vlc_version + '-' + winstaller_suffix + '-debug.7z'

    # generate a 7z from the "installed" and stripped folder
    package_win32_7zip = custom_target(
        output: release_7z, # real name but located in root build folder
        command: [sevenz, 'a', sevenz_opts, release_7z, prefix_path],
        console: true,
    )
    alias_target('package-win32-7zip', package_win32_7zip)

    # generate a 7z from the "installed" folder with debug symbols
    package_win32_debug_7zip = custom_target(
        output: debug_7z, # real name but located in root build folder
        command: [sevenz, 'a', sevenz_opts, debug_7z, prefix_path],
        console: true,
    )
    # disabled for now as meson doesn't understand we are building with .pdb files
    # alias_target('package-win32-debug-7zip', package_win32_debug_7zip)
endif
