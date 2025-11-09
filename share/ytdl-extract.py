#! /usr/bin/python3
#
# Copyright (C) 2020 Rémi Denis-Courmont, Brandon Li
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

import sys
import os
import json
import argparse

# Parse first so we can set sys.path
parser = argparse.ArgumentParser(add_help=False)
parser.add_argument('--py-path', dest='py_path')
parser.add_argument('url')
args, _ = parser.parse_known_args()

if args.py_path:
    sys.path.insert(0, args.py_path)

import yt_dlp

class logger(object):
    def debug(self, msg):
        pass

    def warning(self, msg):
        pass

    def error(self, msg):
        sys.stderr.write(msg + '\n')

def url_extract(url):
    opts = {
        'extract_flat': 'in_playlist',
        'logger': logger(),
    }

    dl = yt_dlp.YoutubeDL(opts)

    # Process a given URL
    infos = dl.extract_info(url, download=False)
    print(json.dumps(dl.sanitize_info(infos)))

url = args.url
url_extract(url)
