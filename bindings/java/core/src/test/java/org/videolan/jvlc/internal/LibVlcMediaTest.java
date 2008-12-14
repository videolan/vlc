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
import org.videolan.jvlc.internal.LibVlc.LibVlcMedia;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;


public class LibVlcMediaTest extends AbstractVLCInternalTest
{
    
    @Test
    public void mediaDescriptorNew() throws Exception
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        Assert.assertNotNull(md);
        Assert.assertEquals(0, exception.raised);
    }
    
    @Test
    public void mediaDescriptorGetMrl()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMedia md = libvlc.libvlc_media_new(libvlcInstance, mrl, exception);
        String mdMrl = libvlc.libvlc_media_get_mrl(md);
        Assert.assertEquals(mrl, mdMrl);
    }

}
