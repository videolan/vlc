#!/usr/bin/env python
# -*- coding: utf8 -*-
#
# Copyright © 2006-2007 Rafaël Carré <funman at videolanorg>
#
# $Id$
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
# MPRIS:  http://wiki.xmms2.xmms.se/index.php/Media_Player_Interfaces
#
# You'll need pygtk >= 2.10 to use gtk.StatusIcon
#
# TODO
#   Ability to choose the Media Player if several are connected to the bus

# core dbus stuff
import dbus
import dbus.glib

# core interface stuff
import gtk
import gtk.glade

# timer
import gobject

# file loading
import os

global win_position # store the window position on the screen

global playing
playing = False

global shuffle      # playlist will play randomly
global repeat       # repeat the playlist
global loop         # loop the current element

# mpris doesn't support getting the status of these (at the moment)
shuffle = False
repeat = False
loop = False

# these are defined on the mpris detected unique name
global root         # / org.freedesktop.MediaPlayer
global player       # /Player org.freedesktop.MediaPlayer
global tracklist    # /Tracklist org.freedesktop.MediaPlayer

global bus          # Connection to the session bus
global identity     # MediaPlayer Identity


# If a Media Player connects to the bus, we'll use it
# Note that we forget the previous Media Player we were connected to
def NameOwnerChanged(name, new, old):
    if old != "" and "org.mpris." in name:
        Connect(name)

# Callback for when "TrackChange" signal is emitted
def TrackChange(Track):
    # the only mandatory metadata is "URI"
    try:
        a = Track["artist"]
    except:
        a = ""
    try:
        t = Track["title"]
    except:
        t = Track["URI"]
    try:
        length = Track["length"]
    except:
        length = 0
    if length > 0:
        time_s.set_range(0,Track["length"])
        time_s.set_sensitive(True)
    else:
        # disable the position scale if length isn't available
        time_s.set_sensitive(False)
    # update the labels
    l_artist.set_text(a)
    l_title.set_text(t)

# Connects to the Media Player we detected
def Connect(name):
    global root, player, tracklist
    global playing, identity

    # first we connect to the objects
    root_o = bus.get_object(name, "/")
    player_o = bus.get_object(name, "/Player")
    tracklist_o = bus.get_object(name, "/TrackList")

    # there is only 1 interface per object
    root = dbus.Interface(root_o, "org.freedesktop.MediaPlayer")
    tracklist  = dbus.Interface(tracklist_o, "org.freedesktop.MediaPlayer")
    player = dbus.Interface(player_o, "org.freedesktop.MediaPlayer")

    # connect to the TrackChange signal
    player_o.connect_to_signal("TrackChange", TrackChange, dbus_interface="org.freedesktop.MediaPlayer")

    # determine if the Media Player is playing something
    if player.GetStatus() == 0:
        playing = True
        TrackChange(player.GetMetadata())

    # gets its identity (name and version)
    identity = root.Identity()
    window.set_title(identity)

#plays an element
def AddTrack(widget):
    mrl = e_mrl.get_text()
    if mrl != None and mrl != "":
        tracklist.AddTrack(mrl, True)
        e_mrl.set_text('')
    else:
        mrl = bt_file.get_filename()
        if mrl != None and mrl != "":
            tracklist.AddTrack("directory://" + mrl, True)
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
    root.Quit(reply_handler=(lambda *args: None), error_handler=(lambda *args: None))
    l_title.set_text("")

def Pause(widget):
    player.Pause()
    status = player.GetStatus()
    if status == 0:
        img_bt_toggle.set_from_stock(gtk.STOCK_MEDIA_PAUSE, gtk.ICON_SIZE_SMALL_TOOLBAR)
    else:
        img_bt_toggle.set_from_stock(gtk.STOCK_MEDIA_PLAY, gtk.ICON_SIZE_SMALL_TOOLBAR)
    update(0)

def Repeat(widget):
    global repeat
    repeat = not repeat
    player.Repeat(repeat)

def Shuffle(widget):
    global shuffle
    shuffle = not shuffle
    tracklist.SetRandom(shuffle)

def Loop(widget):
    global loop
    loop = not loop
    tracklist.SetLoop(loop)

# update status display
def update(widget):
    Track = player.GetMetadata()
    vol.set_value(player.VolumeGet())
    try: 
        a = Track["artist"]
    except:
        a = ""
    try:
        t = Track["title"]
    except:        
        t = ""
    if t == "":
        try:
            t = Track["URI"]
        except:
            t = ""
    l_artist.set_text(a)
    l_title.set_text(t)
    try:
        length = Track["length"]
    except:
        length = 0
    if length > 0:
        time_s.set_range(0,Track["length"])
        time_s.set_sensitive(True)
    else:
        # disable the position scale if length isn't available
        time_s.set_sensitive(False)
    GetPlayStatus(0)

# callback for volume change
def volchange(widget):
    player.VolumeSet(vol.get_value_as_int(), reply_handler=(lambda *args: None), error_handler=(lambda *args: None))

