#! /usr/bin/python

#
# Python ctypes bindings for VLC
# Copyright (C) 2009 the VideoLAN team
# $Id: $
#
# Authors: Olivier Aubert <olivier.aubert at liris.cnrs.fr>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
#

"""This module provides bindings for the
U{libvlc<http://wiki.videolan.org/ExternalAPI>} and
U{MediaControl<http://wiki.videolan.org/MediaControlAPI>} APIs.

You can find documentation at U{http://www.advene.org/download/python-ctypes/}.

Basically, the most important class is L{Instance}, which is used to
create a libvlc Instance. From this instance, you can then create
L{MediaPlayer} and L{MediaListPlayer} instances.
"""

import logging
import ctypes
import sys

build_date="This will be replaced by the build date"

# Used for win32 and MacOS X
detected_plugin_path=None

if sys.platform == 'linux2':
    dll=ctypes.CDLL('libvlc.so')
elif sys.platform == 'win32':
    import ctypes.util
    import os
    detected_plugin_path=None
    path=ctypes.util.find_library('libvlc.dll')
    if path is None:
        # Try to use registry settings
        import _winreg
        detected_plugin_path_found = None
        subkey, name = 'Software\\VideoLAN\\VLC','InstallDir'
        for hkey in _winreg.HKEY_LOCAL_MACHINE, _winreg.HKEY_CURRENT_USER:
            try:
                reg = _winreg.OpenKey(hkey, subkey)
                detected_plugin_path_found, type_id = _winreg.QueryValueEx(reg, name)
                _winreg.CloseKey(reg)
                break
            except _winreg.error:
                pass
        if detected_plugin_path_found:
            detected_plugin_path = detected_plugin_path_found
        else:
            # Try a standard location.
            p='c:\\Program Files\\VideoLAN\\VLC\\libvlc.dll'
            if os.path.exists(p):
                detected_plugin_path=os.path.dirname(p)
        os.chdir(detected_plugin_path)
        # If chdir failed, this will not work and raise an exception
        path='libvlc.dll'
    else:
        detected_plugin_path=os.path.dirname(path)
    dll=ctypes.CDLL(path)
elif sys.platform == 'darwin':
    # FIXME: should find a means to configure path
    d='/Applications/VLC.app'
    import os
    if os.path.exists(d):
        dll=ctypes.CDLL(d+'/Contents/MacOS/lib/libvlc.2.dylib')
        detected_plugin_path=d+'/Contents/MacOS/modules'
    else:
        # Hope some default path is set...
        dll=ctypes.CDLL('libvlc.2.dylib')

#
# Generated enum types.
#

# GENERATED_ENUMS

#
# End of generated enum types.
#

class ListPOINTER(object):
    '''Just like a POINTER but accept a list of ctype as an argument.
    '''
    def __init__(self, etype):
        self.etype = etype

    def from_param(self, param):
        if isinstance(param, (list, tuple)):
            return (self.etype * len(param))(*param)

class LibVLCException(Exception):
    """Python exception raised by libvlc methods.
    """
    pass

# From libvlc_structures.h

# This is version-dependent, depending on the presence of libvlc_errmsg

if hasattr(dll, 'libvlc_errmsg'):
    # New-style message passing
    class VLCException(ctypes.Structure):
        """libvlc exception.
        """
        _fields_= [
                    ('raised', ctypes.c_int),
                    ]

        @property
        def message(self):
            return dll.libvlc_errmsg()

        def init(self):
            libvlc_exception_init(self)

        def clear(self):
            libvlc_exception_clear(self)
else:
    # Old-style exceptions
    class VLCException(ctypes.Structure):
        """libvlc exception.
        """
        _fields_= [
                    ('raised', ctypes.c_int),
                    ('code', ctypes.c_int),
                    ('message', ctypes.c_char_p),
                    ]
        def init(self):
            libvlc_exception_init(self)

        def clear(self):
            libvlc_exception_clear(self)

class PlaylistItem(ctypes.Structure):
    _fields_= [
                ('id', ctypes.c_int),
                ('uri', ctypes.c_char_p),
                ('name', ctypes.c_char_p),
                ]

    def __str__(self):
        return "PlaylistItem #%d %s (%uri)" % (self.id, self.name, self.uri)

class LogMessage(ctypes.Structure):
    _fields_= [
                ('size', ctypes.c_uint),
                ('severity', ctypes.c_int),
                ('type', ctypes.c_char_p),
                ('name', ctypes.c_char_p),
                ('header', ctypes.c_char_p),
                ('message', ctypes.c_char_p),
                ]

    def __init__(self):
        super(LogMessage, self).__init__()
        self.size=ctypes.sizeof(self)

    def __str__(self):
        return "vlc.LogMessage(%d:%s): %s" % (self.severity, self.type, self.message)

class MediaControlPosition(ctypes.Structure):
    _fields_= [
                ('origin', PositionOrigin),
                ('key', PositionKey),
                ('value', ctypes.c_longlong),
                ]

    def __init__(self, value=0, origin=None, key=None):
        # We override the __init__ method so that instanciating the
        # class with an int as parameter will create the appropriate
        # default position (absolute position, media time, with the
        # int as value).
        super(MediaControlPosition, self).__init__()
        self.value=value
        if origin is None:
            origin=PositionOrigin.AbsolutePosition
        if key is None:
            key=PositionKey.MediaTime
        self.origin=origin
        self.key=key

    def __str__(self):
        return "MediaControlPosition %ld (%s, %s)" % (
            self.value,
            str(self.origin),
            str(self.key)
            )

    @staticmethod
    def from_param(arg):
        if isinstance(arg, (int, long)):
            return MediaControlPosition(arg)
        else:
            return arg

class MediaControlException(ctypes.Structure):
    _fields_= [
                ('code', ctypes.c_int),
                ('message', ctypes.c_char_p),
                ]
    def init(self):
        mediacontrol_exception_init(self)

    def clear(self):
        mediacontrol_exception_free(self)

class MediaControlStreamInformation(ctypes.Structure):
    _fields_= [
                ('status', PlayerStatus),
                ('url', ctypes.c_char_p),
                ('position', ctypes.c_longlong),
                ('length', ctypes.c_longlong),
                ]

    def __str__(self):
        return "%s (%s) : %ld / %ld" % (self.url or "<No defined URL>",
                                        str(self.status),
                                        self.position,
                                        self.length)

class RGBPicture(ctypes.Structure):
    _fields_= [
                ('width', ctypes.c_int),
                ('height', ctypes.c_int),
                ('type', ctypes.c_uint32),
                ('date', ctypes.c_ulonglong),
                ('size', ctypes.c_int),
                ('data_pointer', ctypes.c_void_p),
                ]

    @property
    def data(self):
        return ctypes.string_at(self.data_pointer, self.size)

    def __str__(self):
        return "RGBPicture (%d, %d) - %ld ms - %d bytes" % (self.width, self.height, self.date, self.size)

    def free(self):
        mediacontrol_RGBPicture__free(self)

def check_vlc_exception(result, func, args):
    """Error checking method for functions using an exception in/out parameter.
    """
    ex=args[-1]
    if not isinstance(ex, (VLCException, MediaControlException)):
        logging.warn("python-vlc: error when processing function %s. Please report this as a bug to vlc-devel@videolan.org" % str(func))
        return result
    # Take into account both VLCException and MediacontrolException:
    c=getattr(ex, 'raised', getattr(ex, 'code', 0))
    if c:
        raise LibVLCException(ex.message)
    return result

### End of header.py ###
