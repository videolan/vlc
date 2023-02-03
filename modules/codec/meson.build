#
# Encoder and decoder modules
#

# Dummy codec
vlc_modules += {
    'name' : 'ddummy',
    'sources' : files('ddummy.c')
}

# Dummy codec
vlc_modules += {
    'name' : 'edummy',
    'sources' : files('edummy.c')
}

## Audio codecs

# SPDIF codec
vlc_modules += {
    'name' : 'spdif',
    'sources' : files('spdif.c')
}

# a52 codec
a52_lib = cc.find_library('a52', required: get_option('a52'))
if a52_lib.found()
    vlc_modules += {
        'name' : 'a52',
        'sources' : files('a52.c'),
        'dependencies' : [a52_lib]
    }
endif

# DTS Coherent Acoustics decoder
dca_dep = dependency('libdca', version: '>= 0.0.5', required: get_option('dca'))
if dca_dep.found()
    vlc_modules += {
        'name' : 'dca',
        'sources' : files('dca.c'),
        'dependencies' : [dca_dep, m_lib]
    }
endif

# adpcm codec
vlc_modules += {
    'name' : 'adpcm',
    'sources' : files('adpcm.c')
}

# AES3 codec
vlc_modules += {
    'name' : 'aes3',
    'sources' : files('aes3.c')
}

# Raw PCM demux module
vlc_modules += {
    'name' : 'araw',
    'sources' : files('araw.c'),
}

# faad decoder plugin
faad_lib = cc.find_library('faad', required: get_option('faad'))
if faad_lib.found()
    vlc_modules += {
        'name' : 'faad',
        'sources' : files('faad.c'),
        'dependencies' : [faad_lib, m_lib]
    }
endif

# g711 codec
vlc_modules += {
    'name' : 'g711',
    'sources' : files('g711.c')
}

# libfluidsynth (MIDI synthetizer) plugin
fluidsynth_option = get_option('fluidsynth')
fluidsynth_module_flags = []
if not fluidsynth_option.disabled()
    fluidsynth_module_flags += f'-DDATADIR="@sysdatadir_path@"'
    fluidsynth_dep = dependency('fluidsynth', version: '>= 1.1.2', required: false)
    if not fluidsynth_dep.found()
        fluidsynth_dep = dependency('fluidlite', required: false)
        if fluidsynth_dep.found()
            fluidsynth_module_flags += '-DHAVE_FLUIDLITE_H'
        endif
    endif

    if fluidsynth_option.enabled() and not fluidsynth_dep.found()
        error('Neither fluidsynth or fluidlite were found.')
    endif
else
    fluidsynth_dep = dependency('', required: false)
endif

if fluidsynth_dep.found()
    vlc_modules += {
        'name' : 'fluidsynth',
        'sources' : files('fluidsynth.c'),
        'dependencies' : [fluidsynth_dep, m_lib],
        'c_args' : fluidsynth_module_flags
    }
endif

# Audiotoolbox MIDI plugin (Darwin only)
if host_system == 'darwin'
    vlc_modules += {
        'name' : 'audiotoolboxmidi',
        'sources' : files('audiotoolbox_midi.c'),
        'dependencies' : [corefoundation_dep, audiounit_dep, audiotoolbox_dep]
    }
endif

# LPCM codec
vlc_modules += {
    'name' : 'lpcm',
    'sources' : files('lpcm.c')
}

# libmad codec
mad_dep = dependency('mad', required: get_option('mad'))
if mad_dep.found()
  vlc_modules += {
      'name' : 'mad',
      'sources' : files('mad.c'),
      'dependencies' : [mad_dep]
  }
endif

# libmpg123 decoder
mpg123_dep = dependency('libmpg123', required: get_option('mpg123'))
if mpg123_dep.found()
    vlc_modules += {
        'name' : 'mpg123',
        'sources' : files('mpg123.c'),
        'dependencies' : [mpg123_dep]
    }
