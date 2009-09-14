/*****************************************************************************
 * LibVlcMediaInstanceTest.java: VLC Java Bindings
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 *
 * Authors: Filippo Carone <filippo@carone.org>
 *
 *
 * $Id $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

package org.videolan.jvlc.internal;

import org.junit.Assert;
import org.junit.Test;
import org.videolan.jvlc.internal.LibVlc.LibVlcMedia;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaPlayer;


public class LibVlcMediaPlayerTest extends AbstractVLCInternalTest
{
    @Test
    public void mediaPlayerNew()
    {
        LibVlcMediaPlayer instance = libvlc.libvlc_media_player_new(libvlcInstance, exception);
        Assert.assertNotNull(instance);
        Assert.assertEquals(0, exception.b_raised);
    }
    
    @Test
    public void mediaPlayerPlayBad()
    {
        LibVlcMediaPlayer instance = libvlc.libvlc_media_player_new(libvlcInstance, exception);
        libvlc.libvlc_media_player_play(instance, exception);
        Assert.assertEquals(1, exception.b_raised); // no associated media descriptor
    }
    
    @Test
    public void mediaPlayerPlay()
    {
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMediaPlayer mi = libvlc.libvlc_media_player_new_from_media(md, exception);
        libvlc.libvlc_media_player_play(mi, exception);
        Assert.assertEquals(0, exception.b_raised);
    }
    
    @Test
    public void mediaPlayerIsPlaying() throws Exception
    {
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMediaPlayer mi = libvlc.libvlc_media_player_new_from_media(md, exception);
        Assert.assertEquals(0, libvlc.libvlc_media_player_is_playing(mi, exception));
        libvlc.libvlc_media_player_play(mi, exception);
        Assert.assertEquals(0, exception.b_raised);
        Thread.sleep(200);
        Assert.assertEquals(1, libvlc.libvlc_media_player_is_playing(mi, exception));
    }
    
    @Test
    public void mediaPlayerPauseBad()
    {
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMediaPlayer mi = libvlc.libvlc_media_player_new_from_media(md, exception);
        libvlc.libvlc_media_player_pause(mi, exception);
        Assert.assertEquals(1, exception.b_raised);
    }
    
    @Test
    public void mediaPlayerPause()
    {
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMediaPlayer mi = libvlc.libvlc_media_player_new_from_media(md, exception);
        libvlc.libvlc_media_player_play(mi, exception);
        libvlc.libvlc_media_player_pause(mi, exception);
        Assert.assertEquals(0, exception.b_raised);
    }
    
    @Test
    public void mediaPlayerSetPosition()
    {
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMediaPlayer mi = libvlc.libvlc_media_player_new_from_media(md, exception);
        libvlc.libvlc_media_player_play(mi, exception);
        libvlc.libvlc_media_player_set_position(mi, 0.5f, exception);
        Assert.assertEquals(0, exception.b_raised);
        float position = libvlc.libvlc_media_player_get_position(mi, exception);
        Assert.assertTrue("Position is: " + position, position >= 0.5f);
    }
    
    @Test
    public void mediaPlayerStop()
    {
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMediaPlayer mi = libvlc.libvlc_media_player_new_from_media(md, exception);
        libvlc.libvlc_media_player_stop(mi, exception);
        Assert.assertEquals(0, exception.b_raised);
    }
    
    @Test(timeout = 2000L)
    public void mediaPlayerStop2() throws Exception
    {
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMediaPlayer mi = libvlc.libvlc_media_player_new_from_media(md, exception);
        libvlc.libvlc_media_player_play(mi, exception);
        Thread.sleep(100);
        libvlc.libvlc_media_player_stop(mi, exception);
        Thread.sleep(500);
        Assert.assertEquals(0, exception.b_raised);
    }
    
}
