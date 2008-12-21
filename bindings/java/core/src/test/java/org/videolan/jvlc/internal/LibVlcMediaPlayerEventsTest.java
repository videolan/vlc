/*****************************************************************************
 * LibVlcMediaPlayerEventsTest.java: VLC Java Bindings
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
import org.videolan.jvlc.internal.LibVlc.LibVlcCallback;
import org.videolan.jvlc.internal.LibVlc.LibVlcEventManager;
import org.videolan.jvlc.internal.LibVlc.LibVlcMedia;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaPlayer;
import org.videolan.jvlc.internal.LibVlc.libvlc_event_t;

import com.sun.jna.Pointer;


public class LibVlcMediaPlayerEventsTest extends AbstractVLCEventTest
{
    @Test
    public void testPlayingEvent() throws Exception
    {
        LibVlcMedia media = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMediaPlayer mediaPlayer = libvlc.libvlc_media_player_new_from_media(media, exception);
        LibVlcEventManager eventManager = libvlc.libvlc_media_player_event_manager(mediaPlayer, exception);
        LibVlcCallback callback = new LibVlc.LibVlcCallback()
        {

            public void callback(libvlc_event_t libvlc_event, Pointer userData)
            {
                Assert.assertEquals(LibVlcEventType.libvlc_MediaPlayerPlaying.ordinal(), libvlc_event.type);
                eventFired = 1;
            }
        };
        
        libvlc.libvlc_event_attach(
            eventManager,
            LibVlcEventType.libvlc_MediaPlayerPlaying.ordinal(),
            callback,
            null,
            exception);
        libvlc.libvlc_media_player_play(mediaPlayer, exception);
        Thread.sleep(2000L);
        Assert.assertEquals(1, eventFired);
    }

    @Test
    public void testPausedEvent() throws Exception
    {
        LibVlcMedia media = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMediaPlayer mediaPlayer = libvlc.libvlc_media_player_new_from_media(media, exception);
        LibVlcEventManager eventManager = libvlc.libvlc_media_player_event_manager(mediaPlayer, exception);
        LibVlcCallback callback = new LibVlc.LibVlcCallback()
        {

            public void callback(libvlc_event_t libvlc_event, Pointer userData)
            {
                Assert.assertEquals(LibVlcEventType.libvlc_MediaPlayerPaused.ordinal(), libvlc_event.type);                
                eventFired = 1;
            }
        };
        
        libvlc.libvlc_event_attach(
            eventManager,
            LibVlcEventType.libvlc_MediaPlayerPaused.ordinal(),
            callback,
            null,
            exception);
        
        libvlc.libvlc_media_player_play(mediaPlayer, exception);
        Thread.sleep(1000L);
        libvlc.libvlc_media_player_pause(mediaPlayer, exception);
        Thread.sleep(1000L);
        
        Assert.assertEquals(1, eventFired);
    }

    @Test
    public void testStoppedEvent() throws Exception
    {
        LibVlcMedia media = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMediaPlayer mediaPlayer = libvlc.libvlc_media_player_new_from_media(media, exception);
        LibVlcEventManager eventManager = libvlc.libvlc_media_player_event_manager(mediaPlayer, exception);
        LibVlcCallback callback = new LibVlc.LibVlcCallback()
        {

            public void callback(libvlc_event_t libvlc_event, Pointer userData)
            {
                Assert.assertEquals(LibVlcEventType.libvlc_MediaPlayerStopped.ordinal(), libvlc_event.type);                
                eventFired = 1;
            }
        };
        
        libvlc.libvlc_event_attach(
            eventManager,
            LibVlcEventType.libvlc_MediaPlayerStopped.ordinal(),
            callback,
            null,
            exception);
        
        libvlc.libvlc_media_player_play(mediaPlayer, exception);
        Thread.sleep(1000L);
        libvlc.libvlc_media_player_stop(mediaPlayer, exception);
        Thread.sleep(1000L);
        
        Assert.assertEquals(1, eventFired);
    }

}