endif

# Ulead DV Audio codec
vlc_modules += {
    'name' : 'uleaddvaudio',
    'sources' : files('uleaddvaudio.c')
}


## Video codecs

# CDG codec
vlc_modules += {
    'name' : 'cdg',
    'sources' : files('cdg.c')
}

# libmpeg2 decoder
mpeg2_dep = dependency('libmpeg2', version: '> 0.3.2', required: get_option('mpeg2'))
if mpeg2_dep.found()
    vlc_modules += {
        'name' : 'libmpeg2',
        'sources' : files('libmpeg2.c', 'synchro.c'),
        'dependencies' : [mpeg2_dep]
    }
endif

# Raw video codec
vlc_modules += {
    'name' : 'rawvideo',
    'sources' : files('rawvideo.c')
}

# RTP video codec
vlc_modules += {
    'name' : 'rtpvideo',
    'sources' : files('rtpvideo.c')
}

# Dirac decoder and encoder using schroedinger
schroedinger_dep = dependency('schroedinger-1.0',
                              version: '>= 1.0.10',
                              required: get_option('schroedinger'))
if schroedinger_dep.found()
    vlc_modules += {
        'name' : 'schroedinger',
        'sources' : files('schroedinger.c'),
        'dependencies' : [schroedinger_dep]
    }
endif

# RTP Raw video codec
vlc_modules += {
    'name' : 'rtp_rawvid',
    'sources' : files('rtp-rawvid.c')
}

## Image codecs

# PNG codec
png_dep = dependency('libpng', required: get_option('png'))
if png_dep.found()
  vlc_modules += {
      'name' : 'png',
      'sources' : files('png.c'),
      'dependencies' : [png_dep, m_lib]
  }
endif

# JPEG codec
jpeg_dep = dependency('libjpeg', required: get_option('jpeg'))
if jpeg_dep.found()
  vlc_modules += {
      'name' : 'jpeg',
      'sources' : files('jpeg.c'),
      'dependencies' : [jpeg_dep]
  }
endif

# BPG codec
bpg_lib = cc.find_library('bpg', required: get_option('bpg'))
if bpg_lib.found()
  vlc_modules += {
      'name' : 'bpg',
      'sources' : files('bpg.c'),
      'dependencies' : [bpg_lib]
  }
endif

# SVG image decoder
cairo_dep = dependency('cairo', version: '>= 1.13.1', required: get_option('cairo'))
if rsvg_dep.found() and cairo_dep.found()
    vlc_modules += {
        'name' : 'svgdec',
        'sources' : files('svg.c'),
        'dependencies' : [rsvg_dep, cairo_dep]
    }
endif

# xwd
if (get_option('xcb')
    .disable_auto_if(host_system in ['darwin', 'windows'])
    .allowed())
  xproto_dep = dependency('xproto', required: get_option('xcb'))
  if xproto_dep.found()
    vlc_modules += {
        'name' : 'xwd',
        'sources' : files('xwd.c'),
        'dependencies' : [xproto_dep]
    }
  endif
endif


## SPU codecs

# Substation Alpha subtitle decoder (libass)
# TODO: Needs -ldwrite for UWP builds
libass_dep = dependency('libass',
                        version: '>= 0.9.8',
                        required: get_option('libass'))
libass_deps = [libass_dep]

if host_system == 'darwin'
    libass_deps += [corefoundation_dep, coregraphics_dep, coretext_dep]
endif

if libass_dep.found()
    if freetype_dep.found()
        libass_deps += freetype_dep
    endif

    vlc_modules += {
        'name' : 'libass',
        'sources' : files('libass.c'),
        'dependencies' : libass_deps
    }
endif

# Closed captioning decoder
vlc_modules += {
    'name' : 'cc',
    'sources' : files('cc.c', 'cea708.c')
}

# cvdsub decoder
vlc_modules += {
    'name' : 'cvdsub',
    'sources' : files('cvdsub.c')
}

