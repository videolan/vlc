/*****************************************************************************
 * MediaInstancePlayCallback.java: VLC Java Bindings
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

package org.videolan.jvlc.event;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.videolan.jvlc.MediaPlayer;
import org.videolan.jvlc.internal.LibVlc;
import org.videolan.jvlc.internal.LibVlcEventType;
import org.videolan.jvlc.internal.LibVlc.LibVlcCallback;
import org.videolan.jvlc.internal.LibVlc.libvlc_event_t;
import org.videolan.jvlc.internal.LibVlc.media_player_time_changed;

import com.sun.jna.Pointer;


public class MediaPlayerCallback implements LibVlcCallback
{

    private MediaPlayerListener listener;
    private MediaPlayer mediaPlayer;

    /**
     * Logger.
     */
    private Logger log = LoggerFactory.getLogger(MediaPlayerCallback.class);

    public MediaPlayerCallback(MediaPlayer mediaInstance, MediaPlayerListener listener)
    {
        this.mediaPlayer = mediaInstance;
        this.listener = listener;
    }
    /**
     * {@inheritDoc}
     */
    public void callback(libvlc_event_t libvlc_event, Pointer userData)
    {
        if (libvlc_event.type == LibVlcEventType.libvlc_MediaPlayerPlaying.ordinal())
        {
            listener.playing(mediaPlayer);
        }
        else if (libvlc_event.type == LibVlcEventType.libvlc_MediaPlayerPaused.ordinal())
        {
            listener.paused(mediaPlayer);
        }
        else if (libvlc_event.type == LibVlcEventType.libvlc_MediaPlayerEndReached.ordinal())
        {
            listener.endReached(mediaPlayer);
        }
        else if (libvlc_event.type == LibVlcEventType.libvlc_MediaPlayerPositionChanged.ordinal())
        {
            listener.positionChanged(mediaPlayer);
        }
        else if (libvlc_event.type == LibVlcEventType.libvlc_MediaPlayerStopped.ordinal())
        {
            listener.stopped(mediaPlayer);
        }
        else if (libvlc_event.type == LibVlcEventType.libvlc_MediaPlayerTimeChanged.ordinal())
        {
            libvlc_event.event_type_specific.setType(LibVlc.media_player_time_changed.class);
            LibVlc.media_player_time_changed timeChanged = (media_player_time_changed) libvlc_event.event_type_specific
                .readField("media_player_time_changed");
            listener.timeChanged(mediaPlayer, timeChanged.new_time);
        }
        else if (libvlc_event.type == LibVlcEventType.libvlc_MediaPlayerEncounteredError.ordinal())
        {
            log.warn("Media player encountered error.");
            listener.errorOccurred(mediaPlayer);
        }
        else
        {
            log.debug("Unsupported event error. Event id: {}", libvlc_event.type);
        }
    }
}
