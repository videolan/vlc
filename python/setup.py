from distutils.core import setup, Extension

FFMPEG_DIR = '/home/cyril/ffmpeg'

vlc = Extension('vlc',
                sources = ['vlcmodule.c'],
                libraries = ['vlc', 'rt', 'dl' , 'pthread', 'ffmpeg', 'm',
                             'memcpymmx','stream_out_transcode',
                             'i420_rgb_mmx','i420_yuy2_mmx','i420_ymga_mmx',
                             'i422_yuy2_mmx','memcpymmxext','memcpy3dn',
                             'encoder_ffmpeg','avcodec'],
                library_dirs = [ '../lib',
                                '../modules/stream_out', '../modules/encoder/ffmpeg',
                                '../modules/misc/memcpy','../modules/video_chroma',
                                '../modules/codec/ffmpeg', FFMPEG_DIR + '/libavcodec'])


setup (name = 'PackageName',
       version = '1.0',
       description = 'This is a demo package',
       ext_modules = [vlc])

