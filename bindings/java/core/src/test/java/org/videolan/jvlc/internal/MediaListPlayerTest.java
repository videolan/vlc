/*****************************************************************************
 * MediaListPlayerTest.java: VLC Java Bindings
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

import java.io.File;

import junit.framework.Assert;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.videolan.jvlc.internal.LibVlc.LibVlcInstance;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaDescriptor;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaInstance;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaList;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaListPlayer;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;


public class MediaListPlayerTest
{

    private LibVlc libvlc = LibVlc.SYNC_INSTANCE;

    private LibVlcInstance libvlcInstance;

    private String mrl = this.getClass().getResource("/raffa_voice.ogg").getPath();

    @Before
    public void testSetup() throws Exception
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlcInstance = libvlc.libvlc_new(0, new String[]{"-A","file","--audiofile-file=" + File.createTempFile("jvlc", ".wav").getAbsolutePath()}, exception);
        // use the following line to use your audio card.
        // libvlcInstance = libvlc.libvlc_new(0, new String[]{}, exception);
    }

    @After
    public void tearDown()
    {
        libvlc.libvlc_release(libvlcInstance);
    }

    @Test
    public void mediaListPlayerNewTest()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        Assert.assertNotNull(mediaListPlayer);
        Assert.assertEquals(0, exception.raised);
    }

    @Test
    public void mediaListPlayerSetMediaListTest()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        libvlc.libvlc_media_list_player_set_media_list(mediaListPlayer, mediaList, exception);
        Assert.assertEquals(0, exception.raised);
    }

    @Test
    public void mediaListPlayerSetMediaListTest2()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        LibVlcMediaDescriptor mediaDescriptor = libvlc.libvlc_media_descriptor_new(libvlcInstance, mrl, exception);
        libvlc.libvlc_media_list_add_media_descriptor(mediaList, mediaDescriptor, exception);
        libvlc.libvlc_media_list_player_set_media_list(mediaListPlayer, mediaList, exception);
        Assert.assertEquals(0, exception.raised);
    }

    @Test
    public void mediaListPlayerIsNotPlayingTest()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        int result = libvlc.libvlc_media_list_player_is_playing(mediaListPlayer, exception);
        Assert.assertEquals(0, result);
        Assert.assertEquals(0, exception.raised);
    }

    @Test
    public void mediaListPlayerPlayNoItemTest()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        libvlc.libvlc_media_list_player_play(mediaListPlayer, exception);
        Assert.assertEquals(1, exception.raised);
    }

    /**
     * this fails: see https://trac.videolan.org/vlc/ticket/1527
     */
