/*****************************************************************************
 * Logger.java: VLC Java Bindings
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

package org.videolan.jvlc;

import java.util.Iterator;

import org.videolan.jvlc.internal.LibVlc;
import org.videolan.jvlc.internal.LibVlc.LibVlcLog;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;


public class Logger
{
    LibVlcLog logInstance;
    LibVlc libvlc;

    
    /**
     * @param jvlc The current jvlc instance 
     */
    public Logger(JVLC jvlc)
    {
        this.libvlc = jvlc.getLibvlc();
        libvlc_exception_t exception = new libvlc_exception_t();
        this.logInstance = jvlc.getLibvlc().libvlc_log_open(jvlc.getInstance(), exception);
        if (exception.b_raised == 1)
        {
            throw new RuntimeException("Native exception thrown");
        }
    }
    
    public void clear()
    {
        libvlc.libvlc_log_clear(logInstance);
    }
    
    public void close()
    {
        libvlc.libvlc_log_close(logInstance);
    }
    
    public int count()
    {
        return libvlc.libvlc_log_count(logInstance);
    }
    
    public Iterator<LoggerMessage> iterator()
    {
        return new LoggerIterator(this);
    }

    
}
