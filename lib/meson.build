libvlc_sources = [
    'core.c',
    'dialog.c',
    'renderer_discoverer.c',
    'error.c',
    'log.c',
    'playlist.c',
    'video.c',
    'audio.c',
    'event.c',
    'media.c',
    'media_track.c',
    'media_player.c',
    'media_list.c',
    'media_list_path.h',
    'media_list_player.c',
    'media_discoverer.c',
    'picture.c'
]

libvlc = library('vlc',
    libvlc_sources, rev_target,
    include_directories: [vlc_include_dirs],
    link_with: [vlc_libcompat],
    dependencies: [libvlccore_dep, m_lib, threads_dep],
    install: true
)
