/*****************************************************************************
 * VLMInternalTest.java: VLC Java Bindings
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

import org.junit.Test;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;


public class VLMInternalTest extends AbstractVLCInternalTest
{

    @Test
    public void testVLMPlay()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_vlm_add_broadcast(libvlcInstance, "test", "file://" + mrl, "", 0, null, 1, 0, exception);
        libvlc.libvlc_vlm_play_media(libvlcInstance, mrl, exception);
        catchException(exception);
        libvlc.libvlc_vlm_stop_media(libvlcInstance, mrl, exception);
    }

}
