/*
 * Created on 25-nov-2005
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 * 
 */
/**
 * @author Filippo Carone <filippo@carone.org>
 */

package org.videolan.jvlc;

import java.awt.Canvas;
import java.awt.Graphics;

public class JVLCCanvas extends Canvas {

    public native void paint(Graphics g);

    private final JVLC jvlcObject = new JVLC();
    
    public JVLCCanvas() {
        jvlcObject.addInterface("dummy", false, false);
    }
    
    public JVLC getJVLCObject() {
        return jvlcObject;
    }
}
