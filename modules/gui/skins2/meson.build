skins2_flags = []
skins2_deps = [ freetype_dep ]

fribidi_dep = dependency('fribidi', required: get_option('fribidi'))
if fribidi_dep.found()
    skins2_deps += [ fribidi_dep ]
    skins2_flags += [ '-DHAVE_FRIBIDI' ]

    harfbuzz_dep = dependency('harfbuzz', required: get_option('harfbuzz'))
    if harfbuzz_dep.found()
        skins2_deps += [ harfbuzz_dep ]
        skins2_flags += [ '-DHAVE_HARFBUZZ' ]
    endif
endif

if cc.has_function('lrintf', dependencies: [m_lib])
    cdata.set('HAVE_LRINTF', 1)
    skins2_deps += [ m_lib ]
endif

skins2_sources = files(
    'commands/async_queue.cpp',
    'commands/cmd_add_item.cpp',
    'commands/cmd_audio.cpp',
    'commands/cmd_dvd.cpp',
    'commands/cmd_change_skin.cpp',
    'commands/cmd_fullscreen.cpp',
    'commands/cmd_input.cpp',
    'commands/cmd_layout.cpp',
    'commands/cmd_muxer.cpp',
    'commands/cmd_on_top.cpp',
    'commands/cmd_playlist.cpp',
    'commands/cmd_playtree.cpp',
    'commands/cmd_minimize.cpp',
    'commands/cmd_quit.cpp',
    'commands/cmd_resize.cpp',
    'commands/cmd_snapshot.cpp',
    'commands/cmd_vars.cpp',
    'controls/ctrl_button.cpp',
    'controls/ctrl_checkbox.cpp',
    'controls/ctrl_generic.cpp',
    'controls/ctrl_image.cpp',
    'controls/ctrl_list.cpp',
    'controls/ctrl_tree.cpp',
    'controls/ctrl_move.cpp',
    'controls/ctrl_resize.cpp',
    'controls/ctrl_slider.cpp',
    'controls/ctrl_radialslider.cpp',
    'controls/ctrl_text.cpp',
    'controls/ctrl_video.cpp',
    'events/evt_input.cpp',
    'events/evt_key.cpp',
    'events/evt_mouse.cpp',
    'events/evt_special.cpp',
    'events/evt_scroll.cpp',
    'parser/builder.cpp',
    'parser/expr_evaluator.cpp',
    'parser/interpreter.cpp',
    'parser/skin_parser.cpp',
    'parser/xmlparser.cpp',
    'src/anchor.cpp',
    'src/anim_bitmap.cpp',
    'src/bitmap_font.cpp',
    'src/dialogs.cpp',
    'src/file_bitmap.cpp',
    'src/art_manager.cpp',
    'src/ft2_bitmap.cpp',
    'src/ft2_font.cpp',
    'src/ft2_err.c',
    'src/generic_bitmap.cpp',
    'src/generic_layout.cpp',
    'src/generic_window.cpp',
    'src/ini_file.cpp',
    'src/logger.cpp',
    'src/os_factory.cpp',
    'src/popup.cpp',
    'src/scaled_bitmap.cpp',
    'src/skin_main.cpp',
    'src/theme.cpp',
    'src/theme_loader.cpp',
    'src/theme_repository.cpp',
    'src/tooltip.cpp',
    'src/top_window.cpp',
    'src/fsc_window.cpp',
    'src/var_manager.cpp',
    'src/vlcproc.cpp',
    'src/vout_window.cpp',
    'src/vout_manager.cpp',
    'src/window_manager.cpp',
    'utils/bezier.cpp',
    'utils/fsm.cpp',
    'utils/position.cpp',
    'utils/ustring.cpp',
    'utils/var_bool.cpp',
    'utils/var_list.cpp',
    'utils/var_percent.cpp',
    'utils/var_text.cpp',
    'utils/var_string.cpp',
    'utils/var_tree.cpp',
    'vars/equalizer.cpp',
    'vars/playtree.cpp',
    'vars/time.cpp',
    'vars/volume.cpp',
)


if host_system == 'windows'
skins2_flags += [ '-DWIN32_SKINS' ]
skins2_deps += [
    cc.find_library('gdi32'),
    cc.find_library('ole32'),
    cc.find_library('uuid'),
    cc.find_library('msimg32'),
]
skins2_sources += files(
    'win32/win32_dragdrop.cpp',
    'win32/win32_factory.cpp',
    'win32/win32_graphics.cpp',
    'win32/win32_loop.cpp',
    'win32/win32_popup.cpp',
    'win32/win32_timer.cpp',
    'win32/win32_tooltip.cpp',
    'win32/win32_window.cpp',
)
endif

if x11_dep.found()
skins2_flags += [ '-DX11_SKINS' ]
skins2_sources += files(
    'x11/x11_display.cpp',
    'x11/x11_dragdrop.cpp',
    'x11/x11_factory.cpp',
    'x11/x11_graphics.cpp',
    'x11/x11_loop.cpp',
    'x11/x11_popup.cpp',
    'x11/x11_timer.cpp',
    'x11/x11_window.cpp',
    'x11/x11_tooltip.cpp',
)
skins2_deps += [
    x11_dep,
    dependency('xpm', required: true),
    dependency('xinerama', required: true),
    dependency('xcursor', required: true),
    dependency('xext', required: true),
]
endif

vlc_modules += {
    'name' : 'skins2',
    'sources' : [ skins2_sources ],
    'dependencies' : [ skins2_deps ],
    'c_args' : skins2_flags,
    'cpp_args' : [skins2_flags, '-O2', '-fno-rtti'],
    'enabled' : get_option('skins2')
      .disable_auto_if(host_system != 'windows' and not x11_dep.found()).allowed() and
      freetype_dep.found() and host_system != 'darwin'
}
