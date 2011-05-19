#!/usr/bin/env python
# -*- coding: utf8 -*-
#
# Copyright © 2006-2011 Rafaël Carré <funman at videolanorg>
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

#
# NOTE: This controller is a SAMPLE, and thus doesn't use all the
# Media Player Remote Interface Specification (MPRIS for short) capabilities
#
# MPRIS: http://www.mpris.org/2.1/spec/
#
# You'll need pygtk >= 2.12
#
# TODO
#   Ability to choose the Media Player if several are connected to the bus

# core dbus stuff
import dbus
import dbus.glib

# core interface stuff
import gtk

# timer
from gobject import timeout_add

# file loading
import os

global win_position # store the window position on the screen

global playing
playing = False

global shuffle

global root
global player
global tracklist
global props

global bus          # Connection to the session bus

mpris='org.mpris.MediaPlayer2'

# If a Media Player connects to the bus, we'll use it
# Note that we forget the previous Media Player we were connected to
def NameOwnerChanged(name, new, old):
    if old != '' and mpris in name:
        Connect(name)

def PropGet(prop):
    global props
    return props.Get(mpris + '.Player', prop)

def PropSet(prop, val):
    global props
    props.Set(mpris + '.Player', prop, val)

# Callback for when 'TrackChange' signal is emitted
def TrackChange(Track):
    try:
        a = Track['xesam:artist']
    except:
        a = ''
    try:
        t = Track['xesam:title']
    except:
        t = Track['xesam:url']
    try:
        length = Track['mpris:length']
    except:
        length = 0
    if length > 0:
        time_s.set_range(0, length)
        time_s.set_sensitive(True)
    else:
        # disable the position scale if length isn't available
        time_s.set_sensitive(False)
    # update the labels
    l_artist.set_text(a)
    l_title.set_text(t)

# Connects to the Media Player we detected
def Connect(name):
    global root, player, tracklist, props
    global playing, shuffle

    root_o = bus.get_object(name, '/org/mpris/MediaPlayer2')
    root        = dbus.Interface(root_o, mpris)
    tracklist   = dbus.Interface(root_o, mpris + '.TrackList')
    player      = dbus.Interface(root_o, mpris + '.Player')
    props       = dbus.Interface(root_o, dbus.PROPERTIES_IFACE)

    # FIXME : doesn't exist anymore in mpris 2.1
    # connect to the TrackChange signal
    # root_o.connect_to_signal('TrackChange', TrackChange, dbus_interface=mpris)

    # determine if the Media Player is playing something
    if PropGet('PlaybackStatus') == 'Playing':
        playing = True
        TrackChange(PropGet('Metadata'))

    window.set_title(props.Get(mpris, 'Identity'))

#plays an element
def AddTrack(widget):
    mrl = e_mrl.get_text()
    if mrl != None and mrl != '':
        tracklist.AddTrack(mrl, '/', True)
        e_mrl.set_text('')
    else:
        mrl = bt_file.get_filename()
        if mrl != None and mrl != '':
            tracklist.AddTrack('directory://' + mrl, '/', True)
    update(0)

# basic control

def Next(widget):
    player.Next(reply_handler=(lambda *args: None), error_handler=(lambda *args: None))
    update(0)

def Prev(widget):
    player.Prev(reply_handler=(lambda *args: None), error_handler=(lambda *args: None))
    update(0)

def Stop(widget):
    player.Stop(reply_handler=(lambda *args: None), error_handler=(lambda *args: None))
    update(0)

def Quit(widget):
    global props
    if props.Get(mpris, 'CanQuit'):
        root.Quit(reply_handler=(lambda *args: None), error_handler=(lambda *args: None))
        l_title.set_text('')
        window.set_title('')

def Pause(widget):
    player.PlayPause()
    if PropGet('PlaybackStatus') == 'Playing':
        icon = gtk.STOCK_MEDIA_PAUSE
    else:
        icon = gtk.STOCK_MEDIA_PLAY
    img_bt_toggle.set_from_stock(icon, gtk.ICON_SIZE_SMALL_TOOLBAR)
    update(0)

def Shuffle(widget):
    global shuffle
    shuffle = not shuffle
    PropSet('Shuffle', shuffle)

# update status display
def update(widget):
    Track = PropGet('Metadata')
    vol.set_value(PropGet('Volume') * 100.0)
    TrackChange(Track)
    GetPlayStatus(0)

# callback for volume change
def volchange(widget):
    PropSet('Volume', vol.get_value_as_int() / 100.0)

# callback for position change
def timechange(widget, x=None, y=None):
    player.SetPosition(PropGet('Metadata')['mpris:trackid'],
                time_s.get_value(),
                reply_handler=(lambda *args: None),
                error_handler=(lambda *args: None))

# refresh position change
def timeset():
    global playing
    if playing == True:
        try:
            time_s.set_value(PropGet('Position'))
        except:
            playing = False
    return True

