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
        self.assertEqual(vlc.EventType.MediaStateChanged.value, 5)

    def test_enum_meta(self):
        self.assertEqual(vlc.Meta.Description.value, 6)

    def test_enum_state(self):
        self.assertEqual(vlc.State.Playing.value, 3)

    def test_enum_media_option(self):
        self.assertEqual(vlc.MediaOption.unique.value, 256)

    def test_enum_playback_mode(self):
        self.assertEqual(vlc.PlaybackMode.repeat.value, 2)

    def test_enum_marquee_int_option(self):
        self.assertEqual(vlc.VideoMarqueeIntOption.Size.value, 5)

    def test_enum_output_device_type(self):
        self.assertEqual(vlc.AudioOutputDeviceTypes._2F2R.value, 4)

    def test_enum_output_channel(self):
        self.assertEqual(vlc.AudioOutputChannel.Dolbys.value, 5)

    def test_enum_position_origin(self):
        self.assertEqual(vlc.PositionOrigin.ModuloPosition.value, 2)

    def test_enum_position_key(self):
        self.assertEqual(vlc.PositionKey.MediaTime.value, 2)

    def test_enum_player_status(self):
        self.assertEqual(vlc.PlayerStatus.StopStatus.value, 5)

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

    def test_mediacontrol_position(self):
        p=vlc.MediaControlPosition(value=2,
                                   origin=vlc.PositionOrigin.RelativePosition,
                                   key=vlc.PositionKey.MediaTime)
        self.assertEqual(p.value, 2)

    def test_mediacontrol_position_shortcut(self):
        p=vlc.MediaControlPosition(2)
        self.assertEqual(p.value, 2)
        self.assertEqual(p.key, vlc.PositionKey.MediaTime)
        self.assertEqual(p.origin, vlc.PositionOrigin.AbsolutePosition)

    def test_mediacontrol_get_media_position(self):
        mc=vlc.MediaControl()
        p=mc.get_media_position()
        self.assertEqual(p.value, -1)

    def test_mediacontrol_get_stream_information(self):
        mc=vlc.MediaControl()
        s=mc.get_stream_information()
        self.assertEqual(s.position, 0)
        self.assertEqual(s.length, 0)

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

    def test_libvlc_player_state(self):
        mrl='/tmp/foo.avi'
        i=vlc.Instance()
        p=i.media_player_new(mrl)
        self.assertEqual(p.get_state(), vlc.State.Ended)

    def test_libvlc_logger(self):
        i=vlc.Instance()
        l=i.log_open()
        l.clear()
        self.assertEqual(l.count(), 0)
        l.close()

    def test_libvlc_logger_clear(self):
        i=vlc.Instance()
        l=i.log_open()
        l.clear()
        self.assertEqual(l.count(), 0)
        l.close()

    def test_libvlc_logger(self):
        i=vlc.Instance()
        i.set_log_verbosity(3)
        l=i.log_open()
        # This should generate a log message
        i.add_intf('dummy')
        self.assertNotEqual(l.count(), 0)
        for m in l:
            # Ensure that messages can be read.
            self.assertNotEqual(len(m.message), 0)
        l.close()

if __name__ == '__main__':
    unittest.main()
