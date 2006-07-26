/*****************************************************************************
 * InputIntf.java: Input interface
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
public interface InputIntf {
    
    /**
     * @return The total length of the current file playing in millis.
     * @throws VLCException
     */
    long getInputLength() throws VLCException;

    /**
     * @return The current position in millis within the playing item.
     * @throws VLCException
     */
    long getInputTime() throws VLCException;
    
    /**
     * @return The position in %.
     * @throws VLCException
     */
    float getInputPosition() throws VLCException;
    
    /**
     * Not implemented
     * @throws VLCException
     */
    void setInputTime() throws VLCException;
    
    
    /**
     * @return If the playing item is a video file, returns the FPS, otherwise 0.
     * @throws VLCException
     */
    double getInputFPS() throws VLCException;
    
}
