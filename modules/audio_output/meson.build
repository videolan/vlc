# Audio output modules

subdir('android')

# Dummy audio output
vlc_modules += {
    'name' : 'adummy',
    'sources' : files('adummy.c'),
    'plugin_dir' : 'audio_output',
}

# File audio output
vlc_modules += {
    'name' : 'afile',
    'sources' : files('file.c'),
    'plugin_dir' : 'audio_output',
}

# Memory audio output
vlc_modules += {
    'name' : 'amem',
    'sources' : files('amem.c'),
    'plugin_dir' : 'audio_output',
}

# PipeWire output
vlc_modules += {
    'name' : 'aout_pipewire',
    'sources' : files('pipewire.c'),
    'dependencies' : [m_lib, pipewire_dep],
    'link_with' : [libvlc_pipewire],
    'plugin_dir' : 'audio_output',
    'enabled' : pipewire_dep.found(),
}

# Pulseaudio output
if pulse_dep.found()
    vlc_modules += {
        'name' : 'pulse',
        'sources' : files('pulse.c'),
        'dependencies' : [m_lib, pulse_dep],
        'link_with' : [libvlc_pulse],
        'plugin_dir' : 'audio_output',
    }
endif

# ALSA audio output
if alsa_dep.found()
    vlc_modules += {
        'name' : 'alsa',
        'sources' : files('alsa.c'),
        'dependencies' : [m_lib, alsa_dep],
        'plugin_dir' : 'audio_output',
    }
endif

# JACK audio output
if jack_dep.found()
    vlc_modules += {
        'name' : 'jack',
        'sources' : files('jack.c'),
        'dependencies' : [jack_dep],
        'plugin_dir' : 'audio_output',
    }
endif

# sndio output
sndio_dep = dependency('sndio', version: '>= 1.8.0', required: get_option('sndio'))

vlc_modules += {
    'name' : 'sndio',
    'sources' : files('sndio.c'),
    'dependencies' : [sndio_dep],
    'plugin_dir' : 'audio_output',
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
        'plugin_dir' : 'audio_output',
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
        'plugin_dir' : 'audio_output',
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
        'objc_args' : ['-fobjc-arc'],
        'plugin_dir' : 'audio_output',
    }
endif

if host_system == 'windows'
vlc_modules += {
    'name' : 'mmdevice',
    'sources' : files('mmdevice.c'),
    'plugin_dir' : 'audio_output',
    'enabled' : have_win_desktop,
}

vlc_modules += {
    'name' : 'winstore',
    'sources' : files('winstore.c'),
    'plugin_dir' : 'audio_output',
    'enabled' : have_win_store,
}

vlc_modules += {
    'name' : 'wasapi',
    'sources' : files('wasapi.c'),
    'dependencies' : [ksuser_lib],
    'plugin_dir' : 'audio_output',
    'enabled' : get_option('wasapi')
      .require(host_system == 'windows', error_message: 'WASAPI requires Windows')
      .allowed() and ksuser_lib.found()
}

vlc_modules += {
    'name' : 'waveout',
    'sources' : files('waveout.c'),
    'dependencies' : [cc.find_library('winmm')],
    'plugin_dir' : 'audio_output',
    'enabled' : have_win_desktop,
}
endif

if host_system == 'os/2'
    if get_option('kai').allowed()
        kai_lib = cc.find_library('kai',
                                  has_headers: ['kai.h'],
                                  required: get_option('kai').enabled())
        vlc_modules += {
            'name' : 'kai',
            'sources' : files('kai.c'),
            'dependencies' : [kai_lib],
            'plugin_dir' : 'audio_output',
            'enabled' : kai_lib.found(),
        }
    endif
endif
