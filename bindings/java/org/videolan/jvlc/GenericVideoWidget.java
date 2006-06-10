/*****************************************************************************
 * SWTVideoWidget.java: A component usable in SWT Application, embeds JVLC
 *****************************************************************************
 *
 * Copyright (C) 1998-2006 the VideoLAN team
 * 
 * Author: Kuldipsingh Pabla <Kuldipsingh.Pabla@sun.com>
 * 
 * Created on 10-jun-2006
 *
 * $Id $
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
import java.awt.Frame;
import java.awt.Component;


public class GenericVideoWidget {
    /*
     * This class implements a Composite container for VLC Video Output
     */

    /*
     * The root SWT Frame we embed JVLCCanvas in
     */
    public Frame rootFrame;
    private JVLCCanvas jvlcCanvas;
    
    public GenericVideoWidget( Component parent ) {
    	// allocate the new AWT Frame to embed in the Composite
    	rootFrame = new Frame ();

    	// add the JVLCCanvas to the Frame
    	jvlcCanvas = new JVLCCanvas();
    	rootFrame.add( jvlcCanvas );
    }
    
    
	public JVLC getJVLC() {
		return jvlcCanvas.getJVLC();
	}
}
