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
from optparse import OptionParser

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

    "libvlc_media_list_view_index_of_item",
    "libvlc_media_list_view_insert_at_index",
    "libvlc_media_list_view_remove_at_index",
    "libvlc_media_list_view_add_item",

    # In svn but not in current 1.0.0.
    #"libvlc_media_add_option_flag",
    #'libvlc_video_set_deinterlace',
    #'libvlc_video_get_marquee_option_as_int',
    #'libvlc_video_get_marquee_option_as_string',
    #'libvlc_video_set_marquee_option_as_int',
    #'libvlc_video_set_marquee_option_as_string',
    #'libvlc_vlm_get_event_manager',
    #"libvlc_media_list_player_event_manager",
    #'libvlc_media_player_next_frame',

    'mediacontrol_PlaylistSeq__free',
    ]

# Precompiled regexps
api_re=re.compile('VLC_PUBLIC_API\s+(\S+\s+.+?)\s*\(\s*(.+?)\s*\)')
param_re=re.compile('\s*(const\s*|unsigned\s*|struct\s*)?(\S+\s*\**)\s+(.+)')
paramlist_re=re.compile('\s*,\s*')
comment_re=re.compile('\\param\s+(\S+)')
python_param_re=re.compile('(@param\s+\S+)(.+)')
forward_re=re.compile('.+\(\s*(.+?)\s*\)(\s*\S+)')
enum_re=re.compile('typedef\s+(enum)\s*(\S+\s*)?\{\s*(.+)\s*\}\s*(\S+);')
special_enum_re=re.compile('^(enum)\s*(\S+\s*)?\{\s*(.+)\s*\};')
event_def_re=re.compile('^DEF\(\s*(\w+)\s*\)')

# Definition of parameter passing mode for types.  This should not be
# hardcoded this way, but works alright ATM.
parameter_passing=DefaultDict(default=1)
parameter_passing['libvlc_exception_t*']=3

class Parser(object):
    def __init__(self, list_of_files):
        self.methods=[]
        self.enums=[]

        for name in list_of_files:
            self.enums.extend(self.parse_typedef(name))
            self.methods.extend(self.parse_include(name))

    def parse_param(self, s):
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

    def parse_typedef(self, name):
        """Parse include file for typedef expressions.

        This generates a tuple for each typedef:
        (type, name, value_list, comment)
        with type == 'enum' (for the moment) and value_list being a list of (name, value)
        Note that values are string, since this is intended for code generation.
        """
        event_names=[]

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
            if l.startswith('/*') or l.endswith('*/'):
                continue

            if (l.startswith('typedef enum') or l.startswith('enum')) and not l.endswith(';'):
                # Multiline definition. Accumulate until end of definition
                accumulator=l
                continue
            elif accumulator:
                accumulator=" ".join( (accumulator, l) )
                if l.endswith(';'):
                    # End of definition
                    l=accumulator
                    accumulator=''

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
                comment=comment.replace('@{', '').replace('@see', 'See').replace('\ingroup', '')
                yield (typ, name.strip(), values, comment)
                comment=''
                continue

            # Special case, used only for libvlc_events.h
            # (version after 96a96f60bb0d1f2506e68b356897ceca6f6b586d)
            m=event_def_re.match(l)
            if m:
                # Event definition.
                event_names.append('libvlc_'+m.group(1))
                continue

            # Special case, used only for libvlc_events.h
            m=special_enum_re.match(l)
            if m:
                (typ, name, data)=m.groups()
                if event_names:
                    # event_names were defined through DEF macro
                    # (see 96a96f60bb0d1f2506e68b356897ceca6f6b586d)
                    values=list( (n, str(i)) for i, n in enumerate(event_names))
                else:
                    # Before 96a96f60bb0d1f2506e68b356897ceca6f6b586d
                    values=[]
                    for i, l in enumerate(paramlist_re.split(data)):
                        l=l.strip()
                        if l.startswith('/*') or l.startswith('#'):
                            continue
                        if '=' in l:
                            # A value was specified. Use it.
                            values.append(re.split('\s*=\s*', l))
                        else:
                            if l:
                                values.append( (l, str(i)) )
                comment=comment.replace('@{', '').replace('@see', 'See').replace('\ingroup', '')
                yield (typ, name.strip(), values, comment)
                comment=''
                continue

    def parse_include(self, name):
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

                rtype, method=self.parse_param(ret)

                params=[]
                for p in paramlist_re.split(param):
                    params.append( self.parse_param(p) )

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
                comment=''

    def dump_methods(self):
        print "** Defined functions **"
        for (rtype, name, params, comment) in self.methods:
            print "%(name)s (%(rtype)s):" % locals()
            for t, n in params:
                print "    %(n)s (%(t)s)" % locals()

    def dump_enums(self):
        print "** Defined enums **"
        for (typ, name, values, comment) in self.enums:
            print "%(name)s (%(typ)s):" % locals()
            for k, v in values:
                print "    %(k)s=%(v)s" % locals()

