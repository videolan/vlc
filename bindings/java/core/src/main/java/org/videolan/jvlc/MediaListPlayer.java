/*****************************************************************************
 * MediaListPlayer.java: VLC Java Bindings, MediaList player
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

import org.videolan.jvlc.internal.LibVlc.LibVlcMediaListPlayer;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;


public class MediaListPlayer
{

    private final LibVlcMediaListPlayer instance;

    private final JVLC jvlc;

    private volatile boolean released;
    
    public MediaListPlayer(JVLC jvlc)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        this.jvlc = jvlc;
        instance = jvlc.getLibvlc().libvlc_media_list_player_new(jvlc.getInstance(), exception);
    }

    public void setMediaList(MediaList list)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_media_list_player_set_media_list(instance, list.getInstance(), exception);
    }

    public boolean isPlaying()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return jvlc.getLibvlc().libvlc_media_list_player_is_playing(instance, exception) == 1;
    }

    
    /**
     * 
     */
    public void play()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_media_list_player_play(instance, exception);
        try
        {
            while (jvlc.getLibvlc().libvlc_media_list_player_is_playing(instance, exception) == 0)
            {
                Thread.sleep(25);
            }
        }
        catch(InterruptedException e)
        {
            //
        }
    }

    public void stop()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_media_list_player_stop(instance, exception);
    }

    public void pause()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_media_list_player_pause(instance, exception);
    }
    
    public void next()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_media_list_player_next(instance, exception);
    }

    /**
     * Plays the given descriptor and returns only when the player has started to play.
     * @param descriptor The media descriptor to play
     */
    public void playItem(MediaDescriptor descriptor)
    {
        playItem(descriptor, true);
    }
    
    /**
     * @param descriptor The media descriptor to play
     * @param synchronous If true it does not return until the player is not playing.
     */
    public void playItem(MediaDescriptor descriptor, boolean synchronous)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_media_list_player_play_item(instance, descriptor.getInstance(), exception);
        if (!synchronous)
        {
            return;
        }
        
        try
        {
            while (jvlc.getLibvlc().libvlc_media_list_player_is_playing(instance, exception) == 0)
            {
                Thread.sleep(25);
            }
        }
        catch(InterruptedException e)
        {
            //
        }
        
    }

    /**
     * Plays the item at the given index and returns only when the player has started to play.
     * @param index The item index to play.
     */
    public void playItem(int index)
    {
        playItem(index, true);
    }
    
    /**
     * @param index The item index to play.
     * @param synchronous If true it does not return until the player is not playing.
     */
    public void playItem(int index, boolean synchronous)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_media_list_player_play_item_at_index(instance, index, exception);
        try
        {
            while (jvlc.getLibvlc().libvlc_media_list_player_is_playing(instance, exception) == 0)
            {
                Thread.sleep(25);
            }
        }
        catch(InterruptedException e)
        {
            //
        }
    }
    
    public void setMediaInstance(MediaPlayer mediaInstance)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_media_list_player_set_media_player(instance, mediaInstance.getInstance(), exception);        
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
     * 
     */
    public void release()
    {
        if (released)
        {
            return;
        }
        released = true;
        jvlc.getLibvlc().libvlc_media_list_player_release(instance);
        
    }

}
