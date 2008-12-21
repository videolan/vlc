/*****************************************************************************
 * LibVlcMediaListEventsTest.java: VLC Java Bindings
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

import org.junit.Test;
import org.videolan.jvlc.internal.LibVlc.LibVlcCallback;
import org.videolan.jvlc.internal.LibVlc.LibVlcEventManager;
import org.videolan.jvlc.internal.LibVlc.LibVlcMedia;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaList;
import org.videolan.jvlc.internal.LibVlc.libvlc_event_t;

import com.sun.jna.Pointer;


public class LibVlcMediaListEventsTest extends AbstractVLCEventTest
{
    
    @Test
    public void itemAddedTest()
    {
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        LibVlcMedia media = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        
        LibVlcEventManager eventManager = libvlc.libvlc_media_list_event_manager(mediaList, exception);
        LibVlcCallback callback = new LibVlc.LibVlcCallback()
        {

            public void callback(libvlc_event_t libvlc_event, Pointer userData)
            {
                Assert.assertEquals(LibVlcEventType.libvlc_MediaListItemAdded.ordinal(), libvlc_event.type);
                eventFired = 1;
            }
        };
        
        libvlc.libvlc_event_attach(
            eventManager,
            LibVlcEventType.libvlc_MediaListItemAdded.ordinal(),
            callback,
            null,
            exception);
        
        libvlc.libvlc_media_list_add_media(mediaList, media, exception);
        Assert.assertEquals(1, eventFired);
    }

    @Test
    public void itemDeletedTest()
    {
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        LibVlcMedia media = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        
        LibVlcEventManager eventManager = libvlc.libvlc_media_list_event_manager(mediaList, exception);
        LibVlcCallback callback = new LibVlc.LibVlcCallback()
        {

            public void callback(libvlc_event_t libvlc_event, Pointer userData)
            {
                Assert.assertEquals(LibVlcEventType.libvlc_MediaListItemDeleted.ordinal(), libvlc_event.type);
                eventFired = 1;
            }
        };
        
        libvlc.libvlc_event_attach(
            eventManager,
            LibVlcEventType.libvlc_MediaListItemDeleted.ordinal(),
            callback,
            null,
            exception);
        
        libvlc.libvlc_media_list_add_media(mediaList, media, exception);
        libvlc.libvlc_media_list_remove_index(mediaList, 0, exception);
        Assert.assertEquals(1, eventFired);
    }

}
