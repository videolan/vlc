/*****************************************************************************
 * LibVlcLogTest.java: VLC Java Bindings
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
import org.videolan.jvlc.internal.LibVlc.LibVlcLog;
import org.videolan.jvlc.internal.LibVlc.LibVlcLogIterator;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;


public class LibVlcLogTest extends AbstractVLCInternalTest
{

    @Test
    public void testLogOpen()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcLog libvlcLog = libvlc.libvlc_log_open(libvlcInstance, exception);
        Assert.assertNotNull(libvlcLog);
    }
    
    @Test
    public void testLogClose()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcLog libvlcLog = libvlc.libvlc_log_open(libvlcInstance, exception);
        libvlc.libvlc_log_close(libvlcLog, exception);
        Assert.assertEquals(exception.message, 0, exception.raised);
    }

    //@Test
    public void testLogClear()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcLog libvlcLog = libvlc.libvlc_log_open(libvlcInstance, exception);
        libvlc.libvlc_log_clear(libvlcLog, exception);
        Assert.assertEquals(exception.message, 0, exception.raised);
        Assert.assertEquals(0, libvlc.libvlc_log_count(libvlcLog, exception));
    }

    //@Test
    public void testLogGetIterator()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcLog libvlcLog = libvlc.libvlc_log_open(libvlcInstance, exception);
        libvlc.libvlc_log_clear(libvlcLog, exception);
        Assert.assertEquals(exception.message, 0, exception.raised);
        Assert.assertEquals(0, libvlc.libvlc_log_count(libvlcLog, exception));
        LibVlcLogIterator logIterator = libvlc.libvlc_log_get_iterator(libvlcLog, exception);
        Assert.assertNotNull(logIterator);
    }
}
