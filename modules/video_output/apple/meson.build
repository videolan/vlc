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
    'dependencies' : [
        frameworks['Foundation'],
        frameworks['CoreFoundation'],
        frameworks['CoreVideo'],
        darwingl_dep,
    ],
}

if have_osx
    uifwk_dep = frameworks['Cocoa']
else
    uifwk_dep = frameworks['UIKit']
endif

vlc_modules += {
    'name' : 'samplebufferdisplay',
    'sources' : files(
        'VLCSampleBufferDisplay.m',
        '../../codec/vt_utils.c',
        '../../codec/vt_utils.h'
    ),
    'objc_args' : ['-fobjc-arc'],
    'dependencies' : [
        frameworks['AVFoundation'],
        frameworks['Foundation'],
        frameworks['AVKit'],
        uifwk_dep,
        frameworks['CoreMedia'],
        frameworks['QuartzCore'],
        frameworks['CoreVideo'],
        frameworks['CoreGraphics'],
        frameworks['VideoToolbox'],
    ],
}

if have_ios or have_tvos
    vlc_modules += {
        'name' : 'caeagl',
        'sources' : files('VLCOpenGLES2VideoView.m'),
        'objc_args' : ['-fobjc-arc'],
        'dependencies' : [
            frameworks['Foundation'],
            frameworks['UIKit'],
            frameworks['QuartzCore'],
            frameworks['OpenGLES']
        ],
    }

    vlc_modules += {
        'name' : 'uiview',
        'sources' : files('VLCVideoUIView.m'),
        'objc_args' : ['-fobjc-arc'],
        'dependencies' : [
            frameworks['Foundation'],
            frameworks['UIKit'],
        ],
    }
endif
