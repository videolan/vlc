from distutils.core import setup,Extension
import os

def get_vlcconfig():
    vlcconfig=None
    for n in ( 'vlc-config',
               os.path.sep.join( ( '..', 'vlc-config' ))):
        if os.path.exists(n):
            vlcconfig=n
            break
    if vlcconfig is None:
        print "*** Warning *** Cannot find vlc-config"
    elif os.sys.platform == 'win32':
        # Win32 does not know how to invoke the shell itself.
        vlcconfig="sh %s" % vlcconfig
    return vlcconfig

def get_cflags():
    vlcconfig=get_vlcconfig()
    if vlcconfig is None:
        return []
    else:
        cflags=os.popen('%s --cflags' % vlcconfig, 'r').readline().rstrip().split()
	cflags.append( "-D__VLC__")
        return cflags

def get_ldflags():
    vlcconfig=get_vlcconfig()
    if vlcconfig is None:
        return []
    else:
	os.environ['top_builddir'] = '..'
	ldflags = []
	if os.sys.platform == 'darwin':
	    ldflags = "-read_only_relocs warning".split()
        ldflags.extend(os.popen('%s --libs vlc builtin' % vlcconfig, 'r').readline().rstrip().split())
	if os.sys.platform == 'darwin':
	    ldflags.append('-lstdc++')
        return ldflags

# To compile in a local vlc tree
native_libvlc_test = Extension( 'native_libvlc_test',
                sources = ['native/init.c'],
                include_dirs = ['../include', '../', '/usr/win32/include' ],
                extra_objects = [ '../src/.libs/libvlc.so' ],
                extra_compile_args = get_cflags(),
       		    extra_link_args = [ '-L../..' ]  + get_ldflags(),
                )

setup( name = 'native_libvlc_test' ,version = '1242', ext_modules = [ native_libvlc_test ] )
