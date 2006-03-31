/*****************************************************************************
 * SWTVideoWidget.java: A component usable in SWT Application, embeds JVLC
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
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


import org.eclipse.swt.awt.SWT_AWT;
import org.eclipse.swt.widgets.Composite;

import java.awt.Frame;

public class SWTVideoWidget {
    /*
     * This class implements an SWTCanvas containing VLC Video Output
     */

    /*
     * The root SWT Frame we embed JVLCCanvas in
     */
    public Frame rootFrame;
    private JVLCCanvas jvlcCanvas;
    
    /*
     * This class 'installs' the VLC video output window
     * in a Composite container such as a canvas. 
     */
    public SWTVideoWidget( Composite parent ) {
    	// allocate the new AWT Frame to embed in the Composite
    	rootFrame = SWT_AWT.new_Frame( parent );

    	// add the JVLCCanvas to the Frame
    	jvlcCanvas = new JVLCCanvas();
    	rootFrame.add( jvlcCanvas );
    }
    
    
	public JVLC getJVLC() {
		return jvlcCanvas.getJVLC();
	}
}