# dvbsub decoder
vlc_modules += {
    'name' : 'dvbsub',
    'sources' : files('dvbsub.c')
}

# aribsub
aribb24_dep = dependency('aribb24', version: '>= 1.0.1', required: get_option('aribsub'))
if aribb24_dep.found()
  vlc_modules += {
      'name' : 'aribsub',
      'sources' : files('arib/aribsub.c'),
      'dependencies' : [aribb24_dep],
      'cpp_args' : ['-DHAVE_ARIBB24']
  }
endif

# scte18 decoder
vlc_modules += {
    'name' : 'scte18',
    'sources' : files('scte18.c', 'atsc_a65.c')
}

# scte27 decoder
vlc_modules += {
    'name' : 'scte27',
    'sources' : files('scte27.c')
}

# SPU decoder
vlc_modules += {
    'name' : 'spudec',
    'sources' : files('spudec/spudec.c', 'spudec/parse.c')
}

# STL codec
vlc_modules += {
    'name' : 'stl',
    'sources' : files('stl.c')
}

# subsdec
vlc_modules += {
    'name' : 'subsdec',
    'sources' : files('subsdec.c')
}

# subsusf
vlc_modules += {
    'name' : 'subsusf',
    'sources' : files('subsusf.c')
}

# TTML decoder
vlc_modules += {
    'name' : 'ttml',
    'sources' : files('ttml/substtml.c', 'ttml/ttml.c', '../demux/ttml.c'),
    'include_directories' : include_directories('.')
}

# svcdsub
vlc_modules += {
    'name' : 'svcdsub',
    'sources' : files('svcdsub.c')
}

# t140
vlc_modules += {
    'name' : 't140',
    'sources' : files('t140.c')
}

# zvbi
use_zvbi = zvbi_dep.found() and get_option('zvbi').enabled()

if use_zvbi
  vlc_modules += {
      'name' : 'zvbi',
      'sources' : files('zvbi.c'),
      'dependencies' : [zvbi_dep, socket_libs]
  }
endif

# telx
if not use_zvbi and get_option('telx').enabled()
  vlc_modules += {
      'name' : 'telx',
      'sources' : files('telx.c')
  }
endif

# textst
vlc_modules += {
    'name' : 'textst',
    'sources' : files('textst.c')
}

# substx3g
vlc_modules += {
    'name' : 'substx3g',
    'sources' : files('substx3g.c')
}

# WebVTT plugin
webvtt_sources = files(
    'webvtt/webvtt.c',
    'webvtt/subsvtt.c',
    '../demux/webvtt.c',
)

if get_option('stream_outputs')
    webvtt_sources += files('webvtt/encvtt.c')
endif

# WebVTT CSS engine requires Flex and Bison
flex = find_program('flex', required: get_option('css_engine'))
bison = find_program('bison', required: get_option('css_engine'))

if flex.found() and bison.found() and not get_option('css_engine').disabled()

    bison_gen = generator(bison,
        output: ['@BASENAME@.c', '@BASENAME@.h'],
        arguments: ['@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@'])

    webvtt_bison_files = bison_gen.process('webvtt/CSSGrammar.y')

    flex_gen = generator(flex,
        output: '@PLAINNAME@.yy.c',
        arguments: ['-o', '@OUTPUT@', '@INPUT@'])

    webvtt_lex_files = flex_gen.process('webvtt/CSSLexer.l')

    webvtt_sources += [
        files(
            'webvtt/css_parser.c',
            'webvtt/css_style.c',
        ),
        webvtt_lex_files,
        webvtt_bison_files,
    ]

    # CSS parser test
    css_parser_test = executable('css_parser_test',
        files(
            'webvtt/css_test.c',
            'webvtt/css_parser.c'
        ),
        webvtt_lex_files, webvtt_bison_files,
        link_with: [vlc_libcompat],
        dependencies: [libvlccore_dep],
        include_directories: [
            vlc_include_dirs,
            include_directories('webvtt')])

    test('css_parser', css_parser_test, suite: 'css_parser')

