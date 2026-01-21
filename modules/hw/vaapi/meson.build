# VAAPI decoder device modules

vaapi_common_sources = files(
    'decoder_device.c',
    'vlc_vaapi.c',
)

# Check for VA-API backends (only if libva is available)
if libva_dep.found()
    libva_x11_dep = dependency('libva-x11', required: false)
    libva_drm_dep = dependency('libva-drm', required: false)
    libva_wayland_dep = dependency('libva-wayland', required: false)
else
    libva_x11_dep = disabler()
    libva_drm_dep = disabler()
    libva_wayland_dep = disabler()
endif

# VAAPI decoder device for X11
vlc_modules += {
    'name' : 'decdev_vaapi_x11',
    'sources' : vaapi_common_sources,
    'c_args' : ['-DHAVE_VA_X11'],
    'dependencies' : [libva_dep, libva_x11_dep, x11_dep],
    'enabled' : libva_dep.found() and libva_x11_dep.found() and x11_dep.found(),
}

# VAAPI decoder device for DRM
vlc_modules += {
    'name' : 'decdev_vaapi_drm',
    'sources' : vaapi_common_sources,
    'c_args' : ['-DHAVE_VA_DRM'],
    'dependencies' : [libva_dep, libva_drm_dep],
    'enabled' : libva_dep.found() and libva_drm_dep.found(),
}

# VAAPI decoder device for Wayland
vlc_modules += {
    'name' : 'decdev_vaapi_wl',
    'sources' : vaapi_common_sources,
    'c_args' : ['-DHAVE_VA_WL'],
    'dependencies' : [libva_dep, libva_wayland_dep] + wayland_deps,
    'enabled' : libva_dep.found() and libva_wayland_dep.found() and have_wayland,
}
