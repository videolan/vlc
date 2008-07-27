/*****************************************************************************
 * mediaPlayerPlayListener.java: VLC Java Bindings
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

import org.videolan.jvlc.MediaPlayer;


public interface MediaPlayerListener
{

    void playing(MediaPlayer mediaPlayer);
    
    void paused(MediaPlayer mediaPlayer);
    
    void stopped(MediaPlayer mediaPlayer);
    
    void endReached(MediaPlayer mediaPlayer);
    
    void timeChanged(MediaPlayer mediaPlayer, long newTime);
    
    void positionChanged(MediaPlayer mediaPlayer);
    
    void errorOccurred(MediaPlayer mediaPlayer);
    
}
