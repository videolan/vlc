#! /usr/bin/python

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

"""Unittest module.
"""

import unittest
import vlc

class TestVLCAPI(unittest.TestCase):
    #def setUp(self):
    #    self.seq = range(10)
    #self.assert_(element in self.seq)

    # We check enum definitions against hardcoded values. In case of
    # failure, check that the reason is not a change in the .h
    # definitions.
    def test_enum_event_type(self):
        self.assertEqual(vlc.EventType.MediaStateChanged, 5)

    def test_enum_meta(self):
        self.assertEqual(vlc.Meta.Description, 6)

    def test_enum_state(self):
        self.assertEqual(vlc.State.Playing, 3)

    def test_enum_media_option(self):
        self.assertEqual(vlc.MediaOption.unique, 256)

    def test_enum_playback_mode(self):
        self.assertEqual(vlc.PlaybackMode.repeat, 2)

    def test_enum_marquee_int_option(self):
        self.assertEqual(vlc.VideoMarqueeIntOption.Size, 5)

    def test_enum_output_device_type(self):
        self.assertEqual(vlc.AudioOutputDeviceTypes._2F2R, 4)

    def test_enum_output_channel(self):
        self.assertEqual(vlc.AudioOutputChannel.Dolbys, 5)

    def test_enum_position_origin(self):
        self.assertEqual(vlc.PositionOrigin.ModuloPosition, 2)

    def test_enum_position_key(self):
        self.assertEqual(vlc.PositionKey.MediaTime, 2)

    def test_enum_player_status(self):
        self.assertEqual(vlc.PlayerStatus.StopStatus, 5)

    # Basic MediaControl tests
    def test_mediacontrol_creation(self):
        mc=vlc.MediaControl()
        self.assert_(mc)

    def test_mediacontrol_initial_mrl(self):
        mc=vlc.MediaControl()
        self.assertEqual(mc.get_mrl(), '')

    def test_mediacontrol_set_mrl(self):
        mrl='/tmp/foo.avi'
        mc=vlc.MediaControl()
        mc.set_mrl(mrl)
        self.assertEqual(mc.get_mrl(), mrl)

    # Basic libvlc tests
    def test_instance_creation(self):
        i=vlc.Instance()
        self.assert_(i)

    def test_libvlc_media(self):
        mrl='/tmp/foo.avi'
        i=vlc.Instance()
        m=i.media_new(mrl)
        self.assertEqual(m.get_mrl(), mrl)

    def test_libvlc_player(self):
        mrl='/tmp/foo.avi'
        i=vlc.Instance()
        p=i.media_player_new(mrl)
        self.assertEqual(p.get_media().get_mrl(), mrl)

if __name__ == '__main__':
    unittest.main()
