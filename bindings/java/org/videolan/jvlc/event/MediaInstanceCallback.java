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

import org.videolan.jvlc.MediaInstance;
import org.videolan.jvlc.internal.LibVlc;
import org.videolan.jvlc.internal.LibVlcEventType;
import org.videolan.jvlc.internal.LibVlc.LibVlcCallback;
import org.videolan.jvlc.internal.LibVlc.libvlc_event_t;
import org.videolan.jvlc.internal.LibVlc.media_instance_time_changed;

import com.sun.jna.Pointer;


public class MediaInstanceCallback implements LibVlcCallback
{

    private MediaInstanceListener listener;
    private MediaInstance mediaInstance;

    public MediaInstanceCallback(MediaInstance mediaInstance, MediaInstanceListener listener)
    {
        this.mediaInstance = mediaInstance;
        this.listener = listener;
    }
    /**
     * {@inheritDoc}
     */
    @Override
    public void callback(libvlc_event_t libvlc_event, Pointer userData)
    {
        if (libvlc_event.type == LibVlcEventType.libvlc_MediaInstancePlayed.ordinal())
        {
            listener.played(mediaInstance);
        }
        else if (libvlc_event.type == LibVlcEventType.libvlc_MediaInstancePaused.ordinal())
        {
            listener.paused(mediaInstance);
        }
        else if (libvlc_event.type == LibVlcEventType.libvlc_MediaInstanceReachedEnd.ordinal())
        {
            listener.endReached(mediaInstance);
        }
        else if (libvlc_event.type == LibVlcEventType.libvlc_MediaInstancePositionChanged.ordinal())
        {
            listener.positionChanged(mediaInstance);
        }
        else if (libvlc_event.type == LibVlcEventType.libvlc_MediaInstanceTimeChanged.ordinal())
        {
            libvlc_event.event_type_specific.setType(LibVlc.media_instance_time_changed.class);
            LibVlc.media_instance_time_changed timeChanged = (media_instance_time_changed) libvlc_event.event_type_specific
                .readField("media_instance_time_changed");
            listener.timeChanged(mediaInstance, timeChanged.new_time);
        }
        else
        {
            throw new RuntimeException("Unsupported event error. Event id: " + libvlc_event.type);
        }
    }
}
