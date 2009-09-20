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

public class JVLC
{

    private final LibVlcInstance instance;

    private final LibVlc libvlc = LibVlc.SYNC_INSTANCE;

    private VLM vlm;
    
    private Audio audio;
    
    private volatile boolean released;

    private Canvas canvas;
    
    public JVLC()
    {
        this(new String[] {});
    }

    public JVLC(String[] args)
    {
        instance = createInstance(args);
        audio = new Audio(this);
    }
    
    public JVLC(String args)
    {
        this(args.split(" "));
    }

    /*
     * Core methods
     */
    private LibVlcInstance createInstance(String[] args)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return libvlc.libvlc_new(args.length, args, exception);
    }

    public MediaPlayer play(String media)
    {
        MediaDescriptor mediaDescriptor = new MediaDescriptor(this, media);
        MediaPlayer mediaPlayer = new MediaPlayer(mediaDescriptor);
        if (canvas != null)
        {
            mediaPlayer.setParent(canvas);
        }
        mediaPlayer.play();
        mediaDescriptor.release();
        return mediaPlayer;
    }

    public void setVideoOutput(Canvas canvas)
    {
        this.canvas = canvas;
    }

    public Logger getLogger()
    {
        return new Logger(this);
    }

    public VLM getVLM()
    {
        if (vlm != null)
        {
            vlm.release();
        }
        this.vlm = new VLM(this);
        return vlm;
    }
    
    public LoggerVerbosityLevel getLogVerbosity()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        int level = libvlc.libvlc_get_log_verbosity(instance, exception);
        return LoggerVerbosityLevel.getSeverity(level);
    }

    public void setLogVerbosity(LoggerVerbosityLevel level)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_set_log_verbosity(instance, level.ordinal(), exception);
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
    
    /**
     * Releases this instance and the native resources.
     */
    public void release()
    {
        if (released)
        {
            return;
        }
        released = true;
        if (vlm != null)
        {
            vlm.release();
            vlm = null;
        }
        
        libvlc.libvlc_release(instance);
    }

    /*
     * (non-Javadoc)
     * @see java.lang.Object#finalize()
     */
    @Override
    protected void finalize() throws Throwable
    {
        release();
        super.finalize();
    }

    /**
     * @return
     */
    public Audio getAudio()
    {
        return audio;
    }
    
}
