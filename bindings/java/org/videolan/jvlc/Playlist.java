/*****************************************************************************
 * Playlist.java: The playlist class
 *****************************************************************************
 * Copyright (C) 1998-2006 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Filippo Carone <filippo@carone.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

package org.videolan.jvlc;

public class Playlist implements PlaylistIntf {
    
    
    private long libvlcInstance;

    public Playlist(long _libvlcInstance) {
        this.libvlcInstance = _libvlcInstance;
    }
    
    native private int _playlist_add(String uri, String name);
    native private void _play(int _id, String[] options);
    native private void _pause();
    native private void _stop();
    native private void _next();
    native private void _prev();
    native private void _clear();
    
    native private int _itemsCount();
    native private int _isPlaying();

    public void play(int id, String[] options) {
        _play(id, options);
    }


    public void pause() {
        _pause();
    }

    public void stop() {
        _stop();

    }

    public boolean isPlaying() {
        return (_isPlaying() == 1)? false : true ;
    }

    public int itemsCount() {
        return _itemsCount();
    }

    public void next() {
        _next();
    }

    public void prev() {
        _prev();
    }

    public void clear() {
        _clear();
    }

    public int add(String uri, String name) {
        return _playlist_add(uri, name);
    }

    public void addExtended() {
        // TODO Auto-generated method stub
    }

    public long getInstance() {
        return libvlcInstance;
    }

    
    
}
