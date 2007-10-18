/*****************************************************************************
 * MediaInstance.java: VLC Java Bindings
 *****************************************************************************
 * Copyright (C) 1998-2007 the VideoLAN team
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


public class MediaInstance
{
    private long _media_instance;

    private native long _new_media_instance();
    private native long _new_from_media_descriptor(MediaDescriptor mediaDescriptor);
    private native void _instance_release(long mediaInstance);
    private native void _instance_retain();
    private native void _set_media_descriptor();
    private native MediaDescriptor _get_media_descriptor();
    private native EventManager _event_manager();
    private native void _play();
    private native void _stop();
    private native void _pause();
    private native void _set_drawable();
    private native long _get_length();
    private native long _get_time();
    private native void _set_time(long time);
    private native float _get_position();
    private native void _set_position(float position);
    private native boolean _will_play();
    private native float _get_rate();
    private native void _set_rate(float rate);
    private native void _get_state();
    private native boolean _has_vout();
    private native float _get_fps();
    
    public MediaInstance()
    {
        this._media_instance = _new_media_instance();
    }
    
    public MediaInstance(MediaDescriptor mediaDescriptor)
    {
        this._media_instance = _new_from_media_descriptor(mediaDescriptor);
    }
    
    public MediaDescriptor getMediaDescriptor()
    {
        return _get_media_descriptor();
    }
    
    public void setMediaDescriptor(MediaDescriptor mediaDescriptor)
    {
        _new_from_media_descriptor(mediaDescriptor);
    }
    
    public EventManager getEventManager()
    {
        return _event_manager();
    }
    
    public void play()
    {
        _play();
    }
    
    public void stop()
    {
        _stop();
    }
    
    public void pause()
    {
        _pause();
    }

    public long getLength()
    {
        return _get_length();
    }
    
    public long getTime()
    {
        return _get_time();
    }
    
    public void setTime(long time)
    {
        _set_time(time);
    }
    
    public float getPosition()
    {
        return _get_position();
    }
    
    public void setPosition(float position)
    {
        _set_position(position);
    }
    
    public boolean willPlay()
    {
        return _will_play();
    }
    
    public float getRate()
    {
        return _get_rate();
    }
    
    public void setRate(float rate)
    {
        _set_rate(rate);
    }
    
    public boolean hasVideoOutput()
    {
        return _has_vout();
    }
    
    public float getFPS()
    {
        return _get_fps();
    }
    
    /**
     * {@inheritDoc}
     */
    @Override
    protected void finalize() throws Throwable
    {
        super.finalize();
        _instance_release(_media_instance);
    }
    
    
    
    
}