endif

vlc_modules += {
    'name' : 'webvtt',
    'sources' : webvtt_sources,
    'include_directories' : include_directories('webvtt')
}


## Xiph.org codecs

ogg_dep = dependency('ogg', required: get_option('ogg'))

# FLAC codec
flac_dep = dependency('flac', required: get_option('flac'))
if flac_dep.found()
    vlc_modules += {
        'name' : 'flac',
        'sources' : files('flac.c'),
        'dependencies' : [flac_dep]
    }
endif

# Kate codec
kate_dep = dependency('kate', version: '>= 0.3.0', required: get_option('kate'))
if kate_dep.found()
  kate_deps = [kate_dep, m_lib]
  kate_cpp_args = []

  tiger_dep = dependency('tiger', version: '>= 0.3.1', required: get_option('tiger'))

  if tiger_dep.found()
    kate_cpp_args += ['-DHAVE_TIGER=1']
    kate_deps += tiger_dep
  endif

  vlc_modules += {
      'name' : 'kate',
      'sources' : files('kate.c'),
      'dependencies' : kate_deps,
      'cpp_args' : kate_cpp_args
  }
endif

# Opus codec
opus_dep = dependency('opus', version: '>= 1.0.3', required: get_option('opus'))
if opus_dep.found() and ogg_dep.found()
    vlc_modules += {
        'name' : 'opus',
        'sources' : files('opus.c', 'opus_header.c'),
        'dependencies' : [opus_dep, ogg_dep]
    }
endif

# Theora codec
theoraenc_dep = dependency('theoraenc', required: get_option('theoraenc'))
theoradec_dep = dependency('theoradec', version: '>= 1.0', required: get_option('theoradec'))
if theoraenc_dep.found() and theoradec_dep.found() and ogg_dep.found()
    vlc_modules += {
        'name' : 'theora',
        'sources' : files('theora.c'),
        'dependencies' : [theoraenc_dep, theoradec_dep, ogg_dep]
    }
endif

# Daala codec plugin
daaladec_dep = dependency('daaladec', required: get_option('daaladec'))
daalaenc_dep = dependency('daalaenc', required: get_option('daalaenc'))
if daaladec_dep.found() and daalaenc_dep.found()
    vlc_modules += {
        'name' : 'daala',
        'sources' : files('daala.c'),
        'dependencies' : [daaladec_dep, daalaenc_dep]
    }
endif

# Vorbis codec
vorbis_dep    = dependency('vorbis', version: '>= 1.1', required: get_option('vorbis'))
vorbisenc_dep = dependency('vorbisenc', version: '>= 1.1', required: get_option('vorbis'))

if vorbis_dep.found() and vorbisenc_dep.found() and ogg_dep.found()
    vlc_modules += {
        'name' : 'vorbis',
        'sources' : files('vorbis.c'),
        'dependencies' : [vorbis_dep, vorbisenc_dep, ogg_dep]
    }
endif

# OggSpots decoder
if ogg_dep.found()
    vlc_modules += {
        'name' : 'oggspots',
        'sources' : files('oggspots.c'),
        'dependencies' : [ogg_dep]
    }
endif

# VideoToolbox (Apples hardware-accelerated video decoder/encoder)
# TODO: Set proper flags for minimum iOS/tvOS versions
if host_system == 'darwin'
    vlc_modules += {
        'name' : 'videotoolbox',
        'sources' : files(
            'videotoolbox.c',
            'vt_utils.c',
            'hxxx_helper.c',
            '../packetizer/hxxx_nal.c',
            '../packetizer/hxxx_sei.c',
            '../packetizer/h264_slice.c',
            '../packetizer/h264_nal.c',
            '../packetizer/hevc_nal.c'
        ),
        'dependencies' : [videotoolbox_dep, foundation_dep, coremedia_dep, corevideo_dep],
    }
