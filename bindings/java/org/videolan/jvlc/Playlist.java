 /*****************************************************************************
 * PlaylistIntf.java: The playlist interface
 *****************************************************************************
 *
 * Copyright (C) 1998-2006 the VideoLAN team
 * 
 * Author: Filippo Carone <filippo@carone.org>
 *
 * Created on 28-feb-2006
 *
 * $Id$
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

public class Playlist implements PlaylistIntf {
    
    
    private long libvlcInstance;

    public Playlist(long _libvlcInstance) {
        this.libvlcInstance = _libvlcInstance;
    }
    
    native private int _playlist_add(String uri, String name, String[] options);
    native private void _play(int _id, String[] options);
    native private void _pause();
    native private void _stop();
    native private void _next();
    native private void _prev();
    native private void _clear();
    native private void _deleteItem(int itemID);
    
    native private int _itemsCount();
    native private int _isRunning();
    
    native private boolean _inputIsPlaying();
	native private boolean _inputHasVout();


    public synchronized void play(int id, String[] options) throws VLCException {
        _play(id, options);
    }

    public synchronized void play() throws VLCException {
        play(-1, null);
    }

    public synchronized void togglePause() throws VLCException {
        _pause();
    }

    public synchronized void stop() throws VLCException {
        _stop();

    }

    public boolean isRunning() throws VLCException {
         return (_isRunning() == 0)? false : true ;
    }

    public synchronized int itemsCount() throws VLCException {
        return _itemsCount();
    }

    public synchronized void next() throws VLCException {
        if (! isRunning())
            play();
        _next();
    }

    public synchronized void prev() throws VLCException {
        if (! isRunning())
            play();
        _prev();
    }

    public synchronized void clear() throws VLCException {
    	_clear();
    }

    public synchronized int add(String uri, String name, String[] options) throws VLCException {
        return _playlist_add(uri, name, options);
    }
    
    public synchronized int add(String uri, String name) throws VLCException {
        return add(uri, name, null);
    }

    public synchronized void addExtended() {
    }

    public synchronized void deleteItem(int itemID) throws VLCException {
        _deleteItem(itemID);
    }
    
    public long getInstance() throws VLCException {
        return libvlcInstance;
    }
    
    public synchronized boolean inputIsPlaying() throws VLCException {
        return _inputIsPlaying();
    }

    public synchronized boolean inputHasVout() throws VLCException {
        return _inputHasVout();
    }
    
}
