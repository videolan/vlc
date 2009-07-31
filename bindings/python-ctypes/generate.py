#! /usr/bin/python
debug=False

#
# Code generator for python ctypes bindings for VLC
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

"""This module parses VLC public API include files and generates
corresponding python/ctypes code. Moreover, it generates class
wrappers for most methods.
"""

import sys
import re
import time
import operator
import itertools

# DefaultDict from ASPN python cookbook
import copy
class DefaultDict(dict):
    """Dictionary with a default value for unknown keys."""
    def __init__(self, default=None, **items):
        dict.__init__(self, **items)
        self.default = default

    def __getitem__(self, key):
        if key in self:
            return self.get(key)
        else:
            ## Need copy in case self.default is something like []
            return self.setdefault(key, copy.deepcopy(self.default))

    def __copy__(self):
        return DefaultDict(self.default, **self)

# Methods not decorated/not referenced
blacklist=[
    "libvlc_exception_raise",
    "libvlc_exception_raised",
    "libvlc_exception_get_message",
    "libvlc_get_vlc_instance",

    "libvlc_media_add_option_flag",
    "libvlc_media_list_view_index_of_item",
    "libvlc_media_list_view_insert_at_index",
    "libvlc_media_list_view_remove_at_index",
    "libvlc_media_list_view_add_item",

    # In svn but not in current 1.0.0
    'libvlc_video_set_deinterlace',
    'libvlc_video_get_marquee_option_as_int',
    'libvlc_video_get_marquee_option_as_string',
    'libvlc_video_set_marquee_option_as_int',
    'libvlc_video_set_marquee_option_as_string',
    'libvlc_vlm_get_event_manager',

    'mediacontrol_PlaylistSeq__free',

    # TODO
    "libvlc_event_detach",
    "libvlc_event_attach",
    ]

# Precompiled regexps
api_re=re.compile('VLC_PUBLIC_API\s+(\S+\s+.+?)\s*\(\s*(.+?)\s*\)')
param_re=re.compile('\s*(const\s*|unsigned\s*|struct\s*)?(\S+\s*\**)\s+(.+)')
paramlist_re=re.compile('\s*,\s*')
comment_re=re.compile('\\param\s+(\S+)')
python_param_re=re.compile('(@param\s+\S+)(.+)')
forward_re=re.compile('.+\(\s*(.+?)\s*\)(\s*\S+)')
enum_re=re.compile('typedef\s+(enum)\s*(\S+\s*)?\{\s*(.+)\s*\}\s*(\S+);')

# Definition of parameter passing mode for types.  This should not be
# hardcoded this way, but works alright ATM.
parameter_passing=DefaultDict(default=1)
parameter_passing['libvlc_exception_t*']=3

# C-type to ctypes/python type conversion.
# Note that enum types conversions are generated (cf convert_enum_names)
typ2class={
    'libvlc_exception_t*': 'ctypes.POINTER(VLCException)',

    'libvlc_media_player_t*': 'MediaPlayer',
    'libvlc_instance_t*': 'Instance',
    'libvlc_media_t*': 'Media',
    'libvlc_log_t*': 'Log',
    'libvlc_log_iterator_t*': 'LogIterator',
    'libvlc_log_message_t*': 'LogMessage',
    'libvlc_event_type_t': 'EventType',
    'libvlc_event_manager_t*': 'EventManager',
    'libvlc_media_discoverer_t*': 'MediaDiscoverer',
    'libvlc_media_library_t*': 'MediaLibrary',
    'libvlc_media_list_t*': 'MediaList',
    'libvlc_media_list_player_t*': 'MediaListPlayer',
    'libvlc_media_list_view_t*': 'MediaListView',
    'libvlc_track_description_t*': 'TrackDescription',
    'libvlc_audio_output_t*': 'AudioOutput',

    'mediacontrol_Instance*': 'MediaControl',
    'mediacontrol_Exception*': 'MediaControlException',
    'mediacontrol_RGBPicture*': 'RGBPicture',
    'mediacontrol_PlaylistSeq*': 'MediaControlPlaylistSeq',
    'mediacontrol_Position*': 'MediaControlPosition',
    'mediacontrol_StreamInformation*': 'MediaControlStreamInformation',
    'WINDOWHANDLE': 'ctypes.c_ulong',

    'void': 'None',
    'void*': 'ctypes.c_void_p',
    'short': 'ctypes.c_short',
    'char*': 'ctypes.c_char_p',
    'char**': 'ListPOINTER(ctypes.c_char_p)',
    'uint32_t': 'ctypes.c_uint',
    'float': 'ctypes.c_float',
    'unsigned': 'ctypes.c_uint',
    'int': 'ctypes.c_int',
    '...': 'FIXMEva_list',
    'libvlc_callback_t': 'FIXMEcallback',
    'libvlc_time_t': 'ctypes.c_longlong',
    }