class PythonGenerator(object):
    # C-type to ctypes/python type conversion.
    # Note that enum types conversions are generated (cf convert_enum_names)
    type2class={
        'libvlc_exception_t*': 'ctypes.POINTER(VLCException)',

        'libvlc_media_player_t*': 'MediaPlayer',
        'libvlc_instance_t*': 'Instance',
        'libvlc_media_t*': 'Media',
        'libvlc_log_t*': 'Log',
        'libvlc_log_iterator_t*': 'LogIterator',
        'libvlc_log_message_t*': 'ctypes.POINTER(LogMessage)',
        'libvlc_event_type_t': 'ctypes.c_uint',
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
        'mediacontrol_RGBPicture*': 'ctypes.POINTER(RGBPicture)',
        'mediacontrol_PlaylistSeq*': 'MediaControlPlaylistSeq',
        'mediacontrol_Position*': 'ctypes.POINTER(MediaControlPosition)',
        'mediacontrol_StreamInformation*': 'ctypes.POINTER(MediaControlStreamInformation)',
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
        'libvlc_callback_t': 'ctypes.c_void_p',
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
        'EventManager',
        'MediaDiscoverer',
        'MediaLibrary',
        'MediaList',
        'MediaListPlayer',
        'MediaListView',
        'TrackDescription',
        'AudioOutput',
        'MediaControl',
        )

    def __init__(self, parser=None):
        self.parser=parser

        # Generate python names for enums
        self.type2class.update(self.convert_enum_names(parser.enums))
        self.check_types()

        # Definition of prefixes that we can strip from method names when
        # wrapping them into class methods
        self.prefixes=dict( (v, k[:-2])
                            for (k, v) in self.type2class.iteritems()
                            if  v in self.defined_classes )
        self.prefixes['MediaControl']='mediacontrol_'

    def save(self, filename=None):
        if filename is None or filename == '-':
            self.fd=sys.stdout
        else:
            self.fd=open(filename, 'w')

        self.insert_code('header.py')
        wrapped_methods=self.generate_wrappers(self.parser.methods)
        for l in self.parser.methods:
            self.output_ctypes(*l)
        self.insert_code('footer.py')

        all_methods=set( t[1] for t in self.parser.methods )
        not_wrapped=all_methods.difference(wrapped_methods)
        self.output("# Not wrapped methods:")
        for m in not_wrapped:
            self.output("#   ", m)

        if self.fd != sys.stdout:
            self.fd.close()

    def output(self, *p):
        self.fd.write(" ".join(p))
        self.fd.write("\n")

    def check_types(self):
        """Make sure that all types are properly translated.

        This method must be called *after* convert_enum_names, since
        the latter populates type2class with converted enum names.
        """
        for (rt, met, params, c) in self.parser.methods:
            for typ, name in params:
                if not typ in self.type2class:
                    raise Exception("No conversion for %s (from %s:%s)" % (typ, met, name))

    def insert_code(self, filename):
        """Generate header/footer code.
        """
        f=open(filename, 'r')
        for l in f:
            if l.startswith('build_date'):
                self.output('build_date="%s"' % time.ctime())
            elif l.startswith('# GENERATED_ENUMS'):
                self.generate_enums(self.parser.enums)
            else:
                self.output(l.rstrip())

        f.close()

    def convert_enum_names(self, enums):
        res={}
        for (typ, name, values, comment) in enums:
            if typ != 'enum':
                raise Exception('This method only handles enums')
            pyname=re.findall('(libvlc|mediacontrol)_(.+?)(_t)?$', name)[0][1]
            if '_' in pyname:
                pyname=pyname.title().replace('_', '')
            elif not pyname[0].isupper():
                pyname=pyname.capitalize()
            res[name]=pyname
        return res

    def generate_enums(self, enums):
        for (typ, name, values, comment) in enums:
            if typ != 'enum':
                raise Exception('This method only handles enums')
            pyname=self.type2class[name]

            self.output("class %s(ctypes.c_ulong):" % pyname)
            self.output('    """%s\n    """' % comment)

            conv={}
            # Convert symbol names
            for k, v in values:
                n=k.split('_')[-1]
                if len(n) == 1:
                    # Single character. Some symbols use 1_1, 5_1, etc.
                    n="_".join( k.split('_')[-2:] )
                if re.match('^[0-9]', n):
                    # Cannot start an identifier with a number
                    n='_'+n
                conv[k]=n

            self.output("    _names={")
            for k, v in values:
                self.output("        %s: '%s'," % (v, conv[k]))
            self.output("    }")

            self.output("""
    def __repr__(self):
        return ".".join((self.__class__.__module__, self.__class__.__name__, self._names[self.value]))

    def __eq__(self, other):
        return ( (isinstance(other, ctypes.c_ulong) and self.value == other.value)
                 or (isinstance(other, (int, long)) and self.value == other ) )

    def __ne__(self, other):
        return not self.__eq__(other)
    """)
            for k, v in values:
                self.output("%(class)s.%(attribute)s=%(class)s(%(value)s)" % {
                        'class': pyname,
                        'attribute': conv[k],
                        'value': v
                        })
            self.output("")

    def output_ctypes(self, rtype, method, params, comment):
        """Output ctypes decorator for the given method.
        """
        if method in blacklist:
            # FIXME
            return

        self.output("""if hasattr(dll, '%s'):""" % method)
        if params:
            self.output("    prototype=ctypes.CFUNCTYPE(%s, %s)" % (self.type2class.get(rtype, 'FIXME_%s' % rtype),
                                                                ",".join( self.type2class[p[0]] for p in params )))
        else:
            self.output("    prototype=ctypes.CFUNCTYPE(%s)" % self.type2class.get(rtype, 'FIXME_%s' % rtype))


        if not params:
            flags='    paramflags= tuple()'
        elif len(params) == 1:
            flags="    paramflags=( (%d, ), )" % parameter_passing[params[0][0]]
        else:
            flags="    paramflags=%s" % ",".join( '(%d,)' % parameter_passing[p[0]] for p in params )
        self.output(flags)
        self.output('    %s = prototype( ("%s", dll), paramflags )' % (method, method))
        if '3' in flags:
            # A VLCException is present. Process it.
            self.output("    %s.errcheck = check_vlc_exception" % method)
        self.output('    %s.__doc__ = """%s"""' % (method, comment))
        self.output()

    def parse_override(self, name):
        """Parse override definitions file.

        It is possible to override methods definitions in classes.

        It returns a tuple
        (code, overriden_methods, docstring)
        """
        code={}

        data=[]
        current=None
        f=open(name, 'r')
        for l in f:
            m=re.match('class (\S+):', l)
            if m:
                # Dump old data
                if current is not None:
                    code[current]="".join(data)
                current=m.group(1)
                data=[]
                continue
            data.append(l)
        code[current]="".join(data)
        f.close()

        docstring={}
        for k, v in code.iteritems():
            if v.lstrip().startswith('"""'):
                # Starting comment. Use it as docstring.
                dummy, docstring[k], code[k]=v.split('"""', 2)

        # Not robust wrt. internal methods, but this works for the moment.
        overridden_methods=dict( (k, re.findall('^\s+def\s+(\w+)', v, re.MULTILINE)) for (k, v) in code.iteritems() )

        return code, overridden_methods, docstring

    def fix_python_comment(self, c):
        """Fix comment by removing first and last parameters (self and exception)
        """
        data=c.replace('@{', '').replace('@see', 'See').splitlines()
        body=itertools.takewhile(lambda l: not '@param' in l and not '@return' in l, data)
        param=[ python_param_re.sub('\\1:\\2', l) for l in  itertools.ifilter(lambda l: '@param' in l, data) ]
        ret=[ l.replace('@return', '@return:') for l in itertools.ifilter(lambda l: '@return' in l, data) ]

        if len(param) >= 2:
            param=param[1:-1]
        elif len(param) == 1:
            param=[]

        return "\n".join(itertools.chain(body, param, ret))

    def generate_wrappers(self, methods):
        """Generate class wrappers for all appropriate methods.

        @return: the set of wrapped method names
        """
        ret=set()
        # Sort methods against the element they apply to.
        elements=sorted( ( (self.type2class.get(params[0][0]), rt, met, params, c)
                           for (rt, met, params, c) in methods
                           if params and self.type2class.get(params[0][0], '_') in self.defined_classes
                           ),
                         key=operator.itemgetter(0))

        overrides, overriden_methods, docstring=self.parse_override('override.py')

        for classname, el in itertools.groupby(elements, key=operator.itemgetter(0)):
            self.output("""class %(name)s(object):""" % {'name': classname})
            if classname in docstring:
                self.output('    """%s\n    """' % docstring[classname])

            self.output("""
    def __new__(cls, pointer=None):
        '''Internal method used for instanciating wrappers from ctypes.
        '''
        if pointer is None:
            raise Exception("Internal method. Surely this class cannot be instanciated by itself.")
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
    """ % {'name': classname})

            if classname in overrides:
                self.output(overrides[classname])

            prefix=self.prefixes.get(classname, '')

            for cl, rtype, method, params, comment in el:
                if method in blacklist:
                    continue
                # Strip prefix
                name=method.replace(prefix, '').replace('libvlc_', '')
                ret.add(method)
                if name in overriden_methods.get(cl, []):
                    # Method already defined in override.py
                    continue

                if params:
                    params[0]=(params[0][0], 'self')
                if params and params[-1][0] in ('libvlc_exception_t*', 'mediacontrol_Exception*'):
                    args=", ".join( p[1] for p in params[:-1] )
                else:
                    args=", ".join( p[1] for p in params )

                self.output("    if hasattr(dll, '%s'):" % method)
                self.output("        def %s(%s):" % (name, args))
                self.output('            """%s\n        """' % self.fix_python_comment(comment))
                if params and params[-1][0] == 'libvlc_exception_t*':
                    # Exception handling
                    self.output("            e=VLCException()")
                    self.output("            return %s(%s, e)" % (method, args))
                elif params and params[-1][0] == 'mediacontrol_Exception*':
                    # Exception handling
                    self.output("            e=MediaControlException()")
                    self.output("            return %s(%s, e)" % (method, args))
                else:
                    self.output("            return %s(%s)" % (method, args))
                self.output()

                # Check for standard methods
                if name == 'count':
                    # There is a count method. Generate a __len__ one.
                    if params and params[-1][0] == 'libvlc_exception_t*':
                        self.output("""    def __len__(self):
        e=VLCException()
        return %s(self, e)
""" % method)
                    else:
                        # No exception
                        self.output("""    def __len__(self):
        return %s(self)
""" % method)
                elif name.endswith('item_at_index'):
                    # Indexable (and thus iterable)"
                    self.output("""    def __getitem__(self, i):
        e=VLCException()
        return %s(self, i, e)

    def __iter__(self):
        e=VLCException()
        for i in xrange(len(self)):
            yield self[i]
""" % method)
        return ret

def process(output, list_of_includes):
    p=Parser(list_of_includes)
    g=PythonGenerator(p)
    g.save(output)

if __name__ == '__main__':
    opt=OptionParser(usage="""Parse VLC include files and generate bindings code.
%prog [options] include_file.h [...]""")

    opt.add_option("-d", "--debug", dest="debug", action="store_true",
                      default=False,
                      help="Debug mode")

    opt.add_option("-c", "--check", dest="check", action="store_true",
                      default=False,
                      help="Check mode")

    opt.add_option("-o", "--output", dest="output", action="store",
                      type="str", default="-",
                      help="Output filename")

    (options, args) = opt.parse_args()

    if not args:
        opt.print_help()
        sys.exit(1)

    p=Parser(args)
    if options.check:
        # Various consistency checks.
        for (rt, name, params, comment) in p.methods:
            if not comment.strip():
                print "No comment for %s" % name
                continue
            names=comment_re.findall(comment)
            if len(names) != len(params):
                print "Docstring comment parameters mismatch for %s" % name

    if options.debug:
        p.dump_methods()
        p.dump_enums()

    if options.check or options.debug:
        sys.exit(0)

    g=PythonGenerator(p)

    g.save(options.output)
