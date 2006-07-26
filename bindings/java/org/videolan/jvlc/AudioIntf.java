/*****************************************************************************
 * AudioIntf.java: Audio methods interface
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

public interface AudioIntf {
    /**
     * @return True if input is currently muted.
     * @throws VLCException
     */
    boolean getMute() throws VLCException;
    
    /**
     * @param value If true, then the input is muted.
     * @throws VLCException
     */
    void setMute(boolean value) throws VLCException;

    /**
     * Toggles mute
     * @throws VLCException
     */
    void toggleMute() throws VLCException;

    /**
     * @return The volume level
     * @throws VLCException
     */
    int getVolume() throws VLCException;

    /**
     * @param volume The volume level (0-200) to set.
     * @throws VLCException
     */
    void setVolume(int volume) throws VLCException;
}
