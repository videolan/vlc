/*****************************************************************************
 * LibVlcMediaListTest.java: VLC Java Bindings
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
import org.videolan.jvlc.internal.LibVlc.LibVlcMedia;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaList;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;


public class LibVlcMediaListTest extends AbstractVLCInternalTest
{

    @Test
    public void mediaListNew()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        Assert.assertNotNull(mediaList);
        Assert.assertEquals(0, exception.b_raised);
    }

    @Test
    public void mediaListAddMediaDescriptor()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        String mrl = this.getClass().getResource("/raffa_voice.ogg").getPath();
        LibVlcMedia libvlc_media = libvlc.libvlc_media_new(
            libvlcInstance,
            mrl,
            exception);
        libvlc.libvlc_media_list_add_media(mediaList, libvlc_media, exception);
        Assert.assertEquals(0, exception.b_raised);
    }

    @Test
    public void mediaListCountTest()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        String mrl = this.getClass().getResource("/raffa_voice.ogg").getPath();
        LibVlcMedia libvlc_media = libvlc.libvlc_media_new(
            libvlcInstance,
            mrl,
            exception);
        libvlc.libvlc_media_list_add_media(mediaList, libvlc_media, exception);
        int result = libvlc.libvlc_media_list_count(mediaList, exception);
        Assert.assertEquals(1, result);
        Assert.assertEquals(0, exception.b_raised);

        libvlc.libvlc_media_list_add_media(mediaList, libvlc_media, exception);
        result = libvlc.libvlc_media_list_count(mediaList, exception);
        Assert.assertEquals(2, result);
        Assert.assertEquals(0, exception.b_raised);
    }

    @Test
    public void mediaListEventManagerTest()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        Assert.assertNotNull(libvlc.libvlc_media_list_event_manager(mediaList, exception));
        Assert.assertEquals(0, exception.b_raised);
    }

    @Test
    public void mediaListIndexOfItemTest()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        String mrl = this.getClass().getResource("/raffa_voice.ogg").getPath();
        LibVlcMedia libvlc_media = libvlc.libvlc_media_new(
            libvlcInstance,
            mrl,
            exception);
        libvlc.libvlc_media_list_add_media(mediaList, libvlc_media, exception);
        int index = libvlc.libvlc_media_list_index_of_item(mediaList, libvlc_media, exception);
        Assert.assertEquals(0, index);
        Assert.assertEquals(0, exception.b_raised);
    }

    @Test
    public void mediaListRemoveIndexTest()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        String mrl = this.getClass().getResource("/raffa_voice.ogg").getPath();
        LibVlcMedia libvlc_media = libvlc.libvlc_media_new(
            libvlcInstance,
            mrl,
            exception);
        libvlc.libvlc_media_list_add_media(mediaList, libvlc_media, exception);
        libvlc.libvlc_media_list_remove_index(mediaList, 0, exception);
        Assert.assertEquals(0, exception.b_raised);
    }

    @Test
    public void mediaListRemoveIndexTest2()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaList mediaList = libvlc.libvlc_media_list_new(libvlcInstance, exception);
        String mrl = this.getClass().getResource("/raffa_voice.ogg").getPath();
        LibVlcMedia libvlc_media = libvlc.libvlc_media_new(
            libvlcInstance,
            mrl,
            exception);
        libvlc.libvlc_media_list_add_media(mediaList, libvlc_media, exception);
        libvlc.libvlc_media_list_remove_index(mediaList, 0, exception);
        Assert.assertEquals(0, exception.b_raised);

        libvlc_media = libvlc.libvlc_media_new(
            libvlcInstance,
            mrl,
            exception);
        libvlc.libvlc_media_list_add_media(mediaList, libvlc_media, exception);
        libvlc.libvlc_media_list_remove_index(mediaList, 0, exception);
    }   
    
}
