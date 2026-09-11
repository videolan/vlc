# TODO handle msi packaging

install_data('../../../AUTHORS',   rename : 'AUTHORS.txt', install_dir : '.')
install_data('../../../COPYING',   rename : 'COPYING.txt', install_dir : '.')
install_data('../../../NEWS',      rename : 'NEWS.txt',    install_dir : '.')
install_data('../../../README.md', rename : 'README.txt',  install_dir : '.')
install_data('../../../THANKS',    rename : 'THANKS.txt',  install_dir : '.')

install_data('../../../share/icons/vlc.ico',  install_dir : '.')

if get_option('nsis').allowed()
    subdir('NSIS')
endif