# toggle simple/full display
def expander(widget):
    if exp.get_expanded() == False:
        exp.set_label('Less')
    else:
        exp.set_label('More')

# close event : hide in the systray
def delete_event(self, widget):
    self.hide()
    return True

# shouldn't happen
def destroy(widget):
    gtk.main_quit()

# hide the controller when 'Esc' is pressed
def key_release(widget, event):
    if event.keyval == gtk.keysyms.Escape:
        global win_position
        win_position = window.get_position()
        widget.hide()

# callback for click on the tray icon
def tray_button(widget):
    global win_position
    if window.get_property('visible'):
        # store position
        win_position = window.get_position()
        window.hide()
    else:
        # restore position
        window.move(win_position[0], win_position[1])
        window.show()

# hack: update position, volume, and metadata
def icon_clicked(widget, event):
    update(0)

# get playing status, modify the Play/Pause button accordingly
def GetPlayStatus(widget):
    global playing
    global shuffle

    playing = PropGet('PlaybackStatus') == 'Playing'
    if playing:
        img_bt_toggle.set_from_stock('gtk-media-pause', gtk.ICON_SIZE_SMALL_TOOLBAR)
    else:
        img_bt_toggle.set_from_stock('gtk-media-play', gtk.ICON_SIZE_SMALL_TOOLBAR)
    shuffle = PropGet('Shuffle')
    bt_shuffle.set_active( shuffle )

# loads UI file from the directory where the script is,
# so we can use /path/to/mpris.py to execute it.
import sys
xml = gtk.Builder()
gtk.Builder.add_from_file(xml, os.path.join(os.path.dirname(sys.argv[0]) , 'mpris.xml'))

# ui setup
bt_close    = xml.get_object('close')
bt_quit     = xml.get_object('quit')
bt_file     = xml.get_object('ChooseFile')
bt_next     = xml.get_object('next')
bt_prev     = xml.get_object('prev')
bt_stop     = xml.get_object('stop')
bt_toggle   = xml.get_object('toggle')
bt_mrl      = xml.get_object('AddMRL')
bt_shuffle  = xml.get_object('shuffle')
l_artist    = xml.get_object('l_artist')
l_title     = xml.get_object('l_title')
e_mrl       = xml.get_object('mrl')
window      = xml.get_object('window1')
img_bt_toggle=xml.get_object('image6')
exp         = xml.get_object('expander2')
expvbox     = xml.get_object('expandvbox')
audioicon   = xml.get_object('eventicon')
vol         = xml.get_object('vol')
time_s      = xml.get_object('time_s')
time_l      = xml.get_object('time_l')

# connect to the different callbacks

window.connect('delete_event',  delete_event)
window.connect('destroy',       destroy)
window.connect('key_release_event', key_release)

tray = gtk.status_icon_new_from_icon_name('audio-x-generic')
tray.connect('activate', tray_button)

bt_close.connect('clicked',     destroy)
bt_quit.connect('clicked',      Quit)
bt_mrl.connect('clicked',       AddTrack)
bt_toggle.connect('clicked',    Pause)
bt_next.connect('clicked',      Next)
bt_prev.connect('clicked',      Prev)
bt_stop.connect('clicked',      Stop)
bt_shuffle.connect('clicked',   Shuffle)
exp.connect('activate',         expander)
vol.connect('changed',          volchange)
time_s.connect('adjust-bounds', timechange)
audioicon.set_events(gtk.gdk.BUTTON_PRESS_MASK) # hack for the bottom right icon
audioicon.connect('button_press_event', icon_clicked)
time_s.set_update_policy(gtk.UPDATE_DISCONTINUOUS)

library = '/media/mp3' # editme

# set the Directory chooser to a default location
try:
    os.chdir(library)
    bt_file.set_current_folder(library)
except:
    bt_file.set_current_folder(os.path.expanduser('~'))

# connect to the bus
bus = dbus.SessionBus()
dbus_names = bus.get_object( 'org.freedesktop.DBus', '/org/freedesktop/DBus' )
dbus_names.connect_to_signal('NameOwnerChanged', NameOwnerChanged, dbus_interface='org.freedesktop.DBus') # to detect new Media Players

dbus_o = bus.get_object('org.freedesktop.DBus', '/')
dbus_intf = dbus.Interface(dbus_o, 'org.freedesktop.DBus')

# connect to the first Media Player found
for n in dbus_intf.ListNames():
    if mpris in n:
        Connect(n)
        vol.set_value(PropGet('Volume') * 100.0)
        update(0)
        break

# run a timer to update position
timeout_add( 1000, timeset)

window.set_icon_name('audio-x-generic')
window.show()

window.set_icon(gtk.icon_theme_get_default().load_icon('audio-x-generic',24,0))
win_position = window.get_position()

gtk.main() # execute the main loop