endif

# FFmpeg common helper library
if avcodec_dep.found()
    libavcodec_common = static_library('avcodec_common',
        files('avcodec/fourcc.c', 'avcodec/chroma.c'),
        dependencies: [avutil_dep, avcodec_dep],
        install: false,
        include_directories: [
            vlc_include_dirs,
            include_directories('avcodec')
        ],
    )
endif

# FFmpeg codec module
if avcodec_dep.found()
    avcodec_extra_sources = []
    if get_option('stream_outputs')
        avcodec_extra_sources += 'avcodec/encoder.c'
    endif

    # TODO: Implement the merge-ffmpeg option

    vlc_modules += {
        'name' : 'avcodec',
        'sources' : files(
                'avcodec/video.c',
                'avcodec/subtitle.c',
                'avcodec/audio.c',
                'avcodec/va.c',
                'avcodec/avcodec.c',
                avcodec_extra_sources
        ),
        'dependencies' : [avutil_dep, avcodec_dep],
        'link_with' : [libavcodec_common],
        'link_args' : symbolic_linkargs
    }
endif

if get_option('libva').enabled() and get_option('avcodec').disabled()
    error('-Dlibva=enabled and -Davcodec=disabled options are mutually exclusive. Use -Davcodec=disabled')
endif

libva_dep = dependency('libva', version: '>= 1.0', required: get_option('libva'))
if get_option('libva').require(
    avcodec_dep.found() and avutil_dep.found(),
    error_message: 'VAAPI requires avcodec').allowed() and libva_dep.found()

    vlc_modules += {
        'name' : 'vaapi',
        'sources' : files(
                'avcodec/vaapi.c',
                '../hw/vaapi/vlc_vaapi.c',
                'avcodec/va_surface.c',
            ),
        'dependencies' : [avcodec_dep, libva_dep, avutil_dep]
    }

endif

if host_system == 'windows'
    d3d11_common_lib = static_library('d3d11_common',
        files(
            '../video_chroma/d3d11_fmt.c',
            '../video_chroma/dxgi_fmt.c',
            ),
        include_directories: [vlc_include_dirs],
        pic: true,
        install: false,
        dependencies: [
            cc.find_library('dxguid'),
            cc.find_library('wbemuuid'),
        ]
    )
    d3d9_common_lib = static_library('d3d9_common',
        files(
            '../video_chroma/d3d9_fmt.c',
            '../video_chroma/dxgi_fmt.c',
            ),
        include_directories: [vlc_include_dirs],
        pic: true,
        install: false
    )
endif

# OpenMAX IL codec
if get_option('omxil')
    vlc_modules += {
        'name' : 'omxil',
        'sources' : files(
                'omxil/utils.c',
                'omxil/qcom.c',
                'omxil/omxil.c',
                'omxil/omxil_core.c',
                '../packetizer/h264_nal.c',
                '../packetizer/hevc_nal.c',
            ),
        'link_with' : [chroma_copy_lib, dl_lib]
    }
endif

## x26X encoders

# x265 encoder
x265_dep = dependency('x265', required: get_option('x265'))
if x265_dep.found()
    vlc_modules += {
        'name' : 'x265',
        'sources' : files('x265.c'),
        'dependencies' : [x265_dep]
    }
endif

# x262 encoder
x262_dep = dependency('x262', required: get_option('x262'))
if x262_dep.found()
    vlc_modules += {
        'name' : 'x262',
        'sources' : files('x264.c'),
        'dependencies' : [x262_dep],
        'c_args' : ['-DPLUGIN_X262'],
        'link_args' : symbolic_linkargs
    }
endif

# x264 10-bit encoder (requires x264 >= 0.153)
x26410b_dep = dependency('x264', version: '>= 0.153', required: get_option('x264'))
if x26410b_dep.found()
    vlc_modules += {
        'name' : 'x26410b',
        'sources' : files('x264.c'),
        'dependencies' : [x26410b_dep],
        'c_args' : ['-DPLUGIN_X264_10B'],
        'link_args' : symbolic_linkargs
    }
