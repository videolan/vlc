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

/**
 * @author little
 *
 */
/**
 * @author little
 *
 */
public interface VideoIntf {
    /**
     * Toggles the fullscreen.
     */
    void	toggleFullscreen();
    
    
    /**
     * Sets fullscreen if fullscreen argument is true. 
     * @param fullscreen
     */
    void	setFullscreen( boolean fullscreen );
    
    
    /**
     * @return True if the current video window is in fullscreen mode.
     */
    boolean getFullscreen();
    
    
    /**
     * Saves a snapshot of the current video window.
     * @param filepath The full path (including filename) were to save the snapshot to. 
     * If you only give a path, not including the filename, the snapshot will be saved in
     * the specified path using vlc naming conventions. 
     */
    void	getSnapshot(String filepath);
    
    
    /**
     * @return The current video window height
     */
    int		getVideoHeight();

    /**
     * @return The current video window width
     */
    int		getVideoWidth();    
}
