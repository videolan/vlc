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

import ctypes
import sys

build_date="This will be replaced by the build date"

if sys.platform == 'linux2':
    dll=ctypes.CDLL('libvlc.so')
elif sys.platform == 'win32':
    import ctypes.util
    import os
    plugin_path=None
    path=ctypes.util.find_library('libvlc.dll')
    if path is None:
        # Try a standard location.
        p='c:\\Program Files\\VideoLAN\\VLC\\libvlc.dll'
        if os.path.exists(p):
            plugin_path=os.path.dirname(p)
            os.chdir(plugin_path)
        # If chdir failed, this will not work and raise an exception
        path='libvlc.dll'
    else:
        plugin_path=os.path.dirname(path)
    dll=ctypes.CDLL(path)
elif sys.platform == 'darwin':
    # FIXME: should find a means to configure path
    dll=ctypes.CDLL('/Applications/VLC.app/Contents/MacOS/lib/libvlc.2.dylib')

class ListPOINTER(object):
    '''Just like a POINTER but accept a list of ctype as an argument.
    '''
    def __init__(self, etype):
        self.etype = etype

    def from_param(self, param):
        if isinstance(param, (list,tuple)):
            return (self.etype * len(param))(*param)

# From libvlc_structures.h
class VLCException(ctypes.Structure):
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

class LogMessage(ctypes.Structure):
    _fields_= [
                ('size', ctypes.c_uint),
                ('severity', ctypes.c_int),
                ('type', ctypes.c_char_p),
                ('name', ctypes.c_char_p),
                ('header', ctypes.c_char_p),
                ('message', ctypes.c_char_p),
                ]

    def __str__(self):
        print "vlc.LogMessage(%d:%s): %s" % (self.severity, self.type, self.message)

class MediaControlPosition(ctypes.Structure):
    _fields_= [
                ('origin', ctypes.c_ushort),
                ('key', ctypes.c_ushort),
                ('value', ctypes.c_longlong),
                ]

    @staticmethod
    def from_param(arg):
        if isinstance(arg, (int, long)):
            p=MediaControlPosition()
            p.value=arg
            p.key=2
            return p
        else:
            return arg

class MediaControlPositionOrigin(ctypes.c_uint):
    enum=(
        'AbsolutePosition',
        'RelativePosition',
        'ModuloPosition',
        )
    def __repr__(self):
        return self.enum[self.value]

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
                ('code', ctypes.c_int),
                ('message', ctypes.c_char_p),
                ]

class RGBPicture(ctypes.Structure):
    _fields_= [
                ('width', ctypes.c_int),
                ('height', ctypes.c_int),
                ('type', ctypes.c_uint32),
                ('date', ctypes.c_longlong),
                ('size', ctypes.c_int),
                ('data', ctypes.c_char_p),
                ]

    def free(self):
        mediacontrol_RGBPicture__free(self)

def check_vlc_exception(result, func, args):
    """Error checking method for functions using an exception in/out parameter.
    """
    ex=args[-1]
    # Take into account both VLCException and MediacontrolException
    c=getattr(ex, 'raised', getattr(ex, 'code', 0))
    if c:
        raise Exception(args[-1].message)
    return result

### End of header.py ###