# Defined python classes, i.e. classes for which we want to generate
# class wrappers around libvlc functions
defined_classes=(
    'MediaPlayer',
    'Instance',
    'Media',
    'Log',
    'LogIterator',
    #'LogMessage',
    'EventType',
    'EventManager',
    'MediaDiscoverer',
    'MediaLibrary',
    'MediaList',
    'MediaListPlayer',
    'MediaListView',
    'TrackDescription',
    'AudioOutput',
    'MediaControl',
    #'RGBPicture',
    #'MediaControlPosition',
    #'MediaControlStreamInformation',
    )

# Definition of prefixes that we can strip from method names when
# wrapping them into class methods
prefixes=dict( (v, k[:-2]) for (k, v) in typ2class.iteritems() if  v in defined_classes )
prefixes['MediaControl']='mediacontrol_'

def parse_param(s):
    """Parse a C parameter expression.

    It is used to parse both the type/name for methods, and type/name
    for their parameters.

    It returns a tuple (type, name).
    """
    s=s.strip()
    s=s.replace('const', '')
    if 'VLC_FORWARD' in s:
        m=forward_re.match(s)
        s=m.group(1)+m.group(2)
    m=param_re.search(s)
    if m:
        const, typ, name=m.groups()
        while name.startswith('*'):
            typ += '*'
            name=name[1:]
        if name == 'const*':
            # K&R definition: const char * const*
            name=''
        typ=typ.replace(' ', '')
        return typ, name
    else:
        # K&R definition: only type
        return s.replace(' ', ''), ''

def generate_header(classes=None):
    """Generate header code.
    """
    f=open('header.py', 'r')
    for l in f:
        if 'build_date' in l:
            print 'build_date="%s"' % time.ctime()
        else:
            print l,
    f.close()

def convert_enum_names(enums):
    res={}
    for (typ, name, values) in enums:
        if typ != 'enum':
            raise Exception('This method only handles enums')
        pyname=re.findall('(libvlc|mediacontrol)_(.+?)(_t)?$', name)[0][1]
        if '_' in pyname:
            pyname=pyname.title().replace('_', '')
        else:
            pyname=pyname.capitalize()
        res[name]=pyname
    return res

def generate_enums(enums):
    for (typ, name, values) in enums:
        if typ != 'enum':
            raise Exception('This method only handles enums')
        pyname=typ2class[name]

        print "class %s(ctypes.c_uint):" % pyname

        conv={}
        # Convert symbol names
        for k, v in values:
            n=k.split('_')[-1]
            if len(n) == 1:
                # Single character. Some symbols use 1_1, 5_1, etc.
                n="_".join( k.split('_')[:-2] )
            if re.match('^[0-9]', n):
                # Cannot start an identifier with a number
                n='_'+n
            conv[k]=n

        for k, v in values:
            print "    %s=%s" % (conv[k], v)

        print "    _names={"
        for k, v in values:
            print "        %s: '%s'," % (v, conv[k])
        print "    }"

        print """
    def __repr__(self):
        return ".".join((self.__class__.__module__, self.__class__.__name__, self._names[self.value]))
"""

def parse_typedef(name):
    """Parse include file for typedef expressions.

    This generates a tuple for each typedef:
    (type, name, value_list)
    with type == 'enum' (for the moment) and value_list being a list of (name, value)
    Note that values are string, since this is intended for code generation.
    """
    f=open(name, 'r')
    accumulator=''
    for l in f:
        # Note: lstrip() should not be necessary, but there is 1 badly
        # formatted comment in vlc1.0.0 includes
        if l.lstrip().startswith('/**'):
            comment=''
            continue
        elif l.startswith(' * '):
            comment = comment + l[3:]
            continue

        l=l.strip()

        if accumulator:
            accumulator=" ".join( (accumulator, l) )
            if l.endswith(';'):
                # End of definition
                l=accumulator
                accumulator=''
        elif l.startswith('typedef enum') and not l.endswith(';'):
            # Multiline definition. Accumulate until end of definition
            accumulator=l
            continue

        m=enum_re.match(l)
        if m:
            values=[]
            (typ, dummy, data, name)=m.groups()
            for i, l in enumerate(paramlist_re.split(data)):
                l=l.strip()
                if l.startswith('/*'):
                    continue
                if '=' in l:
                    # A value was specified. Use it.
                    values.append(re.split('\s*=\s*', l))
                else:
                    if l:
                        values.append( (l, str(i)) )
            yield (typ, name, values)

