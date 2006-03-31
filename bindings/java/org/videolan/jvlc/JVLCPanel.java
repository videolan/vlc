/*****************************************************************************
 * JVLCPanel.java: Java Swing JPanel embedding VLC Video Output
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

import javax.swing.JPanel;

public class JVLCPanel extends JPanel {

    private final JVLCCanvas jvcc;
    
    /**
     * Default constructor. The initial size is 200x200
     */
    public JVLCPanel() {
        jvcc = new JVLCCanvas();
        add(jvcc);        
    }
    
    /**
     * @param width The width of the panel
     * @param height The height of the panel
     */
    public JVLCPanel(int width, int height) {
        jvcc = new JVLCCanvas(width, height);
        add(jvcc);        
    }
    

    public JVLC getJVLCObject() {
        return jvcc.getJVLC();
    }
    
    public void setSize(int width, int height) {
        super.setSize(width, height);
        jvcc.setSize(width, height);
    }

}
