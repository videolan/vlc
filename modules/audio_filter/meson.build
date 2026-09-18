#
# Audio filter modules
#

include_dir = include_directories('.')

# Audio bar graph a module
vlc_modules += {
    'name' : 'audiobargraph_a',
    'sources' : files('audiobargraph_a.c'),
    'dependencies' : [m_lib],
    'shortname' : 'abg_a',
    'plugin_dir' : 'audio_filter',
}

# Chorus flanger module
vlc_modules += {
    'name' : 'chorus_flanger',
    'sources' : files('chorus_flanger.c'),
    'dependencies' : [m_lib],
    'plugin_dir' : 'audio_filter',
}

# Shared dynamics core
libdynamics = static_library('dynamics',
    files('dynamics.c'),
    dependencies: [m_lib],
    include_directories: [vlc_include_dirs],
    install: false,
)

# Compressor module
vlc_modules += {
    'name' : 'compressor',
    'sources' : files('compressor.c'),
    'dependencies' : [m_lib],
    'link_with' : [libdynamics],
    'plugin_dir' : 'audio_filter',
}

# Expander module
vlc_modules += {
    'name' : 'expander',
    'sources' : files('expander.c'),
    'dependencies' : [m_lib],
    'link_with' : [libdynamics],
    'plugin_dir' : 'audio_filter',
}

# Limiter module
vlc_modules += {
    'name' : 'limiter',
    'sources' : files('limiter.c'),
    'dependencies' : [m_lib],
    'link_with' : [libdynamics],
    'plugin_dir' : 'audio_filter',
}

# Equalizer filter module
vlc_modules += {
    'name' : 'equalizer',
    'sources' : files('equalizer.c'),
    'dependencies' : [m_lib],
    'plugin_dir' : 'audio_filter',
}

# Gate filter module
vlc_modules += {
    'name' : 'gate',
    'sources' : files('gate.c'),
    'dependencies' : [m_lib],
    'plugin_dir' : 'audio_filter',
}

# Karaoke filter module
vlc_modules += {
    'name' : 'karaoke',
    'sources' : files('karaoke.c'),
    'plugin_dir' : 'audio_filter',
}

# Volume normalization module
vlc_modules += {
    'name' : 'normvol',
    'sources' : files('normvol.c'),
    'dependencies' : [m_lib],
    'plugin_dir' : 'audio_filter',
}

# Gain module
vlc_modules += {
    'name' : 'gain',
    'sources' : files('gain.c'),
    'plugin_dir' : 'audio_filter',
}

# Parametrical Equalizer module
vlc_modules += {
    'name' : 'param_eq',
    'sources' : files('param_eq.c'),
    'dependencies' : [m_lib],
    'plugin_dir' : 'audio_filter',
}

# Scaletempo module
scaletempo_sources = files('scaletempo.c')
scaletempo_deps = [m_lib]

vlc_modules += {
    'name' : 'scaletempo',
    'sources' : scaletempo_sources,
    'dependencies' : scaletempo_deps,
    'plugin_dir' : 'audio_filter',
}

# Scaletempo pitch module
vlc_modules += {
    'name' : 'scaletempo_pitch',
    'sources' : scaletempo_sources,
    'dependencies' : scaletempo_deps,
    'c_args' : ['-DPITCH_SHIFTER'],
    'shortname' : 'scalet_p',
    'plugin_dir' : 'audio_filter',
}

# Stereo widen module
vlc_modules += {
    'name' : 'stereo_widen',
    'sources' : files('stereo_widen.c'),
    'plugin_dir' : 'audio_filter',
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
    'dependencies' : [m_lib],
    'plugin_dir' : 'audio_filter',
}

# Central channel filter
vlc_modules += {
    'name' : 'center',
    'sources' : files('center.c'),
    'dependencies' : [m_lib],
    'plugin_dir' : 'audio_filter',
}

# Stereo panning filter
vlc_modules += {
    'name' : 'stereopan',
    'sources' : files('stereo_pan.c'),
    'dependencies' : [m_lib],
    'plugin_dir' : 'audio_filter',
}


## Channel mixer

# Dolby surround module
vlc_modules += {
    'name' : 'dolby_surround_decoder',
    'sources' : files('channel_mixer/dolby.c'),
    'plugin_dir' : 'audio_filter',
}

# Headphone mixer module
vlc_modules += {
    'name' : 'headphone_channel_mixer',
    'sources' : files('channel_mixer/headphone.c'),
    'dependencies' : [m_lib],
    'plugin_dir' : 'audio_filter',
}

