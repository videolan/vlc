/*****************************************************************************
 * MediaDescriptor.java: VLC Java Bindings Media Descriptor
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

import org.videolan.jvlc.internal.LibVlc;
import org.videolan.jvlc.internal.LibVlc.LibVlcEventManager;
import org.videolan.jvlc.internal.LibVlc.LibVlcMedia;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;


public class MediaDescriptor
{
    private LibVlcMedia instance;
    private LibVlc libvlc;
    private LibVlcEventManager eventManager;
    private volatile boolean released;
    
    private MediaPlayer mediaPlayer;
    
    /**
     * @param jvlc The jvlc instance to create the media descriptor for.
     * @param media The media string
     */
    public MediaDescriptor(JVLC jvlc, String media)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc = jvlc.getLibvlc();
        instance = libvlc.libvlc_media_new(jvlc.getInstance(), media, exception);
        eventManager = libvlc.libvlc_media_event_manager(instance, exception);
    }

    MediaDescriptor(JVLC jvlc, LibVlcMedia instance)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc = jvlc.getLibvlc();
        this.instance = instance;
        eventManager = libvlc.libvlc_media_event_manager(instance, exception);
    }

    public void addOption(String option)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_media_add_option(instance, option, exception );
    }
    
    public String getMrl()
    {
        return libvlc.libvlc_media_get_mrl(instance);
    }
    
    public MediaPlayer getMediaPlayer()
    {
        if (mediaPlayer == null)
        {
            this.mediaPlayer = new MediaPlayer(this);
        }
        return this.mediaPlayer;
    }
    
    /**
     * {@inheritDoc}
     */
    @Override
    protected void finalize() throws Throwable
    {
        release();
        super.finalize();
    }

    
    
    /**
     * Returns the instance.
     * @return the instance
     */
    LibVlcMedia getInstance()
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

    /**
     * 
     */
    public void release()
    {
        if (released)
        {
            return;
        }
        released = true;
        libvlc.libvlc_media_release(instance);
    }
    
    
}
