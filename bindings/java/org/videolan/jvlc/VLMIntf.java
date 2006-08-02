/*****************************************************************************
 * VLMIntf.java: VLM Interface
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

public interface VLMIntf {

	/**
	 * Add a broadcast, with one input
	 * @param name the name of the new broadcast
	 * @param input the input MRL
	 * @param output the output MRL (the parameter to the "sout" variable)
	 * @param options additional options
	 * @param enabled boolean for enabling the new broadcast
	 * @param loop Should this broadcast be played in loop ?
	 */
    void addBroadcast( String name, String input, String output, String[] options, boolean enabled, boolean loop )
    	throws VLCException;
    
    /**
     * Delete a media (vod or broadcast)
     * @param name the media to delete
     */    
    void deleteMedia( String name ) throws VLCException;
    
    /**
     * Enable or disable a media (vod or broadcast)
     * @param name the media to work on
     * @param enabled the new status
     */    
    void setEnabled( String name, boolean enabled ) throws VLCException;
    
    /**
     * Set the output for a media
     * @param name the media to work on
     * @param output the output MRL (the parameter to the "sout" variable)
     */
    void setOutput( String name, String output ) throws VLCException;
    
    /**
     * Set a media's input MRL. This will delete all existing inputs and
     * add the specified one.
     * @param name the media to work on
     * @param input the input MRL
     */
    void setInput( String name, String input ) throws VLCException;
    
    /**
     * Set loop mode for a media
     * @param name the media to work on
     * @param loop the new status
     */
    void setLoop( String name, boolean loop ) throws VLCException;
    
    /**
     * Edit the parameters of a media. This will delete all existing inputs and
     * add the specified one.
     * @param name the name of the new broadcast
     * @param input the input MRL
     * @param output the output MRL (the parameter to the "sout" variable)
     * @param options additional options
     * @param enabled boolean for enabling the new broadcast
     * @param loop Should this broadcast be played in loop ?
     */    
    void changeMedia( String name, String input, String output, String[] options, boolean enabled, boolean loop )
    	throws VLCException;
    /**
     * Plays a media
     * @param name of the broadcast to play
     */
    void playMedia(String name) throws VLCException;

    /**
     * Stops a media
     * @param name of the broadcast to stop
     */
    void stopMedia(String name) throws VLCException;

    /**
     * Pauses a media
     * @param name name of the broadcast to pause
     */    
    void pauseMedia(String name) throws VLCException;

}
