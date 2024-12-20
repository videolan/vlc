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
freetype_cppargs = []
freetype_cargs = []
if host_system == 'windows'
    freetype_srcs += files('freetype/fonts/dwrite.cpp')
    freetype_cppargs += libcom_cppflags
    # TODO: Don't add this file for UWP builds
    freetype_srcs += files('freetype/fonts/win32.c')
    freetype_deps += [ cc.find_library('dwrite', required: true) ]
elif host_system == 'darwin'
    freetype_srcs += files('freetype/fonts/darwin.c')
    freetype_deps += [corefoundation_dep, coretext_dep]
endif
# TODO: Anroid-specific sources
# TODO: Fribidi support
# TODO: Harfbuzz support

fontconfig_dep = dependency('fontconfig', version: '>= 2.11',
    required: get_option('fontconfig').disable_auto_if(host_system in ['darwin', 'windows']))

if fontconfig_dep.found()
    freetype_srcs += files('freetype/fonts/fontconfig.c')
    freetype_cargs += '-DHAVE_FONTCONFIG'
    freetype_deps += fontconfig_dep
endif

fribidi_dep = dependency('fribidi', required: get_option('fribidi'))

if fribidi_dep.found()
    freetype_cargs += '-DHAVE_FRIBIDI'
    freetype_deps += fribidi_dep
endif

harfbuzz_dep = dependency('harfbuzz', required: get_option('harfbuzz'))

if harfbuzz_dep.found()
    freetype_cargs += '-DHAVE_HARFBUZZ'
    freetype_deps += harfbuzz_dep
endif

vlc_modules += {
    'name' : 'freetype',
    'sources' : freetype_srcs,
    'dependencies' : freetype_deps,
    'c_args' : freetype_cargs,
    'cpp_args' : freetype_cppargs,
    'enabled' : freetype_dep.found(),
}

# SVG plugin
vlc_modules += {
    'name' : 'svg',
    'sources' : files('svg.c'),
    'dependencies' : [rsvg_dep, cairo_dep],
    'enabled' : rsvg_dep.found() and cairo_dep.found(),
}

# macOS text to speech
vlc_modules += {
    'name' : 'nsspeechsynthesizer',
    'sources' : files('nsspeechsynthesizer.m'),
    'dependencies' : [cocoa_dep],
    'enabled' : have_osx,
}


# Windows SAPI text to speech
if host_system == 'windows'
    have_sapi = cpp.has_type('ISpObjectToken', prefix: '\n'.join([
        '#include <windows.h>',
        '#include <sapi.h>',
        '#include <sphelper.h>'
    ]))
else
    have_sapi = false
endif

vlc_modules += {
    'name' : 'sapi',
    'sources' : files('sapi.cpp'),
    'cpp_args' : libcom_cppflags,
    'enabled' : have_sapi,
}

