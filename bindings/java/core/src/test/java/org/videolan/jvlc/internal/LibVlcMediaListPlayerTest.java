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

import junit.framework.Assert;

import org.junit.After;
import org.junit.Test;
import org.videolan.jvlc.internal.LibVlc.LibVlcMedia;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaPlayer;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaList;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaListPlayer;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;


public class LibVlcMediaListPlayerTest extends AbstractVLCInternalTest
{

    private LibVlcMediaListPlayer current;

    @Test
    public void mediaListPlayerNewTest()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        Assert.assertNotNull(mediaListPlayer);
        Assert.assertEquals(0, exception.b_raised);
        libvlc.libvlc_media_list_player_release(mediaListPlayer);
    }

    @Test
    public void mediaListPlayerSetMediaListTest()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        libvlc.libvlc_media_list_player_set_media_list(mediaListPlayer, mediaList, exception);
        Assert.assertEquals(0, exception.b_raised);
        libvlc.libvlc_media_list_release(mediaList);
        libvlc.libvlc_media_list_player_release(mediaListPlayer);
    }

    @Test
    public void mediaListPlayerSetMediaListTest2()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        LibVlcMedia mediaDescriptor = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        libvlc.libvlc_media_list_add_media(mediaList, mediaDescriptor, exception);
        libvlc.libvlc_media_list_player_set_media_list(mediaListPlayer, mediaList, exception);
        Assert.assertEquals(0, exception.b_raised);
        libvlc.libvlc_media_list_release(mediaList);
        libvlc.libvlc_media_list_player_release(mediaListPlayer);
    }

    @Test
    public void mediaListPlayerIsNotPlayingTest()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        int result = libvlc.libvlc_media_list_player_is_playing(mediaListPlayer, exception);
        Assert.assertEquals(0, result);
        Assert.assertEquals(0, exception.b_raised);
        libvlc.libvlc_media_list_player_release(mediaListPlayer);
    }

    @Test
    public void mediaListPlayerPlayNoItemTest()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        libvlc.libvlc_media_list_player_play(mediaListPlayer, exception);
        Assert.assertEquals(1, exception.b_raised);
        libvlc.libvlc_media_list_player_release(mediaListPlayer);
    }

    @Test
    public void mediaListPlayerPlay()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        current = mediaListPlayer;
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        LibVlcMedia mediaDescriptor = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        libvlc.libvlc_media_list_add_media(mediaList, mediaDescriptor, exception);
        libvlc.libvlc_media_list_player_set_media_list(mediaListPlayer, mediaList, exception);
        libvlc.libvlc_media_list_player_play(mediaListPlayer, exception);
        Assert.assertEquals(0, exception.b_raised);
        libvlc.libvlc_media_release(mediaDescriptor);
        libvlc.libvlc_media_list_release(mediaList);
    }

    @Test
    public void mediaListPlayerPlayItemAtIndex() throws Exception
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        LibVlcMedia mediaDescriptor = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        libvlc.libvlc_media_list_add_media(mediaList, mediaDescriptor, exception);
        libvlc.libvlc_media_list_player_set_media_list(mediaListPlayer, mediaList, exception);
        libvlc.libvlc_media_list_player_play_item_at_index(mediaListPlayer, 0, exception);
        while (true)
        {
            int playing = libvlc.libvlc_media_list_player_is_playing(mediaListPlayer, exception);
            if (exception.b_raised == 1)
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
        while (libvlc.libvlc_media_list_player_get_state(mediaListPlayer, exception) != LibVlcState.libvlc_Ended
            .ordinal())
        {
            Thread.sleep(100);
        }
        libvlc.libvlc_media_release(mediaDescriptor);
        libvlc.libvlc_media_list_release(mediaList);
        libvlc.libvlc_media_list_player_release(mediaListPlayer);
    }

    @Test
    public void mediaListPlayerPlayItem() throws Exception
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        LibVlcMedia mediaDescriptor = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        libvlc.libvlc_media_list_add_media(mediaList, mediaDescriptor, exception);
        libvlc.libvlc_media_list_player_set_media_list(mediaListPlayer, mediaList, exception);
        libvlc.libvlc_media_list_player_play_item(mediaListPlayer, mediaDescriptor, exception);
        Assert.assertEquals(0, exception.b_raised);
        while (true)
        {
            int playing = libvlc.libvlc_media_list_player_is_playing(mediaListPlayer, exception);
            if (exception.b_raised == 1)
            {
                throw new RuntimeException("Native exception thrown");
            }
            if (playing == 1)
            {
                break;
            }
            Thread.sleep(150);
        }
        Thread.sleep(400);
        libvlc.libvlc_media_list_player_stop(mediaListPlayer, exception);
        libvlc.libvlc_media_list_release(mediaList);
        libvlc.libvlc_media_list_player_release(mediaListPlayer);
    }

    @Test
    public void mediaListPlayerGetStateEnded()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        int state = libvlc.libvlc_media_list_player_get_state(mediaListPlayer, exception);
        Assert.assertEquals(LibVlcState.libvlc_Ended.ordinal(), state);
        libvlc.libvlc_media_list_player_release(mediaListPlayer);
    }

    @Test
    public void mediaLtistPlayerPause() throws Exception
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        LibVlcMedia mediaDescriptor = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        libvlc.libvlc_media_list_add_media(mediaList, mediaDescriptor, exception);
        libvlc.libvlc_media_list_player_set_media_list(mediaListPlayer, mediaList, exception);
        libvlc.libvlc_media_list_player_play_item(mediaListPlayer, mediaDescriptor, exception);
        while (true)
        {
            int playing = libvlc.libvlc_media_list_player_is_playing(mediaListPlayer, exception);
            if (exception.b_raised == 1)
            {
                throw new RuntimeException("Native exception thrown");
            }
            if (playing == 1)
            {
                break;
            }
            Thread.sleep(150);
        }
        libvlc.libvlc_media_list_player_pause(mediaListPlayer, exception);

        int state = libvlc.libvlc_media_list_player_get_state(mediaListPlayer, exception);
        Assert.assertEquals(0, exception.b_raised);
        Thread.sleep(200L);
        Assert.assertEquals(
            "Expected state: " + LibVlcState.libvlc_Paused + ".\n",
            LibVlcState.libvlc_Paused.ordinal(),
            state);
        libvlc.libvlc_media_list_player_stop(mediaListPlayer, exception);
        libvlc.libvlc_media_list_release(mediaList);
        libvlc.libvlc_media_list_player_release(mediaListPlayer);
    }

    @Test
    public void mediaListPlayerSetMediaInstance()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMediaPlayer mi = libvlc.libvlc_media_player_new_from_media(md, exception);
        libvlc.libvlc_media_list_player_set_media_player(mediaListPlayer, mi, exception);
        Assert.assertEquals(0, exception.b_raised);
    }

    @Test
    public void mediaListPlayerNextNoItems()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        libvlc.libvlc_media_list_player_next(mediaListPlayer, exception);
        Assert.assertEquals(1, exception.b_raised);
    }

    /**
     * fails, see https://trac.videolan.org/vlc/ticket/1535
     */
    // @Test
    public void mediaListPlayerNext() throws Exception
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        LibVlcMedia mediaDescriptor = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        libvlc.libvlc_media_list_add_media(mediaList, mediaDescriptor, exception);
        libvlc.libvlc_media_list_add_media(mediaList, mediaDescriptor, exception);
        libvlc.libvlc_media_list_player_set_media_list(mediaListPlayer, mediaList, exception);
        libvlc.libvlc_media_list_player_play_item_at_index(mediaListPlayer, 0, exception);
        Thread.sleep(150);
        libvlc.libvlc_media_list_player_next(mediaListPlayer, exception);
        Assert.assertEquals(0, exception.b_raised);
        libvlc.libvlc_media_list_release(mediaList);
    }

    @Test
    public void mediaListPlayerIsPlaying() throws Exception
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaListPlayer mediaListPlayer = libvlc.libvlc_media_list_player_new(libvlcInstance, exception);
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        LibVlcMedia mediaDescriptor = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        libvlc.libvlc_media_list_add_media(mediaList, mediaDescriptor, exception);
        libvlc.libvlc_media_list_player_set_media_list(mediaListPlayer, mediaList, exception);
        libvlc.libvlc_media_list_player_play_item(mediaListPlayer, mediaDescriptor, exception);

        while (true)
        {
            int playing = libvlc.libvlc_media_list_player_get_state(mediaListPlayer, exception);
            Assert.assertEquals(0, exception.b_raised);
            if (playing == LibVlcState.libvlc_Playing.ordinal())
            {
                break;
            }
            Thread.sleep(150);
        }

        libvlc.libvlc_media_list_player_stop(mediaListPlayer, exception);
        while (true)
        {
            int playing = libvlc.libvlc_media_list_player_is_playing(mediaListPlayer, exception);
            Assert.assertEquals(0, exception.b_raised);
            if (playing == 0)
            {
                break;
            }
            Thread.sleep(150);
        }
        Assert.assertEquals(LibVlcState.libvlc_Ended.ordinal(), libvlc.libvlc_media_list_player_get_state(
            mediaListPlayer,
            exception));
        libvlc.libvlc_media_list_release(mediaList);
    }

    @Override
    @After
    public void tearDown()
    {
        if (current != null)
        {
            libvlc.libvlc_media_list_player_stop(current, exception);
            while (libvlc.libvlc_media_list_player_get_state(current, exception) != LibVlcState.libvlc_Ended.ordinal())
            {
                try
                {
                    Thread.sleep(100);
                }
                catch (InterruptedException e)
                {
                    //
                }
            }
        }
        current = null;
        super.tearDown();
    }

}