//    @Test
    public void mediaListPlayerPlay()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        LibVlcMediaDescriptor mediaDescriptor = libvlc.libvlc_media_descriptor_new(libvlcInstance, mrl, exception);
        libvlc.libvlc_media_list_add_media_descriptor(mediaList, mediaDescriptor, exception);
        libvlc.libvlc_media_list_player_set_media_list(mediaListPlayer, mediaList, exception);
        libvlc.libvlc_media_list_player_play(mediaListPlayer, exception);
        Assert.assertEquals("Exception message: " + exception.message + ".\n", 0, exception.raised);
    }

    @Test
    public void mediaListPlayerPlayItemAtIndex() throws Exception
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        LibVlcMediaDescriptor mediaDescriptor = libvlc.libvlc_media_descriptor_new(libvlcInstance, mrl, exception);
        libvlc.libvlc_media_list_add_media_descriptor(mediaList, mediaDescriptor, exception);
        libvlc.libvlc_media_list_player_set_media_list(mediaListPlayer, mediaList, exception);
        libvlc.libvlc_media_list_player_play_item_at_index(mediaListPlayer, 0, exception);
        while (true)
        {
            int playing = libvlc.libvlc_media_list_player_is_playing(mediaListPlayer, exception);
            if (exception.raised == 1)
            {
                throw new RuntimeException("Native exception thrown");
            }
            if (playing == 1)
            {
                break;
            }
            Thread.sleep(150);
        }
        libvlc.libvlc_media_list_player_stop(mediaListPlayer, exception);

    }

    @Test
    public void mediaListPlayerPlayItem() throws Exception
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        LibVlcMediaDescriptor mediaDescriptor = libvlc.libvlc_media_descriptor_new(libvlcInstance, mrl, exception);
        libvlc.libvlc_media_list_add_media_descriptor(mediaList, mediaDescriptor, exception);
        libvlc.libvlc_media_list_player_set_media_list(mediaListPlayer, mediaList, exception);
        libvlc.libvlc_media_list_player_play_item(mediaListPlayer, mediaDescriptor, exception);
        Assert.assertEquals(0, exception.raised);
        while (true)
        {
            int playing = libvlc.libvlc_media_list_player_is_playing(mediaListPlayer, exception);
            if (exception.raised == 1)
            {
                throw new RuntimeException("Native exception thrown");
            }
            if (playing == 1)
            {
                break;
            }
            Thread.sleep(150);
        }
        libvlc.libvlc_media_list_player_stop(mediaListPlayer, exception);
    }

    @Test
    public void mediaListPlayerPause() throws Exception
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        LibVlcMediaDescriptor mediaDescriptor = libvlc.libvlc_media_descriptor_new(libvlcInstance, mrl, exception);
        libvlc.libvlc_media_list_add_media_descriptor(mediaList, mediaDescriptor, exception);
        libvlc.libvlc_media_list_player_set_media_list(mediaListPlayer, mediaList, exception);
        libvlc.libvlc_media_list_player_play_item(mediaListPlayer, mediaDescriptor, exception);
        libvlc.libvlc_media_list_player_pause(mediaListPlayer, exception);
        Assert.assertEquals(0, exception.raised);
        while (true)
        {
            int playing = libvlc.libvlc_media_list_player_is_playing(mediaListPlayer, exception);
            if (exception.raised == 1)
            {
                throw new RuntimeException("Native exception thrown");
            }            
            if (playing == 0)
            {
                break;
            }
            Thread.sleep(150);
        }
        int state = libvlc.libvlc_media_list_player_get_state(mediaListPlayer, exception);
        Assert.assertEquals("Expected state: " + LibVlcState.libvlc_Paused +".\n", LibVlcState.libvlc_Paused.ordinal(), state);
        libvlc.libvlc_media_list_player_stop(mediaListPlayer, exception);
    }

    @Test
    public void mediaListPlayerGetStateStopped()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        int state = libvlc.libvlc_media_list_player_get_state(mediaListPlayer, exception);
        Assert.assertEquals(LibVlcState.libvlc_Stopped.ordinal(), state);
    }
    
    @Test
    public void mediaListPlayerSetMediaInstance()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        LibVlcMediaDescriptor md = libvlc.libvlc_media_descriptor_new(libvlcInstance, mrl, exception);
        LibVlcMediaInstance mi = libvlc.libvlc_media_instance_new_from_media_descriptor(md, exception);
        libvlc.libvlc_media_list_player_set_media_instance(mediaListPlayer, mi, exception);
        Assert.assertEquals(0, exception.raised);
    }
    
    @Test
    public void mediaListPlayerNextNoItems()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        libvlc.libvlc_media_list_player_next(mediaListPlayer, exception);
        Assert.assertEquals(1, exception.raised);
    }
    
    /**
     * fails, see https://trac.videolan.org/vlc/ticket/1535
     */
//    @Test
    public void mediaListPlayerNext() throws Exception
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        LibVlcMediaDescriptor mediaDescriptor = libvlc.libvlc_media_descriptor_new(libvlcInstance, mrl, exception);
        libvlc.libvlc_media_list_add_media_descriptor(mediaList, mediaDescriptor, exception);
        libvlc.libvlc_media_list_add_media_descriptor(mediaList, mediaDescriptor, exception);
        libvlc.libvlc_media_list_player_set_media_list(mediaListPlayer, mediaList, exception);
        libvlc.libvlc_media_list_player_play_item_at_index(mediaListPlayer, 0, exception);
        Thread.sleep(150);
        libvlc.libvlc_media_list_player_next(mediaListPlayer, exception);
        Assert.assertEquals(0, exception.raised);
    }

    @Test
    public void mediaListPlayerIsPlaying() throws Exception
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        LibVlcMediaDescriptor mediaDescriptor = libvlc.libvlc_media_descriptor_new(libvlcInstance, mrl, exception);
        libvlc.libvlc_media_list_add_media_descriptor(mediaList, mediaDescriptor, exception);
        libvlc.libvlc_media_list_player_set_media_list(mediaListPlayer, mediaList, exception);
        libvlc.libvlc_media_list_player_play_item(mediaListPlayer, mediaDescriptor, exception);

        while (true)
        {
            int playing = libvlc.libvlc_media_list_player_is_playing(mediaListPlayer, exception);
            Assert.assertEquals(0, exception.raised);
            if (playing == 1)
            {
                break;
            }
            Thread.sleep(150);
        }
        Assert.assertEquals("Expected state: " + LibVlcState.libvlc_Playing +".\n", LibVlcState.libvlc_Playing.ordinal(), libvlc.libvlc_media_list_player_get_state(
            mediaListPlayer,
            exception));
        
        libvlc.libvlc_media_list_player_stop(mediaListPlayer, exception);
        while (true)
        {
            int playing = libvlc.libvlc_media_list_player_is_playing(mediaListPlayer, exception);
            Assert.assertEquals(0, exception.raised);
            if (playing == 0)
            {
                break;
            }
            Thread.sleep(150);
        }
        Assert.assertEquals(LibVlcState.libvlc_Stopped.ordinal(), libvlc.libvlc_media_list_player_get_state(
            mediaListPlayer,
            exception));
    }



}
