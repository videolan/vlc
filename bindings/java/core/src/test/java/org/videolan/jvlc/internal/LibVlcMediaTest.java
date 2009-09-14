/*****************************************************************************
 * MediaDescriptorTest.java: VLC Java Bindings
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
import org.videolan.jvlc.internal.LibVlc.LibVlcEventManager;
import org.videolan.jvlc.internal.LibVlc.LibVlcMedia;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;


public class LibVlcMediaTest extends AbstractVLCInternalTest
{
    
    @Test
    public void testMediaNew() throws Exception
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        Assert.assertNotNull(md);
        Assert.assertEquals(0, exception.b_raised);
    }
    
    @Test
    public void testMediaGetMrl()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        String mdMrl = libvlc.libvlc_media_get_mrl(md);
        Assert.assertEquals(mrl, mdMrl);
    }
    
    @Test
    public void testMediaDuplicate()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcMedia md2 = libvlc.libvlc_media_duplicate(md);
        Assert.assertNotSame(md.getPointer(), md2.getPointer());
        Assert.assertEquals(libvlc.libvlc_media_get_mrl(md2), libvlc.libvlc_media_get_mrl(md));
    }
    
    @Test
    public void testMediaEventManager()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        LibVlcEventManager evManager = libvlc.libvlc_media_event_manager(md, exception);
        Assert.assertEquals(0, exception.b_raised);
        Assert.assertNotNull(evManager);
    }

    @Test
    public void testMediaGetState()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        int state = libvlc.libvlc_media_get_state(md, exception);
        Assert.assertEquals(0, exception.b_raised);
        Assert.assertEquals(LibVlcState.libvlc_NothingSpecial.ordinal(), state);
    }

    @Test
    public void testMediaIsPreparsed()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        int state = libvlc.libvlc_media_is_preparsed(md, exception);
        Assert.assertEquals(0, exception.b_raised);
        Assert.assertEquals(0, state);
    }

    
}
