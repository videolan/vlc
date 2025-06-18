# Video filters

# Edge-detection filter
vlc_modules += {
    'name' : 'edgedetection',
    'sources' : files('edgedetection.c'),
    'dependencies' : [m_lib]
}

# Adjust (saturation/hue) filter
vlc_modules += {
    'name' : 'adjust',
    'sources' : files(
        'adjust.c',
        'adjust_sat_hue.c',
        'adjust_sat_hue.h'
    ),
    'dependencies' : [m_lib]
}

# Alpha-mask filter
vlc_modules += {
    'name' : 'alphamask',
    'sources' : files('alphamask.c')
}

# Anaglyph filter
vlc_modules += {
    'name' : 'anaglyph',
    'sources' : files('anaglyph.c')
}

# Anti-flicker filter
vlc_modules += {
    'name' : 'antiflicker',
    'sources' : files('antiflicker.c')
}

opengl_filter_dep = disabler()
opengl_filter_cargs = []
if opengl_dep.found() or opengles2_dep.found()
    if host_system in ['darwin', 'android'] and opengles2_dep.found()
        opengl_filter_dep = opengles2_dep
        opengl_filter_cargs += '-DUSE_OPENGL_ES2'
    elif opengl_dep.found()
        opengl_filter_dep = opengl_dep
    endif
endif

vlc_modules += {
    'name' : 'opengl_filter',
    'sources' : files('opengl.c'),
    'dependencies' : [gl_common_dep, opengl_filter_dep],
    'c_args' : opengl_filter_cargs,
    'enabled' : opengl_filter_dep.found()
}

vlc_modules += {
    'name' : 'opengl_win_offscreen',
    'sources' : files('opengl_win_offscreen.c'),
    'dependencies' : [ gl_common_dep, opengl_dep ],
    'enabled' : opengl_dep.found() and host_system == 'windows'
}

if host_system == 'windows'
    vlc_modules += {
        'name' : 'amf_frc',
        'sources' : files(
                'amf_frc.c',
                '../hw/amf/amf_helper.c',
            ),
        'link_with': [ d3d11_common_lib ],
        'dependencies': [ contrib_dep, cc.find_library('d3d11')],
        'enabled' : get_option('amf_frc').allowed() \
                    and cc.find_library('d3d11').found() \
                    and cc.check_header('AMF/core/PropertyStorage.h', args: [contrib_inc_args]) \
                    and cc.check_header('AMF/components/FRC.h', args: [contrib_inc_args]),
    }

    vlc_modules += {
        'name' : 'amf_vqenhancer',
        'sources' : files(
                'amf_vqenhancer.c',
                '../hw/amf/amf_helper.c',
            ),
        'link_with': [ d3d11_common_lib ],
        'dependencies': [ contrib_dep, cc.find_library('d3d11') ],
        'enabled' : get_option('amf_vqenhancer').allowed() \
                    and cc.find_library('d3d11').found() \
                    and cc.check_header('AMF/core/PropertyStorage.h', args: [contrib_inc_args]) \
                    and cc.check_header('AMF/components/VQEnhancer.h', args: [contrib_inc_args]),
    }
endif

# Ball filter
vlc_modules += {
    'name' : 'ball',
    'sources' : files('ball.c'),
    'dependencies' : [m_lib]
}

vlc_modules += {
    'name' : 'blendbench',
    'sources' : files('blendbench.c')
}

vlc_modules += {
    'name' : 'bluescreen',
    'sources' : files('bluescreen.c')
}

vlc_modules += {
    'name' : 'canvas',
    'sources' : files('canvas.c')
}

vlc_modules += {
    'name' : 'colorthres',
    'sources' : files('colorthres.c'),
    'dependencies' : [m_lib]
}

# Crop filter
vlc_modules += {
    'name' : 'croppadd',
    'sources' : files('croppadd.c')
}

vlc_modules += {
    'name' : 'erase',
    'sources' : files('erase.c')
}

vlc_modules += {
    'name' : 'extract',
    'sources' : files('extract.c'),
    'dependencies' : [m_lib]
}

vlc_modules += {
    'name' : 'fps',
    'sources' : files('fps.c')
}

vlc_modules += {
    'name' : 'freeze',
    'sources' : files('freeze.c')
}

# Gaussian blur filter
vlc_modules += {
    'name' : 'gaussianblur',
    'sources' : files('gaussianblur.c'),
    'dependencies' : [m_lib]
}

vlc_modules += {
    'name' : 'gradfun',
    'sources' : files('gradfun.c', 'gradfun.h'),
    'dependencies' : [m_lib]
}

vlc_modules += {
    'name' : 'gradient',
    'sources' : files('gradient.c'),
    'dependencies' : [m_lib]
}

vlc_modules += {
    'name' : 'grain',
    'sources' : files('grain.c'),
    'dependencies' : [m_lib]
}

vlc_modules += {
    'name' : 'hqdn3d',
    'sources' : files('hqdn3d.c', 'hqdn3d.h'),
    'dependencies' : [m_lib]
}

