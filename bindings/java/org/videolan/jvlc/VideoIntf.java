/*****************************************************************************
 * VideoIntf.java: Video methods interface
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

import java.awt.Component;
import java.awt.Dimension;


public interface VideoIntf {
    /**
     * Toggles the fullscreen.
     */
    void	toggleFullscreen() throws VLCException;
    
    
    /**
     * Sets fullscreen if fullscreen argument is true. 
     * @param fullscreen
     */
    void	setFullscreen( boolean fullscreen ) throws VLCException;
    
    
    /**
     * @return True if the current video window is in fullscreen mode.
     */
    boolean getFullscreen() throws VLCException;
    
    
    /**
     * Saves a snapshot of the current video window.
     * @param filepath The full path (including filename) were to save the snapshot to. 
     * If you only give a path, not including the filename, the snapshot will be saved in
     * the specified path using vlc naming conventions. 
     */
    void	getSnapshot(String filepath) throws VLCException;
    
    
    /**
     * @return The current video window height
     */
    int		getHeight() throws VLCException;

    /**
     * @return The current video window width
     */
    int		getWidth() throws VLCException;
    
    /**
     * Get the size of the video output window as a Dimension object.
     * @return The video size in a Dimension object.
     * @throws VLCException
     */
    Dimension	getSize() throws VLCException;
    
    /**
     * Destroys video output, but the item continues to play.
     * @throws VLCException
     */
    void destroyVideo() throws VLCException;
    
    /**
     * Moves video output from the current Canvas to another. This funtion
     * doens't resize the video output to the new canvas size. See resizeVideo().
     * @param c
     * @throws VLCException
     */
    void reparent(Component c) throws VLCException;
    
    /**
     * Resizes video output to width and height. This operation could be necessary
     * after reparenting. See reparentVideo().
     * @param width The new video output width
     * @param height The new video output height
     * @throws VLCException
     */
    void setSize(int width, int height) throws VLCException;
    
    /**
     * Resizes video output to width and height. This operation could be necessary
     * after reparenting. See reparentVideo().
	 *
     * @param d The new size of video
     * @throws VLCException
     */
    void setSize(Dimension d) throws VLCException;
    
}
