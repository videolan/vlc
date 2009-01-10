/*****************************************************************************
 * MediaInstance.java: VLC Java Bindings Media Instance
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

import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;

import org.videolan.jvlc.event.MediaPlayerCallback;
import org.videolan.jvlc.event.MediaPlayerListener;
import org.videolan.jvlc.internal.LibVlc;
import org.videolan.jvlc.internal.LibVlcEventType;
import org.videolan.jvlc.internal.LibVlc.LibVlcEventManager;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaPlayer;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;


public class MediaPlayer
{

    private final LibVlcMediaPlayer instance;

    private final LibVlc libvlc;

    private final LibVlcEventManager eventManager;

    private List<MediaPlayerCallback> callbacks = new ArrayList<MediaPlayerCallback>();

    private MediaDescriptor mediaDescriptor;

    private volatile boolean released;

    MediaPlayer(JVLC jvlc, LibVlcMediaPlayer instance)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        this.instance = instance;
        libvlc = jvlc.getLibvlc();
        eventManager = libvlc.libvlc_media_player_event_manager(instance, exception);
    }

    public MediaPlayer(MediaDescriptor mediaDescriptor)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc = mediaDescriptor.getLibvlc();
        instance = libvlc.libvlc_media_player_new_from_media(mediaDescriptor.getInstance(), exception);
        eventManager = libvlc.libvlc_media_player_event_manager(instance, exception);
        this.mediaDescriptor = mediaDescriptor;
    }

    public MediaDescriptor getMediaDescriptor()
    {
        return mediaDescriptor;
    }

    public void play()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_media_player_play(instance, exception);
    }

    public void stop()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_media_player_stop(instance, exception);
    }

    public void pause()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_media_player_pause(instance, exception);
    }

    public long getLength()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return libvlc.libvlc_media_player_get_length(instance, exception);
    }

    public long getTime()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return libvlc.libvlc_media_player_get_time(instance, exception);
    }

    public void setTime(long time)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_media_player_set_time(instance, time, exception);
    }

    public float getPosition()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return libvlc.libvlc_media_player_get_position(instance, exception);
    }

    public void setPosition(float position)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_media_player_set_position(instance, position, exception);
    }

    public boolean willPlay()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return (libvlc.libvlc_media_player_will_play(instance, exception) == 1);
    }

    public float getRate()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return libvlc.libvlc_media_player_get_rate(instance, exception);
    }

    public void setRate(float rate)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_media_player_set_rate(instance, rate, exception);
    }

    public boolean hasVideoOutput()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return (libvlc.libvlc_media_player_has_vout(instance, exception) == 1);
    }

    public float getFPS()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return libvlc.libvlc_media_player_get_fps(instance, exception);
    }
    
    public boolean isPlaying()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return libvlc.libvlc_media_player_is_playing(instance, exception) == 1? true : false;
    }

    public void addListener(final MediaPlayerListener listener)
    {
        MediaPlayerCallback callback = new MediaPlayerCallback(this, listener);
        libvlc_exception_t exception = new libvlc_exception_t();
        for (LibVlcEventType event : EnumSet.range(
            LibVlcEventType.libvlc_MediaPlayerPlaying,
            LibVlcEventType.libvlc_MediaPlayerTimeChanged))
        {
            libvlc.libvlc_event_attach(eventManager, event.ordinal(), callback, null, exception);
        }
        callbacks.add(callback);
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

    public void release()
    {
        if (released)
        {
            return;
        }
        released = true;

        libvlc_exception_t exception = new libvlc_exception_t();
        for (MediaPlayerCallback callback : callbacks)  
        {
            for (LibVlcEventType event : EnumSet.range(
                LibVlcEventType.libvlc_MediaPlayerPlaying,
                LibVlcEventType.libvlc_MediaPlayerPositionChanged))
            {
                libvlc.libvlc_event_detach(eventManager, event.ordinal(), callback, null, exception);
            }
        }
        libvlc.libvlc_media_player_release(instance);
        
    }
    
    /**
     * Returns the instance.
     * @return the instance
     */
    LibVlcMediaPlayer getInstance()
    {
        return instance;
    }

}
