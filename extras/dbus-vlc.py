#!/usr/bin/python
# -*- coding: utf8 -*-
#
# Copyright (C) 2006 Rafaël Carré <funman at videolanorg>
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

import dbus
import dbus.glib
import gtk
import gtk.glade
import egg.trayicon
import gobject
import os

global position
global timer
#global playing

def itemchange_handler(item):
    gobject.timeout_add( 2000, timeset)
    l_item.set_text(item)

bus = dbus.SessionBus()
player_o = bus.get_object("org.freedesktop.MediaPlayer", "/Player")
tracklist_o = bus.get_object("org.freedesktop.MediaPlayer", "/TrackList")

tracklist  = dbus.Interface(tracklist_o, "org.freedesktop.MediaPlayer")
player = dbus.Interface(player_o, "org.freedesktop.MediaPlayer")
try:
    player_o.connect_to_signal("TrackChange", itemchange_handler, dbus_interface="org.freedesktop.MediaPlayer")
except:
    True

def AddTrack(widget):
    mrl = e_mrl.get_text()
    if mrl != None and mrl != "":
        tracklist.AddTrack(mrl, True)
    else:
        mrl = bt_file.get_filename()
        if mrl != None and mrl != "":
            tracklist.AddTrack("directory://" + mrl, True)

def Next(widget):
    player.Next(reply_handler=(lambda *args: None), error_handler=(lambda *args: None))
    update(0)

def Prev(widget):
    player.Prev(reply_handler=(lambda *args: None), error_handler=(lambda *args: None))
    update(0)

def Stop(widget):
    player.Stop(reply_handler=(lambda *args: None), error_handler=(lambda *args: None))
    update(0)

def update(widget):
#    itemchange_handler(str(player.GetPlayingItem()))
    vol.set_value(player.VolumeGet())
    GetPlayStatus(0)

def GetPlayStatus(widget):
    global playing
    status = str(player.GetStatus())
    if status == 0:
        img_bt_toggle.set_from_stock("gtk-media-pause", gtk.ICON_SIZE_SMALL_TOOLBAR)
        playing = True
    else:
        img_bt_toggle.set_from_stock("gtk-media-play", gtk.ICON_SIZE_SMALL_TOOLBAR)
        playing = False

def Quit(widget):
    player.Quit(reply_handler=(lambda *args: None), error_handler=(lambda *args: None))
    l_item.set_text("")

def Pause(widget):
    player.Pause()
#        img_bt_toggle.set_from_stock(gtk.STOCK_MEDIA_PAUSE, gtk.ICON_SIZE_SMALL_TOOLBAR)
#        img_bt_toggle.set_from_stock(gtk.STOCK_MEDIA_PLAY, gtk.ICON_SIZE_SMALL_TOOLBAR)
    update(0)

def volchange(widget, data):
    player.VolumeSet(vol.get_value_as_int(), reply_handler=(lambda *args: None), error_handler=(lambda *args: None))

def timechange(widget, x=None, y=None):
    player.PositionSet(time_s.get_value(), reply_handler=(lambda *args: None), error_handler=(lambda *args: None))

def timeset():
#    global playing
    time_s.set_value(player.PositionGet())
#    return playing

def expander(widget):
    if exp.get_expanded() == False:
        exp.set_label("Less")
    else:
        exp.set_label("More")

def delete_event(self, widget):
    widget.hide()
    return True

def destroy(widget):
    gtk.main_quit()

def key_release(widget, event):
    global position
    if event.keyval == gtk.keysyms.Escape:
        position = window.get_position()
        widget.hide()

def tray_button(widget,event):
    global position
    if event.button == 1:
        if window.get_property('visible'):
            position = window.get_position()
            window.hide()
        else:
            window.move(position[0], position[1])
            window.show()
    if event.button == 3:
        menu.popup(None,None,None,event.button,event.time)

xml = gtk.glade.XML('dbus-vlc.glade')

bt_close    = xml.get_widget('close')
bt_quit     = xml.get_widget('quit')
bt_file     = xml.get_widget('ChooseFile')
bt_mrl      = xml.get_widget('AddMRL')
bt_next     = xml.get_widget('next')
bt_prev     = xml.get_widget('prev')
bt_stop     = xml.get_widget('stop')
bt_toggle   = xml.get_widget('toggle')
l_item      = xml.get_widget('item')
e_mrl       = xml.get_widget('mrl')
window      = xml.get_widget('window1')
img_bt_toggle=xml.get_widget('image6')
exp         = xml.get_widget('expander2')
expvbox     = xml.get_widget('expandvbox')
menu        = xml.get_widget('menu1')
menuitem    = xml.get_widget('menuquit')
vlcicon     = xml.get_widget('eventicon')
vol         = xml.get_widget('vol')
time_s      = xml.get_widget('time_s')
time_l      = xml.get_widget('time_l')

window.connect('delete_event',  delete_event)
window.connect('destroy',       destroy)
window.connect('key_release_event', key_release)

tray = egg.trayicon.TrayIcon("VLC")
eventbox = gtk.EventBox()
tray.add(eventbox)
eventbox.set_events(gtk.gdk.BUTTON_PRESS_MASK)
eventbox.connect('button_press_event', tray_button)
image = gtk.Image()
eventbox.add(image)
image.set_from_icon_name("vlc", gtk.ICON_SIZE_MENU)
tray.show_all()

def icon_clicked(widget, event):
    update(0)

menu.attach_to_widget(eventbox,None)

bt_close.connect('clicked',     destroy)
bt_quit.connect('clicked',      Quit)
bt_mrl.connect('clicked',       AddTrack)
bt_toggle.connect('clicked',    Pause)
bt_next.connect('clicked',      Next)
bt_prev.connect('clicked',      Prev)
bt_stop.connect('clicked',      Stop)
exp.connect('activate',         expander)
menuitem.connect('activate',    destroy)
vlcicon.set_events(gtk.gdk.BUTTON_PRESS_MASK)
vlcicon.connect('button_press_event', icon_clicked)
vol.connect('change-value',     volchange)
vol.connect('scroll-event',     volchange)
time_s.connect('adjust-bounds', timechange)

time_s.set_update_policy(gtk.UPDATE_DISCONTINUOUS)
gobject.timeout_add( 2000, timeset)

library = "/media/mp3"

try:
    os.chdir(library)
    bt_file.set_current_folder(library)
except:
    print "edit this file to point to your media library"

window.set_icon_name('vlc')
window.set_title("VLC - D-Bus ctrl")
window.show()

try:
    update(0)
except:
    True

gtk.main()
