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

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.videolan.jvlc.internal.LibVlc.LibVlcInstance;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaDescriptor;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaInstance;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;


public class LibVlcMediaInstanceTest
{
    
    private LibVlc libvlc = LibVlc.SYNC_INSTANCE;

    private LibVlcInstance libvlcInstance;

    private String mrl = this.getClass().getResource("/raffa_voice.ogg").getPath();

    private libvlc_exception_t exception;

    @Before
    public void testSetup()
    {
        exception = new libvlc_exception_t();
        libvlcInstance = libvlc.libvlc_new(0, new String[]{}, exception);
        libvlc.libvlc_exception_clear(exception);
    }
    
    @After
    public void tearDown()
    {
        libvlc.libvlc_release(libvlcInstance);
        libvlc.libvlc_exception_clear(exception);
    }
    
    @Test
    public void mediaInstanceNew()
    {
        LibVlcMediaInstance instance = libvlc.libvlc_media_instance_new(libvlcInstance, exception);
        Assert.assertNotNull(instance);
        Assert.assertEquals(0, exception.raised);
    }
    
    @Test
    public void mediaInstancePlayBad()
    {
        LibVlcMediaInstance instance = libvlc.libvlc_media_instance_new(libvlcInstance, exception);
        libvlc.libvlc_media_instance_play(instance, exception);
        Assert.assertEquals(1, exception.raised); // no associated media descriptor
    }
    
    @Test
    public void mediaInstancePlay()
    {
        LibVlcMediaDescriptor md = libvlc.libvlc_media_descriptor_new(libvlcInstance, mrl, exception);
        LibVlcMediaInstance mi = libvlc.libvlc_media_instance_new_from_media_descriptor(md, exception);
        libvlc.libvlc_media_instance_play(mi, exception);
        Assert.assertEquals(0, exception.raised);
    }
    
    @Test
    public void mediaInstancePauseBad()
    {
        LibVlcMediaDescriptor md = libvlc.libvlc_media_descriptor_new(libvlcInstance, mrl, exception);
        LibVlcMediaInstance mi = libvlc.libvlc_media_instance_new_from_media_descriptor(md, exception);
        libvlc.libvlc_media_instance_pause(mi, exception);
        Assert.assertEquals(1, exception.raised);
    }
    
    @Test
    public void mediaInstancePause()
    {
        LibVlcMediaDescriptor md = libvlc.libvlc_media_descriptor_new(libvlcInstance, mrl, exception);
        LibVlcMediaInstance mi = libvlc.libvlc_media_instance_new_from_media_descriptor(md, exception);
        libvlc.libvlc_media_instance_play(mi, exception);
        libvlc.libvlc_media_instance_pause(mi, exception);
        Assert.assertEquals(0, exception.raised);
    }
    
    @Test
    public void mediaInstanceSetPosition()
    {
        LibVlcMediaDescriptor md = libvlc.libvlc_media_descriptor_new(libvlcInstance, mrl, exception);
        LibVlcMediaInstance mi = libvlc.libvlc_media_instance_new_from_media_descriptor(md, exception);
        libvlc.libvlc_media_instance_play(mi, exception);
        libvlc.libvlc_media_instance_set_position(mi, 0.5f, exception);
        Assert.assertEquals(0, exception.raised);
        float position = libvlc.libvlc_media_instance_get_position(mi, exception);
        Assert.assertTrue("Position is: " + position, position >= 0.5f);
    }
    
    @Test
    public void mediaInstanceStop()
    {
        LibVlcMediaDescriptor md = libvlc.libvlc_media_descriptor_new(libvlcInstance, mrl, exception);
        LibVlcMediaInstance mi = libvlc.libvlc_media_instance_new_from_media_descriptor(md, exception);
        libvlc.libvlc_media_instance_stop(mi, exception);
        Assert.assertEquals(0, exception.raised);
    }
    
    @Test
    public void mediaInstanceStop2()
    {
        LibVlcMediaDescriptor md = libvlc.libvlc_media_descriptor_new(libvlcInstance, mrl, exception);
        LibVlcMediaInstance mi = libvlc.libvlc_media_instance_new_from_media_descriptor(md, exception);
        libvlc.libvlc_media_instance_play(mi, exception);
        libvlc.libvlc_media_instance_stop(mi, exception);
        Assert.assertEquals(0, exception.raised);
    }
    
}