# Mono mixer module
vlc_modules += {
    'name' : 'mono',
    'sources' : files('channel_mixer/mono.c'),
    'dependencies' : [m_lib],
    'plugin_dir' : 'audio_filter',
}

# Channel remap mixer module
vlc_modules += {
    'name' : 'remap',
    'sources' : files('channel_mixer/remap.c'),
    'plugin_dir' : 'audio_filter',
}

# Trivial channel mixer module
vlc_modules += {
    'name' : 'trivial_channel_mixer',
    'sources' : files('channel_mixer/trivial.c'),
    'plugin_dir' : 'audio_filter',
}

# Simple channel mixer module
vlc_modules += {
    'name' : 'simple_channel_mixer',
    'sources' : files('channel_mixer/simple.c'),
    'plugin_dir' : 'audio_filter',
}

# TODO: NEON optimized channel mixer plugin
# simple_channel_mixer_plugin_arm_neon

# Spatial audio (ambisonics/binaural) mixer
spatial_dep = dependency('spatialaudio', required: get_option('spatialaudio'))
spatial_extra_cppflags = []
if spatial_dep.found()
    if (spatial_dep.version().version_compare('>= 0.4.0'))
        spatial_extra_cppflags += ['-DHAVE_SPATIALAUDIOVERSION_H=1']
    endif

    if host_system == 'windows'
        # HRTFS
        install_subdir('../../share/hrtfs', install_dir : prefix_path )
    endif
endif

vlc_modules += {
    'name' : 'spatialaudio',
    'sources' : files('channel_mixer/spatialaudio.cpp'),
    'dependencies' : [spatial_dep],
    'cpp_args' : spatial_extra_cppflags,
    'plugin_dir' : 'audio_filter',
    'enabled' : spatial_dep.found(),
}


## Converters

# Format converter module
vlc_modules += {
    'name' : 'audio_format',
    'sources' : files('converter/format.c'),
    'dependencies' : [m_lib],
    'plugin_dir' : 'audio_filter',
}

# SPDIF converter module
vlc_modules += {
    'name' : 'tospdif',
    'sources' : files(
        'converter/tospdif.c',
        '../packetizer/dts_header.c'
    ),
    'include_directories' : include_dir,
    'plugin_dir' : 'audio_filter',
}


## Resampler

# Ugly resampler module
vlc_modules += {
    'name' : 'ugly_resampler',
    'sources' : files('resampler/ugly.c'),
    'plugin_dir' : 'audio_filter',
}

# libsamplerate resampler
samplerate_dep = dependency('samplerate', required: get_option('samplerate'))
if samplerate_dep.found()
    vlc_modules += {
        'name' : 'samplerate',
        'sources' : files('resampler/src.c'),
        'dependencies' : [m_lib, samplerate_dep],
        'plugin_dir' : 'audio_filter',
    }
endif

# SoX resampler
soxr_dep = dependency('soxr', version: '>= 0.1.2', required: get_option('soxr'))
if soxr_dep.found()
    vlc_modules += {
        'name' : 'soxr',
        'sources' : files('resampler/soxr.c'),
        'dependencies' : [m_lib, soxr_dep],
        'plugin_dir' : 'audio_filter',
    }
endif

# EBU R 128
ebur128_dep = dependency('libebur128', version: '>= 1.2.4', required: get_option('ebur128'))
if ebur128_dep.found()
    vlc_modules += {
	'name' : 'ebur128',
	'sources' : files('libebur128.c'),
	'dependencies' : [ebur128_dep],
    'plugin_dir' : 'audio_filter',
    }
endif

# Speex resampler
speexdsp_dep = dependency('speexdsp', required: get_option('speexdsp'))
if speexdsp_dep.found()
    vlc_modules += {
        'name' : 'speex_resampler',
        'sources' : files('resampler/speex.c'),
        'dependencies' : [speexdsp_dep],
        'plugin_dir' : 'audio_filter',
    }
endif

# Rnnoise denoiser
rnnoise_dep = dependency('rnnoise', required: get_option('rnnoise'))
if rnnoise_dep.found()
    vlc_modules += {
	'name' : 'rnnoise',
	'sources' : files('rnnoise.c'),
	'dependencies' : [m_lib, rnnoise_dep],
    'plugin_dir' : 'audio_filter',
    }
endif
