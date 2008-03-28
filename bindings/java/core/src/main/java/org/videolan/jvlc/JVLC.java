/*****************************************************************************
 * JVLC.java: Main Java Class, represents a libvlc_instance_t object
 *****************************************************************************
 *
 * Copyright (C) 1998-2008 the VideoLAN team
 * 
 * Author: Filippo Carone <filippo@carone.org>
 *         Philippe Morin <phmorin@free.fr>
 *
 * Created on 28-feb-2006
 *
 * $Id: JVLC.java 20141 2007-05-16 19:31:35Z littlejohn $
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 * 
 */

package org.videolan.jvlc;

import java.awt.Canvas;

import org.videolan.jvlc.internal.LibVlc;
import org.videolan.jvlc.internal.LibVlc.LibVlcInstance;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;

import com.sun.jna.Native;

public class JVLC
{

    private final LibVlcInstance instance;

    private final LibVlc libvlc = LibVlc.SYNC_INSTANCE;

    private MediaList mediaList;
    
    public JVLC()
    {
        String[] args = new String[] {};
        instance = createInstance(args);
        mediaList = new MediaList(this);
    }

    public JVLC(String[] args)
    {
        instance = createInstance(args);
    }
    
    public JVLC(String args)
    {
        this(args.split(" "));
    }

    public MediaInstance play(String media)
    {
        MediaDescriptor mediaDescriptor = new MediaDescriptor(this, media);
        MediaInstance mediaInstance = new MediaInstance(mediaDescriptor);
        mediaInstance.play();
        return mediaInstance;
    }

    public void setVideoOutput(Canvas canvas)
    {
        long drawable = Native.getComponentID(canvas);
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_video_set_parent(instance, drawable, exception );
    }

    /*
     * Core methods
     */
    private LibVlcInstance createInstance(String[] args)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_exception_init(exception);

        return libvlc.libvlc_new(args.length, args, exception);
    }

    /**
     * Returns the _instance.
     * @return the _instance
     */
    LibVlcInstance getInstance()
    {
        return instance;
    }

    /**
     * Returns the libvlc.
     * @return the libvlc
     */
    LibVlc getLibvlc()
    {
        return libvlc;
    }

    /*
     * (non-Javadoc)
     * @see java.lang.Object#finalize()
     */
    @Override
    protected void finalize() throws Throwable
    {
        libvlc.libvlc_release(instance);
        super.finalize();
    }
    
    /**
     * Returns the mediaList.
     * @return the mediaList
     */
    public MediaList getMediaList()
    {
        return mediaList;
    }
    
}
