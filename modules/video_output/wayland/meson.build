if not have_wayland
    subdir_done()
endif

wayland_client_dep = wayland_deps[0]
wayland_cursor_dep = wayland_deps[1]
wayland_egl_dep = wayland_deps[2]

wayland_inc = include_directories('.')

# Protocol code generation
viewporter_xml = wayland_protocols_dir / 'stable/viewporter/viewporter.xml'

viewporter_client_hdr = custom_target(
    'wayland-viewporter-client-header',
    output: 'viewporter-client-protocol.h',
    input: viewporter_xml,
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)

viewporter_code = custom_target(
    'wayland-viewporter-code',
    output: 'viewporter-protocol.c',
    input: viewporter_xml,
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

xdg_shell_xml = wayland_protocols_dir / 'stable/xdg-shell/xdg-shell.xml'

xdg_shell_client_hdr = custom_target(
    'wayland-xdg-shell-client-header',
    output: 'xdg-shell-client-protocol.h',
    input: xdg_shell_xml,
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)

xdg_shell_code = custom_target(
    'wayland-xdg-shell-code',
    output: 'xdg-shell-protocol.c',
    input: xdg_shell_xml,
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

xdg_decoration_xml = wayland_protocols_dir / 'unstable/xdg-decoration/xdg-decoration-unstable-v1.xml'

xdg_decoration_client_hdr = custom_target(
    'wayland-xdg-decoration-client-header',
    output: 'xdg-decoration-client-protocol.h',
    input: xdg_decoration_xml,
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)
xdg_decoration_code = custom_target(
    'wayland-xdg-decoration-code',
    output: 'xdg-decoration-protocol.c',
    input: xdg_decoration_xml,
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)
xdg_decoration_sources = [xdg_decoration_client_hdr, xdg_decoration_code]

vlc_modules += {
    'name': 'wl_shm',
    'sources': [
        files('registry.c', 'registry.h', 'shm.c'),
        viewporter_client_hdr,
        viewporter_code,
    ],
    'include_directories': [wayland_inc],
    'dependencies': [wayland_client_dep],
}

xkbcommon_dep = dependency('xkbcommon', required: false)

shell_common_sources = files(
    'output.c', 'output.h',
    'input.c', 'input.h',
    '../xcb/xkb.c',
    'xdg-shell.c',
)

shell_common_deps = [wayland_client_dep, wayland_cursor_dep]
shell_common_cargs = []

if xkbcommon_dep.found()
    shell_common_cargs += '-DHAVE_XKBCOMMON'
    shell_common_deps += xkbcommon_dep
endif

vlc_modules += {
    'name': 'xdg_shell',
    'sources': [
        shell_common_sources,
        xdg_shell_client_hdr,
        xdg_shell_code,
        xdg_decoration_sources,
    ],
    'c_args': shell_common_cargs + ['-DXDG_SHELL'],
    'include_directories': [wayland_inc],
    'dependencies': shell_common_deps,
}

vlc_modules += {
    'name': 'wl_shell',
    'sources': [
        shell_common_sources,
    ],
    'c_args': shell_common_cargs,
    'include_directories': [wayland_inc],
    'dependencies': shell_common_deps,
}

if egl_dep.found()
    vlc_modules += {
        'name': 'egl_wl',
        'sources': files('../opengl/egl.c'),
        'c_args': ['-DUSE_PLATFORM_WAYLAND=1'],
        'dependencies': [egl_dep, wayland_egl_dep],
    }

    if gbm_dep.found() and drm_dep.found()
        linux_dmabuf_xml = wayland_protocols_dir / 'unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml'

        linux_dmabuf_client_hdr = custom_target(
            'wayland-linux-dmabuf-client-header',
            output: 'linux-dmabuf-client-protocol.h',
            input: linux_dmabuf_xml,
            command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
        )
        linux_dmabuf_code = custom_target(
            'wayland-linux-dmabuf-code',
            output: 'linux-dmabuf-protocol.c',
            input: linux_dmabuf_xml,
            command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
        )

        vlc_modules += {
            'name': 'egl_gbm_wl',
            'sources': [
                files('../opengl/egl_gbm.c'),
                linux_dmabuf_client_hdr,
                linux_dmabuf_code,
            ],
            'include_directories': [wayland_inc],
            'dependencies': [egl_dep, wayland_egl_dep, gbm_dep, drm_dep],
        }
    endif
endif
