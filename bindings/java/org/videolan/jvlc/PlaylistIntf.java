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

public interface PlaylistIntf {

	/**
	 * Plays the item specified in id, with options. At the moment options
	 * has no effect and can be safely set to null.
     * @param id The ID to play
     * @param options Options to play the item with
     */
    void play(int id, String[] options) throws VLCException;
    
    /**
     * Plays the current item in the playlist.
     */
    void play() throws VLCException;
    
    /**
     * Toggles pause for the current item.
     */
    void togglePause() throws VLCException;
    
    
    /**
     * Pauses the currently playing item if pause value is true. Plays it
     * otherwise. If you set pause to true and the current item is already
     * playing, this has no effect.
     * 
     * @param pause
     * @throws VLCException
     */
    void setPause(boolean pause) throws VLCException;
    
    /**
     * Stops the currently playing item. Differently from pause, stopping
     * an item destroys any information related to the item.
     */
    void stop() throws VLCException;
    
    /**
     * This function returns true if the current item has not been stopped.
     * @return True if the current item has not been stopped
     */
    boolean isRunning() throws VLCException;
    
    /**
     * TODO: this should return the number of items added with add, with no
     * respect to videolan internal playlist.
     * 
     * Returns the number of items in the playlist. Beware that this number
     * could be bigger than the number of times add() has been called.
     * 
     * @return Current number of items in the playlist
     */
    int itemsCount() throws VLCException;
    
    /**
     * Move to next item in the playlist and play it.
     */
    void next() throws VLCException;
    
    /**
     * Move to previous item in the playlist and play it.
     */
    void prev() throws VLCException;
    
    /**
     * Clear the playlist which becomes empty after this call.
     */
    void clear() throws VLCException;
    
    /**
     * TODO: document the kind of items that can be added.
     * Add a new item in the playlist.
     * 
     * @param uri Location of the item
     * @param name Name of the item
     * @return The item ID
     */
    int add(String uri, String name) throws VLCException;
    
    /**
     * Currently not implemented
     */
    void addExtended();
    
    /**
     * @param loop
     */
    void setLoop(boolean loop);
}