def parse_include(name):
    """Parse include file.

    This generates a tuple for each function:
    (return_type, method_name, parameter_list, comment)
    with parameter_list being a list of tuples (parameter_type, parameter_name).
    """
    f=open(name, 'r')
    accumulator=''
    comment=''
    for l in f:
        # Note: lstrip() should not be necessary, but there is 1 badly
        # formatted comment in vlc1.0.0 includes
        if l.lstrip().startswith('/**'):
            comment=''
            continue
        elif l.startswith(' * '):
            comment = comment + l[3:]
            continue

        l=l.strip()

        if accumulator:
            accumulator=" ".join( (accumulator, l) )
            if l.endswith(');'):
                # End of definition
                l=accumulator
                accumulator=''
        elif l.startswith('VLC_PUBLIC_API') and not l.endswith(');'):
            # Multiline definition. Accumulate until end of definition
            accumulator=l
            continue

        m=api_re.match(l)
        if m:
            (ret, param)=m.groups()

            rtype, method=parse_param(ret)

            params=[]
            for p in paramlist_re.split(param):
                params.append( parse_param(p) )

            if len(params) == 1 and params[0][0] == 'void':
                # Empty parameter list
                params=[]

            if list(p for p in params if not p[1]):
                # Empty parameter names. Have to poke into comment.
                names=comment_re.findall(comment)
                if len(names) < len(params):
                    # Bad description: all parameters are not specified.
                    # Generate default parameter names
                    badnames=[ "param%d" % i for i in xrange(len(params)) ]
                    # Put in the existing ones
                    for (i, p) in enumerate(names):
                        badnames[i]=names[i]
                    names=badnames
                    print "### Error ###"
                    print "### Cannot get parameter names from comment for %s: %s" % (method, comment.replace("\n", ' '))
                    # Note: this was previously
                    # raise Exception("Cannot get parameter names from comment for %s: %s" % (method, comment))
                    # but it prevented code generation for a minor detail (some bad descriptions).
                params=[ (p[0], names[i]) for (i, p) in enumerate(params) ]

            for typ, name in params:
                if not typ in typ2class:
                    raise Exception("No conversion for %s (from %s:%s)" % (typ, method, name))

            # Transform Doxygen syntax into epydoc syntax
            comment=comment.replace('\\param', '@param').replace('\\return', '@return')

            if debug:
                print '********************'
                print l
                print '-------->'
                print "%s (%s)" % (method, rtype)
                for typ, name in params:
                    print "        %s (%s)" % (name, typ)
                print '********************'
            yield (rtype,
                   method,
                   params,
                   comment)

def output_ctypes(rtype, method, params, comment):
    """Output ctypes decorator for the given method.
    """
    if method in blacklist:
        # FIXME
        return

    if params:
        print "prototype=ctypes.CFUNCTYPE(%s, %s)" % (typ2class.get(rtype, 'FIXME_%s' % rtype),
                                                      ",".join( typ2class[p[0]] for p in params ))
    else:
        print "prototype=ctypes.CFUNCTYPE(%s)" % typ2class.get(rtype, 'FIXME_%s' % rtype)


    if not params:
        flags='paramflags= tuple()'
    elif len(params) == 1:
        flags="paramflags=( (%d, ), )" % parameter_passing[params[0][0]]
    else:
        flags="paramflags=%s" % ",".join( '(%d,)' % parameter_passing[p[0]] for p in params )
    print flags
    print '%s = prototype( ("%s", dll), paramflags )' % (method, method)
    if '3' in flags:
        # A VLCException is present. Process it.
        print "%s.errcheck = check_vlc_exception" % method
    print '%s.__doc__ = """%s"""' % (method, comment)
    print

