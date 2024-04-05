#
# Audio filter modules
#

include_dir = include_directories('.')

# Audio bar graph a module
vlc_modules += {
    'name' : 'audiobargraph_a',
    'sources' : files('audiobargraph_a.c'),
    'dependencies' : [m_lib]
}

# Chorus flanger module
vlc_modules += {
    'name' : 'chorus_flanger',
    'sources' : files('chorus_flanger.c'),
    'dependencies' : [m_lib]
}

# Compressor module
vlc_modules += {
    'name' : 'compressor',
    'sources' : files('compressor.c'),
    'dependencies' : [m_lib]
}

# Equalizer filter module
vlc_modules += {
    'name' : 'equalizer',
    'sources' : files('equalizer.c'),
    'dependencies' : [m_lib]
}

# Karaoke filter module
vlc_modules += {
    'name' : 'karaoke',
    'sources' : files('karaoke.c')
}

# Volume normalization module
vlc_modules += {
    'name' : 'normvol',
    'sources' : files('normvol.c'),
    'dependencies' : [m_lib]
}

# Gain module
vlc_modules += {
    'name' : 'gain',
    'sources' : files('gain.c')
}

# Parametrical Equalizer module
vlc_modules += {
    'name' : 'param_eq',
    'sources' : files('param_eq.c'),
    'dependencies' : [m_lib]
}

# Scaletempo module
scaletempo_sources = files('scaletempo.c')
scaletempo_deps = [m_lib]

vlc_modules += {
    'name' : 'scaletempo',
    'sources' : scaletempo_sources,
    'dependencies' : scaletempo_deps
}

# Scaletempo pitch module
vlc_modules += {
    'name' : 'scaletempo_pitch',
    'sources' : scaletempo_sources,
    'dependencies' : scaletempo_deps,
    'c_args' : ['-DPITCH_SHIFTER']
}

# Stereo widen module
vlc_modules += {
    'name' : 'stereo_widen',
    'sources' : files('stereo_widen.c')
}

# Spatializer module
vlc_modules += {
    'name' : 'spatializer',
    'sources' : files(
        'spatializer/spatializer.cpp',
        'spatializer/allpass.cpp',
        'spatializer/comb.cpp',
        'spatializer/denormals.c',
        'spatializer/revmodel.cpp'),
    'dependencies' : [m_lib]
}

# Central channel filter
vlc_modules += {
    'name' : 'center',
    'sources' : files('center.c'),
    'dependencies' : [m_lib]
}

# Stereo panning filter
vlc_modules += {
    'name' : 'stereopan',
    'sources' : files('stereo_pan.c'),
    'dependencies' : [m_lib]
}


## Channel mixer

# Dolby surround module
vlc_modules += {
    'name' : 'dolby_surround_decoder',
    'sources' : files('channel_mixer/dolby.c')
}

# Headphone mixer module
vlc_modules += {
    'name' : 'headphone_channel_mixer',
    'sources' : files('channel_mixer/headphone.c'),
    'dependencies' : [m_lib]
}

# Mono mixer module
vlc_modules += {
    'name' : 'mono',
    'sources' : files('channel_mixer/mono.c'),
    'dependencies' : [m_lib]
}

# Channel remap mixer module
vlc_modules += {
    'name' : 'remap',
    'sources' : files('channel_mixer/remap.c')
}

# Trivial channel mixer module
vlc_modules += {
    'name' : 'trivial_channel_mixer',
    'sources' : files('channel_mixer/trivial.c')
}

# Simple channel mixer module
vlc_modules += {
    'name' : 'simple_channel_mixer',
    'sources' : files('channel_mixer/simple.c')
}

# TODO: NEON optimized channel mixer plugin
# simple_channel_mixer_plugin_arm_neon

# Spatial audio (ambisonics/binaural) mixer
spatial_dep = dependency('spatialaudio', required: get_option('spatialaudio'))
if spatial_dep.found()
    vlc_modules += {
        'name' : 'spatialaudio',
        'sources' : files('channel_mixer/spatialaudio.cpp'),
        'dependencies' : [spatial_dep]
    }
endif


## Converters

# Format converter module
vlc_modules += {
    'name' : 'audio_format',
    'sources' : files('converter/format.c'),
    'dependencies' : [m_lib]
}

# SPDIF converter module
vlc_modules += {
    'name' : 'tospdif',
    'sources' : files(
        'converter/tospdif.c',
        '../packetizer/dts_header.c'
    ),
    'include_directories' : include_dir
}


## Resampler

# Ugly resampler module
vlc_modules += {
    'name' : 'ugly_resampler',
    'sources' : files('resampler/ugly.c')
}

# libsamplerate resampler
samplerate_dep = dependency('samplerate', required: get_option('samplerate'))
if samplerate_dep.found()
    vlc_modules += {
        'name' : 'samplerate',
        'sources' : files('resampler/src.c'),
        'dependencies' : [m_lib, samplerate_dep]
    }
endif

# SoX resampler
soxr_dep = dependency('soxr', version: '>= 0.1.2', required: get_option('soxr'))
if soxr_dep.found()
    vlc_modules += {
        'name' : 'soxr',
        'sources' : files('resampler/soxr.c'),
        'dependencies' : [m_lib, soxr_dep]
    }
endif

# EBU R 128
ebur128_dep = dependency('libebur128', version: '>= 1.2.4', required: get_option('ebur128'))
if ebur128_dep.found()
    vlc_modules += {
	'name' : 'ebur128',
	'sources' : files('libebur128.c'),
	'dependencies' : [ebur128_dep]
    }
endif

# Speex resampler
speexdsp_dep = dependency('speexdsp', required: get_option('speexdsp'))
if speexdsp_dep.found()
    vlc_modules += {
        'name' : 'speex_resampler',
        'sources' : files('resampler/speex.c'),
        'dependencies' : [speexdsp_dep]
    }
endif

# Rnnoise denoiser
rnnoise_dep = dependency('rnnoise', required: get_option('rnnoise'))
if rnnoise_dep.found()
    vlc_modules += {
	'name' : 'rnnoise',
	'sources' : files('rnnoise.c'),
	'dependencies' : [m_lib, rnnoise_dep]
    }
endif

