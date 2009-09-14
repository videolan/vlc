/*****************************************************************************
 * LibVlcCoreTest.java: VLC Java Bindings
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
import org.videolan.jvlc.internal.LibVlc.LibVlcInstance;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;


public class LibVlcCoreTest
{

    private LibVlc instance = LibVlc.SYNC_INSTANCE;
    
    @Test
    public void testNew() throws Exception
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcInstance libvlcInstance = instance.libvlc_new(0, new String[] {"-I","dummy","--aout=dummy","--vout=dummy"}, exception);
        Assert.assertNotNull(libvlcInstance);
        Assert.assertEquals(0, exception.b_raised);
    }
    
    @Test
    public void testRelease() throws Exception
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcInstance libvlcInstance = instance.libvlc_new(0, new String[] {}, exception);
        instance.libvlc_release(libvlcInstance);
    }
    
    @Test
    public void testAddIntf() throws Exception
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcInstance libvlcInstance = instance.libvlc_new(0, new String[] {}, exception);
        instance.libvlc_add_intf(libvlcInstance, "dummy", exception);
        Assert.assertEquals(0, exception.b_raised);
        instance.libvlc_release(libvlcInstance);
    }
    
}
