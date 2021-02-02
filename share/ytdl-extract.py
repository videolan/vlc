#! /usr/bin/python3
#
# Copyright (C) 2020 Rémi Denis-Courmont
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
import json
import urllib.parse
import youtube_dl

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
        'youtube_include_dash_manifest': False,
    }

    dl = youtube_dl.YoutubeDL(opts)

    # Process a given URL
    infos = dl.extract_info(url, download=False)

    if 'entries' in infos:
        for entry in infos['entries']:
             if 'ie_key' in entry and entry['ie_key']:
                 # Flat-extracted playlist entry
                 url = 'ytdl:///?' + urllib.parse.urlencode(entry)
                 entry['url'] = url;

    print(json.dumps(infos))

def url_process(ie_url):
    opts = {
        'logger': logger(),
        'youtube_include_dash_manifest': False,
    }

    dl = youtube_dl.YoutubeDL(opts)

    # Rebuild the original IE entry
    entry = { }

    for p in urllib.parse.parse_qsl(url[9:]):
        entry[p[0]] = p[1]

    infos = dl.process_ie_result(entry, download=False)
    print(json.dumps(infos))

url = sys.argv[1]

if url.startswith('ytdl:///?'):
    url_process(url)
else:
    url_extract(url)