vlc_modules += {
    'name' : 'invert',
    'sources' : files('invert.c')
}

vlc_modules += {
    'name' : 'magnify',
    'sources' : files('magnify.c')
}

vlc_modules += {
    'name' : 'formatcrop',
    'sources' : files('formatcrop.c')
}

vlc_modules += {
    'name' : 'mirror',
    'sources' : files('mirror.c')
}

# Motion blur filter
vlc_modules += {
    'name' : 'motionblur',
    'sources' : files('motionblur.c')
}

# Motion detection filter
vlc_modules += {
    'name' : 'motiondetect',
    'sources' : files('motiondetect.c')
}

# Old movie filter
vlc_modules += {
    'name' : 'oldmovie',
    'sources' : files('oldmovie.c'),
    'dependencies' : [m_lib]
}

vlc_modules += {
    'name' : 'posterize',
    'sources' : files('posterize.c')
}

vlc_modules += {
    'name' : 'psychedelic',
    'sources' : files('psychedelic.c'),
    'dependencies' : [m_lib]
}

# Puzzle filter
vlc_modules += {
    'name' : 'puzzle',
    'sources' : files(
        'puzzle/puzzle.c',
        'puzzle/puzzle.h',
        'puzzle/puzzle_bezier.c',
        'puzzle/puzzle_bezier.h',
        'puzzle/puzzle_lib.c',
        'puzzle/puzzle_lib.h',
        'puzzle/puzzle_mgt.c',
        'puzzle/puzzle_mgt.h',
        'puzzle/puzzle_pce.c',
        'puzzle/puzzle_pce.h',
    ),
    'dependencies' : [m_lib]
}

vlc_modules += {
    'name' : 'ripple',
    'sources' : files('ripple.c'),
    'dependencies' : [m_lib]
}

# Rotate filter
vlc_modules += {
    'name' : 'rotate',
    'sources' : files('rotate.c'),
    'dependencies' : [m_lib],
    'link_with' : [vlc_motion_lib]
}

vlc_modules += {
    'name' : 'scale',
    'sources' : files('scale.c')
}

vlc_modules += {
    'name' : 'scene',
    'sources' : files('scene.c'),
    'dependencies' : [m_lib]
}

# Sepia filter
vlc_modules += {
    'name' : 'sepia',
    'sources' : files('sepia.c')
}

# Sharpen filter
vlc_modules += {
    'name' : 'sharpen',
    'sources' : files('sharpen.c')
}

vlc_modules += {
    'name' : 'transform',
    'sources' : files('transform.c')
}

# VHS-effect filter
vlc_modules += {
    'name' : 'vhs',
    'sources' : files('vhs.c')
}

vlc_modules += {
    'name' : 'wave',
    'sources' : files('wave.c'),
    'dependencies' : [m_lib]
}

# macOS/iOS accelerated video filters (CoreImage)
vlc_modules += {
    'name' : 'ci_filters',
    'sources' : files('ci_filters.m', '../codec/vt_utils.c'),
    'dependencies' : [
        frameworks['Foundation'],
        frameworks['CoreGraphics'],
        frameworks['CoreImage'],
        frameworks['CoreVideo'],
        (have_osx) ? dependency('gl', required: true) : frameworks['OpenGLES'],
    ],
    'include_directories' : [include_directories('../codec')],
    'enabled' : host_system == 'darwin',
}

# deinterlace filter
deinterlace_sources = files(
        'deinterlace/deinterlace.c',
        'deinterlace/merge.c',
        'deinterlace/helpers.c',
        'deinterlace/algo_basic.c',
        'deinterlace/algo_x.c',
        'deinterlace/algo_yadif.c',
        'deinterlace/algo_phosphor.c',
        'deinterlace/algo_ivtc.c',
    )
if cdata.has('HAVE_X86ASM')
    deinterlace_sources+=files('deinterlace/yadif_x86.asm')
endif
vlc_modules += {
    'name' : 'deinterlace',
    'sources' : deinterlace_sources,
    'include_directories' : include_directories('../../extras/include/x86'),
    'link_with' : [deinterlacecommon_lib]
}

# Postproc filter
postproc_dep = dependency('libpostproc', required: get_option('postproc'))
postproc_c_args = []
if postproc_dep.found()
    if cc.check_header('postproc/postprocess.h', dependencies: [postproc_dep])
        postproc_c_args += ['-DHAVE_POSTPROC_POSTPROCESS_H']
    endif
    vlc_modules += {
    'name' : 'postproc',
    'sources' : files('postproc.c'),
    'dependencies' : [m_lib, postproc_dep],
    'c_args' : postproc_c_args,
    }
endif

vlc_modules += {
    'name' : 'blend',
    'sources' : files('blend.cpp')
}

vlc_modules += {
    'name' : 'dither',
    'sources' : files('dither.c'),
    'dependencies' : [m_lib]
}