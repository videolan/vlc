from distutils.core import setup, Extension

FFMPEG_DIR = '/home/cyril/ffmpeg'

vlc = Extension('vlc',
                sources = ['vlcmodule.c'],
                libraries = ['vlc', 'rt', 'dl' , 'pthread', 'ffmpeg', 'm',
                             'avcodec','mpeg_video','idct','idctclassic',
                             'motion','memcpymmx','idctmmx','motionmmx',
                             'i420_rgb_mmx','i420_yuy2_mmx','i420_ymga_mmx',
                             'i422_yuy2_mmx','memcpymmxext','idctmmxext',
                             'motionmmxext','memcpy3dn'],
                library_dirs = ['/usr/local/lib/vlc', '../lib', 
'../modules/codec/ffmpeg',
                                FFMPEG_DIR + '/libavcodec'])


setup (name = 'PackageName',
       version = '1.0',
       description = 'This is a demo package',
       ext_modules = [vlc])

