# TODO handle msi packaging

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
