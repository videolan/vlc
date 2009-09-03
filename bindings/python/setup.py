from distutils.core import setup, Extension
import os
import commands

# Get build variables (buildir, srcdir)
top_builddir = os.path.join( '..', '..' )
os.environ['top_builddir'] = top_builddir

# Determine the extra link args. Normally, vlc-config should take care
# of this and return the right path values, from a development tree or
# an installed version.
libtool=False
linkargs=[]
d=os.path.join(top_builddir, 'src', '.libs')
if os.path.exists(d):
    # We are in a development tree, which was compiled with libtool
    libtool=True
    linkargs=[ '-L' + d ]
else:
    d=os.path.join(top_builddir, 'src')
    # We are in a development tree, which was compiled without libtool
    if os.path.exists(d):
        linkargs=[ '-L' + d ]

# For out-of-tree compilations
srcdir = '.'

def get_vlcconfig():
    vlcconfig=None
    for n in ( 'vlc-config',
               os.path.join( top_builddir, 'vlc-config' )):
        if os.path.exists(n):
            vlcconfig=n
            break
    status, output = commands.getstatusoutput('pkg-config libvlc --exists')
    if status == 0:
        vlcconfig="pkg-config libvlc"
    if vlcconfig is None:
        print "*** Warning *** Cannot find vlc-config. Will try sane defaults."
    elif os.sys.platform == 'win32':
        # Win32 does not know how to invoke the shell itself.
        vlcconfig="sh %s" % vlcconfig
    return vlcconfig

def get_vlc_version():
    vlcconfig=get_vlcconfig()
    if vlcconfig is None:
        return ""
    else:
        version=os.popen('%s --modversion' % vlcconfig, 'r').readline().strip()
        return version

def get_cflags():
    vlcconfig=get_vlcconfig()
    if vlcconfig is None:
        return []
    else:
        cflags=os.popen('%s --cflags ' % vlcconfig, 'r').readline().strip()
        return cflags

def get_ldflags():
    vlcconfig=get_vlcconfig()
    if vlcconfig is None:
        return [ '-lvlc' ]
    else:
	ldflags = []
	if os.sys.platform == 'darwin':
	    ldflags = "-read_only_relocs warning".split()
        ldflags.extend(os.popen('%s --libs ' % vlcconfig,
                                'r').readline().rstrip().split())
	if os.sys.platform == 'darwin':
	    ldflags.append('-lstdc++')
        if not '-lvlc' in ldflags:
            # Some broken vlc-config can exist (esp. on win32). Try to
            # workaround the problem.
            ldflags.append('-lvlc')
        return ldflags

#source_files = [ 'vlc_module.c', 'vlc_mediacontrol.c',
#                 'vlc_position.c', 'vlc_instance.c', 'vlc_input.c' ]
source_files = [ 'vlc_module.c' ]

# To compile in a local vlc tree
vlclocal = Extension('vlc',
                     sources = [ os.path.join( srcdir, f ) for f in source_files ],
                     include_dirs = [ top_builddir,
                                      srcdir ],
                     extra_objects = [ ],
                     extra_compile_args = get_cflags(),
                     extra_link_args = linkargs + get_ldflags(),
                     )

setup (name = 'python-vlc',
       version = '1.0.0.90',
       author='Olivier Aubert',
       author_email='olivier.aubert@liris.cnrs.fr',
       url='http://wiki.videolan.org/PythonBinding',
       py_modules=['vlcwidget'],
       keywords = [ 'vlc', 'video' ],
       license = "GPL",
       description = "VLC bindings for python.",
       long_description = """VLC bindings for python.

This module provides bindings for the native libvlc API of the VLC
video player. Documentation can be found on the VLC wiki :
http://wiki.videolan.org/ExternalAPI

This module also provides a MediaControl object, which implements an
API inspired from the OMG Audio/Video Stream 1.0 specification.
Documentation can be found on the VLC wiki :
http://wiki.videolan.org/PythonBinding

Example session (for the MediaControl API):

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
       """,
       ext_modules = [ vlclocal ])
