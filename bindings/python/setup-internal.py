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
    srcdir=os.environ['srcdir']
except KeyError:
    # Note: same as above
    srcdir=None
if not srcdir:
    srcdir = '.'

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
        cflags=os.popen('%s --cflags vlc' % vlcconfig, 'r').readline().rstrip().split()
        return cflags

def get_ldflags():
    vlcconfig=get_vlcconfig()
    if vlcconfig is None:
        return []
    else:
	ldflags = []
	if os.sys.platform == 'darwin':
	    ldflags = "-read_only_relocs warning".split()
        ldflags.extend(os.popen('%s --libs vlc external' % vlcconfig,
				'r').readline().rstrip().split())
	if os.sys.platform == 'darwin':
	    ldflags.append('-lstdc++')
        return ldflags

#source_files = [ 'vlc_module.c', 'vlc_object.c', 'vlc_mediacontrol.c',
#                 'vlc_position.c', 'vlc_instance.c', 'vlc_input.c' ]
source_files = [ 'vlc_internal.c' ]

# To compile in a local vlc tree
vlclocal = Extension('vlcinternal',
                     sources = [ os.path.join( srcdir, f ) for f in source_files ],
                     include_dirs = [ top_builddir,
                                      os.path.join( srcdir, '..', '..', 'include' ),
                                      srcdir,
                                      '/usr/win32/include' ],
                extra_objects = [ ],
                extra_compile_args = get_cflags(),
		extra_link_args = [ '-L' + os.path.join(top_builddir, 'src', '.libs') ]  + get_ldflags(),
                )

setup (name = 'VLC Internal Bindings',
       version = get_vlc_version(),
       #scripts = [ os.path.join( srcdir, 'vlcwrapper.py') ],
       keywords = [ 'vlc', 'video' ],
       license = "GPL", 
       description = """VLC internal bindings for python.

This module provides an Object type, which gives a low-level access to
the vlc objects and their variables.

Example session:

import vlcinternal

# Access lowlevel objets
o=vlcinternal.Object(1)
o.info()
i=o.find_object('input')
i.list()
i.get('time')
       """,
       ext_modules = [ vlclocal ])
