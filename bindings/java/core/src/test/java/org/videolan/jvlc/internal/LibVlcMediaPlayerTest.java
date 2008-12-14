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
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaInstance;


public class LibVlcMediaPlayerTest extends AbstractVLCInternalTest
{
    @Test
    public void mediaInstanceNew()
    {
        LibVlcMediaInstance instance = libvlc.libvlc_media_player_new(libvlcInstance, exception);
        Assert.assertNotNull(instance);
        Assert.assertEquals(0, exception.raised);
    }
    
    @Test
    public void mediaInstancePlayBad()
    {
        LibVlcMediaInstance instance = libvlc.libvlc_media_player_new(libvlcInstance, exception);
        libvlc.libvlc_media_player_play(instance, exception);
        Assert.assertEquals(1, exception.raised); // no associated media descriptor
    }
    
    @Test
    public void mediaInstancePlay()
    {
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMediaInstance mi = libvlc.libvlc_media_player_new_from_media(md, exception);
        libvlc.libvlc_media_player_play(mi, exception);
        Assert.assertEquals(0, exception.raised);
    }
    
    @Test
    public void mediaInstancePauseBad()
    {
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMediaInstance mi = libvlc.libvlc_media_player_new_from_media(md, exception);
        libvlc.libvlc_media_player_pause(mi, exception);
        Assert.assertEquals(1, exception.raised);
    }
    
    @Test
    public void mediaInstancePause()
    {
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMediaInstance mi = libvlc.libvlc_media_player_new_from_media(md, exception);
        libvlc.libvlc_media_player_play(mi, exception);
        libvlc.libvlc_media_player_pause(mi, exception);
        Assert.assertEquals(0, exception.raised);
    }
    
    @Test
    public void mediaInstanceSetPosition()
    {
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMediaInstance mi = libvlc.libvlc_media_player_new_from_media(md, exception);
        libvlc.libvlc_media_player_play(mi, exception);
        libvlc.libvlc_media_player_set_position(mi, 0.5f, exception);
        Assert.assertEquals(0, exception.raised);
        float position = libvlc.libvlc_media_player_get_position(mi, exception);
        Assert.assertTrue("Position is: " + position, position >= 0.5f);
    }
    
    @Test
    public void mediaInstanceStop()
    {
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMediaInstance mi = libvlc.libvlc_media_player_new_from_media(md, exception);
        libvlc.libvlc_media_player_stop(mi, exception);
        Assert.assertEquals(0, exception.raised);
    }
    
    @Test
    public void mediaInstanceStop2() throws Exception
    {
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMediaInstance mi = libvlc.libvlc_media_player_new_from_media(md, exception);
        libvlc.libvlc_media_player_play(mi, exception);
        Thread.sleep(100);
        libvlc.libvlc_media_player_stop(mi, exception);
        Thread.sleep(500);
        Assert.assertEquals(0, exception.raised);
    }
    
}
