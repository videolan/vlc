from distutils.core import setup, Extension
import os

# Get build variables (buildir, srcdir)
try:
    top_builddir=os.environ['top_builddir']
except KeyError:
    # Note: do not initialize here, so that we get
    # a correct default value if the env. var is
    # defined but empty
    top_builddir=None
if not top_builddir:
    top_builddir = os.path.join( '..', '..' )
    os.environ['top_builddir'] = top_builddir

try:
    top_srcdir=os.environ['top_srcdir']
except KeyError:
    # Note: same as above
    srcdir=None
if not top_srcdir:
    top_srcdir = os.path.join( '..', '..' )
    os.environ['top_srcdir'] = top_srcdir

vlclib= "-L"  + os.path.join( top_builddir, 'src') + " -lvlc"
picflag=''
srcdir=os.path.join( top_srcdir, 'bindings', 'mediacontrol-python' )

def get_vlcconfig():
    vlcconfig=None
    for n in ( 'vlc-config',
               os.path.join( top_builddir, 'vlc-config' )):
        if os.path.exists(n):
            vlcconfig=n
            break
    if vlcconfig is None:
        print "*** Warning *** Cannot find vlc-config"
    elif os.sys.platform == 'win32':
        # Win32 does not know how to invoke the shell itself.
        vlcconfig="sh %s" % vlcconfig
    return vlcconfig

def get_vlc_version():
    vlcconfig=get_vlcconfig()
    if vlcconfig is None:
        return ""
    else:
        version=os.popen('%s --version' % vlcconfig, 'r').readline().strip()
        return version
    
def get_cflags():
    vlcconfig=get_vlcconfig()
    if vlcconfig is None:
        return []
    else:
        cflags=os.popen('%s --cflags' % vlcconfig, 'r').readline().rstrip().split()
        return cflags

def get_ldflags():
    vlcconfig=get_vlcconfig()
    if vlcconfig is None:
        return []
    else:
	ldflags = []
	if os.sys.platform == 'darwin':
	    ldflags = "-read_only_relocs warning".split()
        ldflags.extend(os.popen('%s --libs vlc %s' % (vlcconfig,
							      picflag), 
				'r').readline().rstrip().split())
	if os.sys.platform == 'darwin':
	    ldflags.append('-lstdc++')
        return ldflags

#source_files = [ 'vlc_module.c', 'vlc_object.c', 'vlc_mediacontrol.c',
#                 'vlc_position.c', 'vlc_instance.c', 'vlc_input.c' ]
source_files = [ 'vlc_module.c' ]

# To compile in a local vlc tree
vlclocal = Extension('vlc',
                     sources = [ os.path.join( srcdir, f ) for f in source_files ],
                     include_dirs = [ top_builddir,
                                      os.path.join( top_srcdir, 'include' ),
                                      srcdir,
                                      '/usr/win32/include' ],
                extra_objects = [ vlclib ],
                extra_compile_args = get_cflags(),
		extra_link_args = [ '-L' + top_builddir ]  + get_ldflags(),
                )

setup (name = 'VLC Bindings',
       version = get_vlc_version(),
       #scripts = [ os.path.join( srcdir, 'vlcwrapper.py') ],
       keywords = [ 'vlc', 'video' ],
       license = "GPL", 
       description = """VLC bindings for python.

This module provides bindings for the native libvlc API of the VLC
video player. Documentation can be found on the VLC wiki : 
http://wiki.videolan.org/index.php/ExternalAPI

The module also provides a Object type, which gives a low-level access
to the vlc objects and their variables.

This module also provides a MediaControl object, which implements an
API inspired from the OMG Audio/Video Stream 1.0 specification.
Documentation can be found on the VLC wiki :
http://wiki.videolan.org/index.php/PythonBinding

Example session:

import vlc
mc=vlc.MediaControl(['--verbose', '1'])
mc.playlist_add_item('movie.mpg')

# Start the movie at 2000ms
p=vlc.Position()
p.origin=vlc.RelativePosition
p.key=vlc.MediaTime
p.value=2000
mc.start(p)
# which could be abbreviated as
# mc.start(2000)
# for the default conversion from int is to make a RelativePosition in MediaTime

# Display some text during 2000ms
mc.display_text('Some useless information', 0, 2000)

# Pause the video
mc.pause(0)

# Get status information
mc.get_stream_information()

# Access lowlevel objets
o=vlc.Object(1)
o.info()
i=o.find_object('input')
i.list()
i.get('time')

       """,
       ext_modules = [ vlclocal ])