endif

# x264 encoder
x264_dep = dependency('x264', version: '>= 0.148', required: get_option('x264'))
if x264_dep.found()
    vlc_modules += {
        'name' : 'x264',
        'sources' : files('x264.c'),
        'dependencies' : [x264_dep],
        'c_args' : ['-DPLUGIN_X264'],
        'link_args' : symbolic_linkargs
    }
endif


## Misc codecs

# fdkaac encoder
fdkaac_dep = dependency('fdk-aac', required: get_option('fdk-aac'))
if fdkaac_dep.found()
    vlc_modules += {
        'name' : 'fdkaac',
        'sources' : files('fdkaac.c'),
        'dependencies' : [fdkaac_dep]
    }
endif

# Shine MPEG Audio Layer 3 encoder
shine_dep = dependency('shine', version: '>= 3.0.0', required: get_option('shine'))
if shine_dep.found()
    vlc_modules += {
        'name' : 'shine',
        'sources' : files('shine.c'),
        'dependencies' : [shine_dep]
    }
endif

# VP8/VP9 encoder/decoder (libvpx)
vpx_dep = dependency('vpx', version: '>= 1.5.0', required: get_option('vpx'))
if vpx_dep.found()
  vpx_cpp_args = []

  if cc.has_function('vpx_codec_vp8_dx', dependencies: [vpx_dep])
    vpx_cpp_args += '-DENABLE_VP8_DECODER'
  endif
  if cc.has_function('vpx_codec_vp9_dx', dependencies: [vpx_dep])
    vpx_cpp_args += '-DENABLE_VP9_DECODER'
  endif
  if cc.has_function('vpx_codec_vp8_cx', dependencies: [vpx_dep])
    vpx_cpp_args += '-DENABLE_VP8_ENCODER'
  endif
  if cc.has_function('vpx_codec_vp9_cx', dependencies: [vpx_dep])
    vpx_cpp_args += '-DENABLE_VP9_ENCODER'
  endif

  vlc_modules += {
      'name' : 'vpx',
      'sources' : files('vpx.c'),
      'cpp_args' : vpx_cpp_args,
      'dependencies' : [vpx_dep]
  }
endif

# libaom AV1 codec
aom_dep = dependency('aom', required: get_option('aom'))
if aom_dep.found()
    vlc_modules += {
        'name' : 'aom',
        'sources' : files('aom.c'),
        'dependencies' : [aom_dep]
    }
endif

rav1e_dep = dependency('rav1e', required: get_option('rav1e'))
if rav1e_dep.found()
    vlc_modules += {
        'name' : 'rav1e',
        'sources' : files('rav1e.c'),
        'dependencies' : [rav1e_dep]
    }
endif

# Twolame MPEG Audio Layer 2 encoder
twolame_dep = dependency('twolame', required: get_option('twolame'))
if twolame_dep.found()
    vlc_modules += {
        'name' : 'twolame',
        'sources' : files('twolame.c'),
        'dependencies' : [twolame_dep],
        'c_args' : ['-DLIBTWOLAME_STATIC']
    }
endif

# dav1d AV1 decoder
dav1d_dep = dependency('dav1d', version: '>= 0.5.0', required: get_option('dav1d'))
if dav1d_dep.found()
    vlc_modules += {
        'name' : 'dav1d',
        'sources' : files(
                'dav1d.c',
                '../packetizer/av1_obu.c'
            ),
        'dependencies' : [dav1d_dep]
    }
endif


## Hardware encoders

# QSV encoder
qsv_dep = dependency('libmfx', required: get_option('mfx'))
if qsv_dep.found()
    vlc_modules += {
        'name' : 'qsv',
        'sources' : files('qsv.c'),
        'dependencies' : [qsv_dep]
    }
endif

## External frameworks
