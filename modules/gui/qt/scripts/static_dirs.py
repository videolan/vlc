#!/usr/bin/env python3

# SPDX-License-Identifier: ISC
# Copyright © 2025 VideoLabs, VLC authors and VideoLAN
#
# Authors: Steve Lhomme <robux4@videolabs.io>

import argparse
import os
import pathlib
import re
import subprocess
import sys

def call_qmake(qmake:str, qtconf, builddir, pro, debug:bool) -> str:
    if builddir and builddir != '' and not os.path.exists(builddir):
        os.makedirs(builddir)
    qmake_cmd = [ qmake ]
    if debug:
        qmake_cmd += ['CONFIG+=debug']
    if qtconf and qtconf != '':
        qmake_cmd += [ '-qtconf', qtconf ]
    qmake_cmd += [pro, '-o', '-' ]
    call = subprocess.Popen(qmake_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=builddir)
    stdout_bin, stderr_bin = call.communicate()
    errcode = call.wait()

    if errcode != 0:
        sys.stderr.write(stderr_bin.decode('utf-8'))
        sys.exit(errcode)

    return stdout_bin.decode('utf-8')

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="This program provides the list of directories/defines to use with Qt static"
    )
    parser.add_argument("--qmake",
                        type=pathlib.Path, required=True,
                        help="native qmake path")
    parser.add_argument("--qtconf",
                        type=str, required=False,
                        help="qmake qtconf path")
    parser.add_argument("--pro",
                        type=pathlib.Path, required=True,
                        help=".pro file to use as base")
    parser.add_argument("--builddir",
                        type=str, required=True,
                        help="build directory path")

    parser.add_argument("--cflags",
                        required=False, action='store_true',
                        help="get the list of compiler flags")
    parser.add_argument("--libs",
                        required=False, action='store_true',
                        help="get the list of libraries")
    parser.add_argument("--ldflags",
                        required=False, action='store_true',
                        help="get the list of linker flags")
    parser.add_argument("--debug",
                        required=False, action='store_true',
                        help="debug mode")
    parser.add_argument("--moc_include_dirs",
                        required=False, action='store_true',
                        help="moc include dirs")
    args = parser.parse_args()

    result = ''
    sources = [ os.path.join(args.builddir, '.qmake.stash') ]
    in_sources = False
    makefile = call_qmake(args.qmake, args.qtconf, args.builddir, args.pro, args.debug)

    moc_include_dirs_re = re.compile(r"^\s+moc_include_dirs\s+(.*)")
    for line in makefile.splitlines():
        if in_sources:
            l = line.strip()
            in_sources = l.endswith('\\')
            if in_sources:
                l = l[:-1].strip()
            sources += l.split(' ')
        elif line.startswith('SOURCES '):
            l = re.sub(r'SOURCES[\W]+=', ' ', line).strip()
            in_sources = l.endswith(' \\')
            if in_sources:
                l = l[:-1].strip()
            sources += l.split(' ')
        elif line.startswith('DEFINES '):
            if args.cflags:
                l = re.sub(r'DEFINES[\W]+=', ' ', line)
                for i in l.strip().split(' '):
                    result += ' ' + i
        elif line.startswith('INCPATH '):
            if args.cflags:
                l = re.sub(r'INCPATH[\W]+=', ' ', line)
                for i in l.strip().split(' '):
                    result += ' ' + i
        elif line.startswith('LFLAGS '):
            if args.ldflags:
                l = re.sub(r'LFLAGS[\W]+=', ' ', line)
                for i in l.strip().split(' '):
                    result += ' ' + i
        elif line.startswith('LIBS '):
            if args.libs:
                l = re.sub(r'LIBS[\W]+=', ' ', line).split(' ')
                for lib in l:
                    if lib == '':
                        continue
                    if lib.startswith('-l'):
                        result += ' ' + lib
                    elif lib.endswith('.so') or lib.endswith('.a'):
                        libname = os.path.splitext(os.path.basename(lib))[0]
                        if libname.startswith('lib'):
                            libdir = os.path.dirname(lib)
                            result += ' -L' + libdir
                            result += ' -l' + libname[3:]
                    else:
                        result += ' ' + lib
        elif args.moc_include_dirs:
            m = moc_include_dirs_re.match(line)
            if m:
                for i in l.strip().split(' '):
                    result += f"-I{i}"

    for generated in sources:
        if os.path.exists(generated):
            os.remove(generated)

    sys.stdout.write(result)
