#!/usr/bin/env python3

# Copyright © 2022 VideoLabs, VLC authors and VideoLAN
#
# Authors: Steve Lhomme <robux4@videolabs.io>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.

import argparse
import os
import pathlib
import hashlib

# Argument parsing
parser = argparse.ArgumentParser(description="Generate WIX compatible wxs listing")
parser.add_argument('-d', "--dir", type=pathlib.Path, help='directory with files to list')
parser.add_argument('-out', type=argparse.FileType('w', encoding='UTF-8'), help="output file")
parser.add_argument('-pdb', action=argparse.BooleanOptionalAction, help='keep PDB files', default=False)
parser.add_argument('-dr', "--directory-reference", help='directory reference')
parser.add_argument('-cg', "--component-group", help='component group')
args, remaining = parser.parse_known_args()

# print(args.out)
# print(args.dir)

def generate_id(prefix='', file=''):
    joined = prefix + file
    hash_object = hashlib.sha1(joined.encode('ascii'))
    pbHash = hash_object.hexdigest()
    return prefix + pbHash.upper()

args.out.write('<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n')
args.out.write('<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">\r\n')
args.out.write('    <Fragment>\r\n')
args.out.write('        <DirectoryRef Id="{}">\r\n'.format(args.directory_reference))

# args.out.write('            <Directory Id="{}" Name="{}">\r\n'.format(generate_id('dir', args.dir.name), args.dir.name))

fileIdList =[]

def outputDir(top, parent: str, dir: str, with_pdb: bool):
    cwd = top.joinpath(parent).joinpath(dir)
    dirName = os.path.join(parent, dir)
    if dir=='':
        dir = top.name
        dirId = generate_id('dir', os.path.join(top.name, dir))
    else:
        dirId = generate_id('dir', os.path.join(parent, dir))
    args.out.write('                <Directory Id="{}" Name="{}">\r\n'.format(dirId, dir))
    if cwd.is_dir():
        # first list files
        for file in cwd.iterdir():
            if not file.is_dir():
                # args.out.write('          file   <{}>\r\n'.format(file))
                if not file.name.endswith('.pdb'):
                    outname = os.path.join(dirName, file.name)
                    fileId = generate_id('cmp', outname)
                    args.out.write('                    <Component Id="{}" Guid="*">\r\n'.format(fileId))
                    fileIdList.append(fileId)
                    args.out.write('                        <File Id="{}" Name="{}" KeyPath="yes" Source="SourceDir/{}"/>\r\n'.format(generate_id('fil', outname), file.name, outname))
                    args.out.write('                    </Component>\r\n')
        # then sub directories
        for file in cwd.iterdir():
            if file.is_dir():
                # args.out.write('         dir    <{}>\r\n'.format(file))
                outputDir(top, dirName, file.name, with_pdb)

    args.out.write('                </Directory>\r\n')

print(args.dir.parent)
print(args.dir.name)
outputDir(args.dir, '', '', args.pdb)

# args.out.write('            </Directory>\r\n')
args.out.write('        </DirectoryRef>\r\n')
args.out.write('    </Fragment>\r\n')

args.out.write('    <Fragment>\r\n')
args.out.write('        <ComponentGroup Id="{}">\r\n'.format(args.component_group))
for name in fileIdList:
    args.out.write('                    <ComponentRef Id="{}"/>\r\n'.format(name))
args.out.write('        </ComponentGroup>\r\n')
args.out.write('    </Fragment>\r\n')

args.out.write('</Wix>\r\n')
