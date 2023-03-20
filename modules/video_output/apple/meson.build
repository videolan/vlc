#
# Video output modules
#
#

darwingl_dep = []
if have_osx
    darwingl_dep = opengl_dep
else
    darwingl_dep = opengles_dep
endif

vlc_modules += {
    'name' : 'cvpx_gl',
    'sources' : files(
        'VLCCVOpenGLProvider.m',
        '../../codec/vt_utils.c'
    ),
    'objc_args' : ['-fobjc-arc'],
    'dependencies' : [foundation_dep, corefoundation_dep, corevideo_dep, darwingl_dep],
}

vlc_modules += {
    'name' : 'samplebufferdisplay',
    'sources' : files(
        'VLCSampleBufferDisplay.m',
        '../../codec/vt_utils.c',
        '../../codec/vt_utils.h'
    ),
    'objc_args' : ['-fobjc-arc'],
    'dependencies' : [avfoundation_dep, avkit_dep, cocoa_dep, coremedia_dep, quartz_dep],
}

if have_ios or have_tvos
    vlc_modules += {
        'name' : 'caeagl',
        'sources' : files('VLCOpenGLES2VideoView.m'),
        'objc_args' : ['-fobjc-arc'],
        'dependencies' : [foundation_dep, uikit_dep, quartz_dep, opengles_dep],
    }

    vlc_modules += {
        'name' : 'uiview',
        'sources' : files('VLCVideoUIView.m'),
        'objc_args' : ['-fobjc-arc'],
        'dependencies' : [foundation_dep, uikit_dep],
    }
endif
