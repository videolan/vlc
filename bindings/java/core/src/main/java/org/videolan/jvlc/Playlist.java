 /*****************************************************************************
 * Playlist.java: PlaylistIntf implementation class
 *****************************************************************************
 *
 * Copyright (C) 1998-2008 the VideoLAN team
 * 
 * Author: Filippo Carone <filippo@carone.org>
 *
 * Created on 28-feb-2006
 *
 * $Id: Playlist.java 17089 2006-10-15 10:54:15Z littlejohn $
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

import org.videolan.jvlc.internal.LibVlc;
import org.videolan.jvlc.internal.LibVlc.LibVlcInstance;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaPlayer;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;

/**
 * The playlist is deprecated and will be removed. Use MediaList and MediaListPlayer instead.
 */
@Deprecated
public class Playlist {
    
    
    private final LibVlcInstance libvlcInstance;
    private final LibVlc libvlc;
    private final JVLC jvlc;

    public Playlist(JVLC jvlc) {
        this.jvlc = jvlc;
        this.libvlcInstance = jvlc.getInstance();
        this.libvlc = jvlc.getLibvlc();
    }
    
    public synchronized void play(int id, String[] options) throws VLCException {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_playlist_play(libvlcInstance, id, options.length, options, exception);
    }

    public synchronized void play() throws VLCException {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_playlist_play(libvlcInstance, -1, 0, new String[] {}, exception);
    }

    public synchronized void togglePause() throws VLCException {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_playlist_pause(libvlcInstance, exception);
    }

    public synchronized void stop() throws VLCException {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_playlist_stop(libvlcInstance, exception);
    }

    public boolean isRunning() throws VLCException {
        libvlc_exception_t exception = new libvlc_exception_t();
        return libvlc.libvlc_playlist_isplaying(libvlcInstance, exception) == 0? false : true;
    }

    public synchronized int itemsCount() throws VLCException {
        libvlc_exception_t exception = new libvlc_exception_t();
        return libvlc.libvlc_playlist_items_count(libvlcInstance, exception);
    }

    public synchronized void next() throws VLCException {
        libvlc_exception_t exception = new libvlc_exception_t();
        if (! isRunning())
            play();
        libvlc.libvlc_playlist_next(libvlcInstance, exception);
    }

    public synchronized void prev() throws VLCException {
        libvlc_exception_t exception = new libvlc_exception_t();
        if (! isRunning())
            play();
        libvlc.libvlc_playlist_prev(libvlcInstance, exception);
    }

    public synchronized void clear() throws VLCException {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_playlist_clear(libvlcInstance, exception);
    }
    
    public synchronized int add(String uri, String name) throws VLCException {
        libvlc_exception_t exception = new libvlc_exception_t();
        return libvlc.libvlc_playlist_add(libvlcInstance, uri, name, exception);
    }


    public synchronized void deleteItem(int itemID) throws VLCException {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_playlist_delete_item(libvlcInstance, itemID, exception);
    }
    
    public synchronized void setLoop(boolean loop) {
        libvlc_exception_t exception = new libvlc_exception_t();
        libvlc.libvlc_playlist_loop(libvlcInstance, loop? 1 : 0, exception);
    }
    
    public MediaPlayer getMediaInstance()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaPlayer mi = libvlc.libvlc_playlist_get_media_player(libvlcInstance, exception);
        return new MediaPlayer(jvlc, mi);
        
    }
}
