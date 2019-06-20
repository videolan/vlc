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

# AUHAL output module (macOS only)
if have_osx
    vlc_modules += {
        'name' : 'auhal',
        'sources' : files('auhal.c', 'coreaudio_common.c'),
        'dependencies' : [
            corefoundation_dep,
            audiounit_dep,
            audiotoolbox_dep,
            coreaudio_dep,
        ]
    }
endif

# AudioUnit output (iOS/tvOS)
if have_ios or have_tvos
    vlc_modules += {
        'name' : 'audiounit_ios',
        'sources' : files('audiounit_ios.m', 'coreaudio_common.c'),
        'dependencies' : [
            foundation_dep,
            avfoundation_dep,
            audiotoolbox_dep,
            coreaudio_dep,
        ]
    }
endif
