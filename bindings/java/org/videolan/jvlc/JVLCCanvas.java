/*****************************************************************************
 * JVLCCanvas.java: AWT Canvas containing VLC Video Output
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

import java.awt.Canvas;
import java.awt.Graphics;

public class JVLCCanvas extends Canvas {

    public native void paint(Graphics g);

    private final JVLC jvlc = new JVLC();
    
    /**
     * Default constructor. The canvas is set a dimension of 200x200
     */
    public JVLCCanvas() {
        setSize(200, 200);
    }
    
    /**
     * @param width The initial canvas width
     * @param height The initial canvas height
     */
    public JVLCCanvas(int width, int height) {
        setSize(width, height);
    }

    
    public JVLC getJVLC() {
        return jvlc;
    }

}