def parse_override(name):
    """Parse override definitions file.

    It is possible to override methods definitions in classes.
    """
    res={}

    data=[]
    current=None
    f=open(name, 'r')
    for l in f:
        m=re.match('class (\S+):', l)
        if m:
            # Dump old data
            if current is not None:
                res[current]="\n".join(data)
            current=m.group(1)
            data=[]
            continue
        data.append(l)
    res[current]="\n".join(data)
    f.close()
    return res

def fix_python_comment(c):
    """Fix comment by removing first and last parameters (self and exception)
    """
    data=c.splitlines()
    body=itertools.takewhile(lambda l: not '@param' in l, data)
    param=[ python_param_re.sub('\\1:\\2', l) for l in  itertools.ifilter(lambda l: '@param' in l, data) ]
    ret=[ l.replace('@return', '@return:') for l in itertools.ifilter(lambda l: '@return' in l, data) ]

    if len(param) >= 2:
        param=param[1:-1]
    elif len(param) == 1:
        param=[]

    return "\n".join(itertools.chain(body, param, ret))

def generate_wrappers(methods):
    """Generate class wrappers for all appropriate methods.

    @return: the set of wrapped method names
    """
    ret=set()
    # Sort methods against the element they apply to.
    elements=sorted( ( (typ2class.get(params[0][0]), rt, met, params, c)
                       for (rt, met, params, c) in methods
                       if params and typ2class.get(params[0][0], '_') in defined_classes
                       ),
                     key=operator.itemgetter(0))

    overrides=parse_override('override.py')

    for classname, el in itertools.groupby(elements, key=operator.itemgetter(0)):
        print """
class %(name)s(object):
    def __new__(cls, pointer=None):
        '''Internal method used for instanciating wrappers from ctypes.
        '''
        if pointer is None:
            raise Exception("Internal method. You should instanciate objects through other class methods (probably named 'new' or ending with 'new')")
        if pointer == 0:
            return None
        else:
            o=object.__new__(cls)
            o._as_parameter_=ctypes.c_void_p(pointer)
            return o

    @staticmethod
    def from_param(arg):
        '''(INTERNAL) ctypes parameter conversion method.
        '''
        return arg._as_parameter_
""" % {'name': classname}

        if classname in overrides:
            print overrides[classname]

        prefix=prefixes.get(classname, '')

        for cl, rtype, method, params, comment in el:
            if method in blacklist:
                continue
            # Strip prefix
            name=method.replace(prefix, '').replace('libvlc_', '')
            ret.add(method)
            if params:
                params[0]=(params[0][0], 'self')
            if params and params[-1][0] in ('libvlc_exception_t*', 'mediacontrol_Exception*'):
                args=", ".join( p[1] for p in params[:-1] )
            else:
                args=", ".join( p[1] for p in params )

            print "    def %s(%s):" % (name, args)
            print '        """%s\n"""' % fix_python_comment(comment)
            if params and params[-1][0] == 'libvlc_exception_t*':
                # Exception handling
                print "        e=VLCException()"
                print "        return %s(%s, e)" % (method, args)
            elif params and params[-1][0] == 'mediacontrol_Exception*':
                # Exception handling
                print "        e=MediaControlException()"
                print "        return %s(%s, e)" % (method, args)
            else:
                print "        return %s(%s)" % (method, args)
            print

            # Check for standard methods
            if name == 'count':
                # There is a count method. Generate a __len__ one.
                print "    def __len__(self):"
                print "        e=VLCException()"
                print "        return %s(self, e)" % method
                print
            elif name.endswith('item_at_index'):
                # Indexable (and thus iterable)"
                print "    def __getitem__(self, i):"
                print "        e=VLCException()"
                print "        return %s(self, i, e)" % method
                print
                print "    def __iter__(self):"
                print "        e=VLCException()"
                print "        for i in xrange(len(self)):"
                print "            yield self[i]"
                print

    return ret

if __name__ == '__main__':
    enums=[]
    for name in sys.argv[1:]:
        enums.extend(list(parse_typedef(name)))
    # Generate python names for enums
    typ2class.update(convert_enum_names(enums))

    methods=[]
    for name in sys.argv[1:]:
        methods.extend(list(parse_include(name)))
    if debug:
        sys.exit(0)

    generate_header()
    generate_enums(enums)
    wrapped=generate_wrappers(methods)
    for l in methods:
        output_ctypes(*l)

    all=set( t[1] for t in methods )
    not_wrapped=all.difference(wrapped)
    print "# Not wrapped methods:"
    for m in not_wrapped:
        print "#   ", m

