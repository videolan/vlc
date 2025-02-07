# Audio output modules

# Dummy audio output
vlc_modules += {
    'name' : 'adummy',
    'sources' : files('adummy.c')
}

# File audio output
vlc_modules += {
    'name' : 'afile',
    'sources' : files('file.c')
}

# Memory audio output
vlc_modules += {
    'name' : 'amem',
    'sources' : files('amem.c')
}

# Pulseaudio output
if pulse_dep.found()
    vlc_modules += {
        'name' : 'pulse',
        'sources' : files('pulse.c'),
        'dependencies' : [m_lib, pulse_dep],
        'link_with' : [libvlc_pulse],
    }
endif

# ALSA audio output
if alsa_dep.found()
    vlc_modules += {
        'name' : 'alsa',
        'sources' : files('alsa.c'),
        'dependencies' : [m_lib, alsa_dep]
    }
endif

# JACK audio output
if jack_dep.found()
    vlc_modules += {
        'name' : 'jack',
        'sources' : files('jack.c'),
        'dependencies' : [jack_dep]
    }
endif

# sndio output
sndio_dep = dependency('sndio', version: '>= 1.8.0', required: get_option('sndio'))

vlc_modules += {
    'name' : 'sndio',
    'sources' : files('sndio.c'),
    'dependencies' : [sndio_dep],
    'enabled' : sndio_dep.found(),
}

# AUHAL output module (macOS only)
if have_osx
    vlc_modules += {
        'name' : 'auhal',
        'sources' : files('apple/auhal.c', 'apple/coreaudio_common.c',
                          'apple/channel_layout.c'),
        'dependencies' : [
            frameworks['CoreFoundation'],
            frameworks['AudioUnit'],
            frameworks['AudioToolbox'],
            frameworks['CoreAudio'],
        ],
    }
endif

# AudioUnit output (iOS/tvOS)
if have_ios or have_tvos
    vlc_modules += {
        'name' : 'audiounit_ios',
        'sources' : files('apple/audiounit_ios.m', 'apple/coreaudio_common.c',
                          'apple/channel_layout.c', 'apple/avaudiosession_common.m'),
        'dependencies' : [
            frameworks['Foundation'],
            frameworks['AVFoundation'],
            frameworks['AudioToolbox'],
            frameworks['CoreAudio'],
        ],
    }
endif

if have_osx or have_ios or have_tvos
    avsamplebuffer_sources = files(
        'apple/avsamplebuffer.m',
        'apple/channel_layout.c',
    )
    if have_ios or have_tvos
        avsamplebuffer_sources += files(
            'apple/avaudiosession_common.m',
        )
    endif

    vlc_modules += {
        'name' : 'avsamplebuffer',
        'sources' : avsamplebuffer_sources,
        'dependencies' : [
            frameworks['CoreMedia'],
            frameworks['Foundation'],
            frameworks['AVFoundation'],
        ],
        'objc_args' : ['-fobjc-arc']
    }
endif

if host_system == 'windows'
vlc_modules += {
    'name' : 'mmdevice',
    'sources' : files('mmdevice.c'),
    'enabled' : have_win_desktop,
}

vlc_modules += {
    'name' : 'winstore',
    'sources' : files('winstore.c'),
    'enabled' : have_win_store,
}

vlc_modules += {
    'name' : 'wasapi',
    'sources' : files('wasapi.c'),
    'dependencies' : [ksuser_lib],
    'enabled' : get_option('wasapi')
      .require(host_system == 'windows', error_message: 'WASAPI requires Windows')
      .allowed() and ksuser_lib.found()
}

vlc_modules += {
    'name' : 'waveout',
    'sources' : files('waveout.c'),
    'dependencies' : [cc.find_library('winmm')],
    'enabled' : have_win_desktop,
}
endif
