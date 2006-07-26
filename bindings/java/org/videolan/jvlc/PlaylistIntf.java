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
     * @param id The ID to play
     * @param options Options to play the item withs
     */
    void play(int id, String[] options) throws VLCException;
    
    /**
     * Plays the current item
     */
    void play() throws VLCException;
    
    /**
     * Toggles pause for the current item.
     */
    void togglePause() throws VLCException;
    
    /**
     * Stops the playlist.
     */
    void stop() throws VLCException;
    
    /**
     * @return True if playlist is not stopped
     */
    boolean isRunning() throws VLCException;
    
    /**
     * @return Current number of items in the playlist
     */
    int itemsCount() throws VLCException;
    
    /**
     * @return True if the current input is really playing
     */
    boolean inputIsPlaying() throws VLCException;
    
    /**
     * Move to next item
     */
    void next() throws VLCException;
    
    /**
     * Move to previous item
     */
    void prev() throws VLCException;
    
    /**
     * Clear the playlist
     */
    void clear() throws VLCException;
    
    /**
     * Add a new item in the playlist
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
     * @return True if the current input has spawned a video output window
     */
    boolean inputHasVout() throws VLCException;
}
