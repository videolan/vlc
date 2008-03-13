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

    public void play()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_media_list_player_play(instance, exception);
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

    
    public void playItem(MediaDescriptor descriptor)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_media_list_player_play_item(instance, descriptor.getInstance(), exception);
    }

    public void playItem(int index)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_media_list_player_play_item_at_index(instance, index, exception);
    }
    
    public void setMediaInstance(MediaInstance mediaInstance)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_media_list_player_set_media_instance(instance, mediaInstance.getInstance(), exception);        
    }

    /**
     * {@inheritDoc}
     */
    @Override
    protected void finalize() throws Throwable
    {
        jvlc.getLibvlc().libvlc_media_list_player_release(instance);
        super.finalize();
    }

}
