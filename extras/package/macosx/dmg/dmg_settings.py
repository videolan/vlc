# -*- coding: utf-8 -*-
from __future__ import unicode_literals
import os

# Configuration file for the dmgbuild tool, used to build
# a fancy DMG for VLC, with background image and aligned
# icons.
#
# This is python, so make sure to write valid python,
# do NOT indent using tabs.
#
# To build a fancy dmg, make sure configure finds dmgbuild
# in your PATH and just do:
#    make package-macosx
#
# To use directly, use this:
#    dmgbuild -s dmgsettings.py "VLC Media Player" VLC.dmg
#
# To specify a different App location:
#    dmgbuild -s dmgsettings.py -D app=/path/to/My.app "My Application" MyApp.dmg

# Application settings
application = defines.get('app', 'VLC.app')
appname = os.path.basename(application)

# Volume format (see hdiutil create -help)
format = defines.get('format', 'UDBZ')

# Volume size (must be large enough for your files)
size = defines.get('size', '150M')

# Files to include
files = [ application ]

# Symlinks to create
symlinks = { 'Applications': '/Applications' }

# Location of the icons
icon_locations = {
    appname:        (181, 168),
    'Applications': (392, 168),
}

# Background
background = 'background.tiff'

# Window configuration
show_status_bar = False
show_tab_view = False
show_toolbar = False
show_pathbar = False
show_sidebar = False
sidebar_width = 180
default_view = 'icon-view'

# Window rect in ((x, y), (w, h)) format
window_rect = ((100, 100), (573, 340))

# Volume icon or badge icon
icon = 'disk_image.icns'
#badge_icon = '/path/to/icon.icns'

# General view configuration
show_icon_preview = False

# Icon view configuration
arrange_by = None
grid_offset = (0, 0)
grid_spacing = 100
scroll_position = (0, 0)
label_pos = 'bottom' # or 'right'
text_size = 14
icon_size = 95
