from distutils.core import setup, Extension

FFMPEG_DIR = '/home/cyril/ffmpeg'

vlc = Extension('vlc',
                sources = ['vlcmodule.c'],
                libraries = ['vlc', 'rt', 'dl' , 'pthread', 'ffmpeg', 'm', 
                             'avcodec'],
                library_dirs = ['../lib', '../modules/codec/ffmpeg', 
                                FFMPEG_DIR + '/libavcodec'])


setup (name = 'PackageName',
       version = '1.0',
       description = 'This is a demo package',
       ext_modules = [vlc])

