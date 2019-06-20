# Text renderer modules
# Modules that are used to render subtitles and OSD text

# Dummy text renderer
vlc_modules += {
    'name' : 'tdummy',
    'sources' : files('tdummy.c')
}

# Freetype text renderer
freetype_deps = [freetype_dep, m_lib]
freetype_srcs = files(
    'freetype/freetype.c',
    'freetype/platform_fonts.c',
    'freetype/text_layout.c',
    'freetype/ftcache.c',
    'freetype/lru.c',
)
if host_system == 'windows'
    freetype_srcs += files('freetype/fonts/dwrite.cpp')
    # TODO: Don't add this file for UWP builds
    freetype_srcs += files('freetype/fonts/win32.c')
elif host_system == 'darwin'
    freetype_srcs += files('freetype/fonts/darwin.c')
    freetype_deps += [corefoundation_dep, coretext_dep]
endif
# TODO: Anroid-specific sources
# TODO: Fribidi support
# TODO: Harfbuzz support
if freetype_dep.found()
    vlc_modules += {
        'name' : 'freetype',
        'sources' : freetype_srcs,
        'dependencies' : freetype_deps,
    }
endif

# SVG plugin
if rsvg_dep.found()
    vlc_modules += {
        'name' : 'svg',
        'sources' : files('svg.c'),
        'dependencies' : [rsvg_dep]
    }
endif

# macOS text to speech
if host_system == 'darwin'
    vlc_modules += {
        'name' : 'nsspeechsynthesizer',
        'sources' : files('nsspeechsynthesizer.m'),
        'dependencies' : [cocoa_dep]
    }
endif

# Windows SAPI text to speech
if host_system == 'windows'
    vlc_modules += {
        'name' : 'sapi',
        'sources' : files('sapi.cpp')
    }
endif
