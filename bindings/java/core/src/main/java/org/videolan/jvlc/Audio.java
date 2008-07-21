/*****************************************************************************
 * Audio.java: VLC Java Bindings, audio methods
 *****************************************************************************
 *
 * Copyright (C) 1998-2008 the VideoLAN team
 * 
 * Author: Filippo Carone <filippo@carone.org>
 *
 *
 * $Id: $
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

import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;


public class Audio
{

    private final JVLC jvlc;

    /**
     * Constant for left channel audio
     */
    public static final int LEFT_CHANNEL = 3;

    /**
     * Constant for right channel audio
     */
    public static final int RIGHT_CHANNEL = 4;

    /**
     * Constant for reverse channel audio
     */
    public static final int REVERSE_CHANNEL = 2;

    /**
     * Constant for stereo channel audio
     */
    public static final int STEREO_CHANNEL = 1;

    /**
     * Constant for dolby channel audio
     */
    public final int DOLBY_CHANNEL = 5;

    
    public Audio(JVLC jvlc)
    {
        this.jvlc = jvlc;
    }

    public int getTrack(MediaPlayer mediaInstance)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return jvlc.getLibvlc().libvlc_audio_get_track(mediaInstance.getInstance(), exception);
    }

    public void setTrack(MediaPlayer mediaInstance, int track)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_audio_set_track(mediaInstance.getInstance(), track, exception);
    }

    public int getChannel()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return jvlc.getLibvlc().libvlc_audio_get_channel(jvlc.getInstance(), exception);
    }

    public void setChannel(int channel)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_audio_set_channel(jvlc.getInstance(), channel, exception);
    }

    public boolean getMute()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return jvlc.getLibvlc().libvlc_audio_get_mute(jvlc.getInstance(), exception) == 1 ? true : false;
    }

    public void setMute(boolean value)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_audio_set_mute(jvlc.getInstance(), value ? 1 : 0, exception);
    }

    public void toggleMute()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_audio_toggle_mute(jvlc.getInstance(), exception);
    }

    public int getVolume()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return jvlc.getLibvlc().libvlc_audio_get_volume(jvlc.getInstance(), exception);
    }

    public void setVolume(int volume)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_audio_set_volume(jvlc.getInstance(), volume, exception);
    }
}