# callback for position change
def timechange(widget, x=None, y=None):
    player.PositionSet(int(time_s.get_value()), reply_handler=(lambda *args: None), error_handler=(lambda *args: None))

# refresh position change
def timeset():
    global playing
    if playing == True:
        try:
            time_s.set_value(player.PositionGet())
        except:
            playing = False
    return True

# toggle simple/full display
def expander(widget):
    if exp.get_expanded() == False:
        exp.set_label("Less")
    else:
        exp.set_label("More")

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
    global loop
    global repeat
    status = player.GetStatus()

    playing = status[0] == 0
    if playing:
        img_bt_toggle.set_from_stock("gtk-media-pause", gtk.ICON_SIZE_SMALL_TOOLBAR)
    else:
        img_bt_toggle.set_from_stock("gtk-media-play", gtk.ICON_SIZE_SMALL_TOOLBAR)
    shuffle = status[1] == 1
    bt_shuffle.set_active( shuffle )
    loop = status[2] == 1
    bt_loop.set_active( loop )
    repeat = status[3] == 1
    bt_repeat.set_active( repeat )
# loads glade file from the directory where the script is,
# so we can use /path/to/mpris.py to execute it.
import sys
xml = gtk.glade.XML(os.path.join(os.path.dirname(sys.argv[0]) , 'mpris.glade'))

# ui setup
bt_close    = xml.get_widget('close')
bt_quit     = xml.get_widget('quit')
bt_file     = xml.get_widget('ChooseFile')
bt_next     = xml.get_widget('next')
bt_prev     = xml.get_widget('prev')
bt_stop     = xml.get_widget('stop')
bt_toggle   = xml.get_widget('toggle')
bt_mrl      = xml.get_widget('AddMRL')
bt_shuffle  = xml.get_widget('shuffle')
bt_repeat   = xml.get_widget('repeat')
bt_loop     = xml.get_widget('loop')
l_artist    = xml.get_widget('l_artist')
l_title     = xml.get_widget('l_title')
e_mrl       = xml.get_widget('mrl')
window      = xml.get_widget('window1')
img_bt_toggle=xml.get_widget('image6')
exp         = xml.get_widget('expander2')
expvbox     = xml.get_widget('expandvbox')
audioicon   = xml.get_widget('eventicon')
vol         = xml.get_widget('vol')
time_s      = xml.get_widget('time_s')
time_l      = xml.get_widget('time_l')

# connect to the different callbacks

window.connect('delete_event',  delete_event)
window.connect('destroy',       destroy)
window.connect('key_release_event', key_release)

tray = gtk.status_icon_new_from_icon_name("audio-x-generic")
tray.connect('activate', tray_button)

bt_close.connect('clicked',     destroy)
bt_quit.connect('clicked',      Quit)
bt_mrl.connect('clicked',       AddTrack)
bt_toggle.connect('clicked',    Pause)
bt_next.connect('clicked',      Next)
bt_prev.connect('clicked',      Prev)
bt_stop.connect('clicked',      Stop)
bt_loop.connect('clicked',      Loop)
bt_repeat.connect('clicked',    Repeat)
bt_shuffle.connect('clicked',   Shuffle)
exp.connect('activate',         expander)
vol.connect('changed',          volchange)
time_s.connect('adjust-bounds', timechange)
audioicon.set_events(gtk.gdk.BUTTON_PRESS_MASK) # hack for the bottom right icon
audioicon.connect('button_press_event', icon_clicked) 
time_s.set_update_policy(gtk.UPDATE_DISCONTINUOUS)

library = "/media/mp3" # editme

# set the Directory chooser to a default location
try:
    os.chdir(library)
    bt_file.set_current_folder(library)
except:
    bt_file.set_current_folder(os.path.expanduser("~"))

# connect to the bus
bus = dbus.SessionBus()
dbus_names = bus.get_object( "org.freedesktop.DBus", "/org/freedesktop/DBus" )
dbus_names.connect_to_signal("NameOwnerChanged", NameOwnerChanged, dbus_interface="org.freedesktop.DBus") # to detect new Media Players

dbus_o = bus.get_object("org.freedesktop.DBus", "/")
dbus_intf = dbus.Interface(dbus_o, "org.freedesktop.DBus")
name_list = dbus_intf.ListNames()

# connect to the first Media Player found
for n in name_list:
    if "org.mpris." in n:
        Connect(n)
        window.set_title(identity)
        vol.set_value(player.VolumeGet())
        update(0)
        break

# run a timer to update position
gobject.timeout_add( 1000, timeset)

window.set_icon_name('audio-x-generic')
window.show()

icon_theme = gtk.icon_theme_get_default()
try:
    pix = icon_theme.load_icon("audio-x-generic",24,0)
    window.set_icon(pix)
except:
    True

win_position = window.get_position()

gtk.main() # execute the main loop
