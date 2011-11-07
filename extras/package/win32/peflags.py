#!/usr/bin/env python
# -*- coding: utf8 -*-
#
# Copyright © 2011 Rafaël Carré <funman at videolanorg>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
#

from os import SEEK_CUR
from sys import argv

def get_le(f, n):
    array = bytearray(f.read(n))
    if len(array) != n:
        raise Exception('Reading failed!')
    shift = 0
    ret = 0
    for c in array:
        ret = ret + (c << shift)
        shift = shift + 8
    return ret

###

if len(argv) != 2:
    exit(1)

f = open(argv[1], 'r+b')

f.seek(0x3c)
f.seek(get_le(f, 4))

if get_le(f, 4) != 0x00004550: # IMAGE_NT_SIGNATURE
    raise Exception('Not a NT executable')

f.seek(20 + 70, SEEK_CUR)

flags = get_le(f, 2)
f.seek(-2, SEEK_CUR)
flags |= 0x100  # NX Compat
flags |= 0x40   # Dynamic Base

f.write(bytearray([flags & 0xff, (flags >> 8) & 0xff ]))

f.close
