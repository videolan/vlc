/*
 * Created on 28-feb-2006
 *
 * $Id: JVLCPlayer.java 10 2006-02-28 19:07:17Z little $
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

public class JVLC implements JLibVLC {
    
    static {
        System.load(System.getProperty("user.dir") + "/libjvlc.so");
    }


    private long _instance;
    public Playlist playlist;
    
    public JVLC() {
        _instance = createInstance();
        playlist = new Playlist(_instance);
    }
    
    public JVLC(String[] args) {
        _instance = createInstance(args);
        playlist = new Playlist(_instance);
    }
    
    private native long createInstance();
    private native long createInstance(String[] args);
    
    public void getMute() {
        // TODO Auto-generated method stub
        
    }

    public void setMute() {
        // TODO Auto-generated method stub
        
    }

    public void getVolume() {
        // TODO Auto-generated method stub
        
    }

    public void setVolume() {
        // TODO Auto-generated method stub
        
    }

    public void toggleFullscreen() {
        // TODO Auto-generated method stub
        
    }

    public void setFullscreen() {
        // TODO Auto-generated method stub
        
    }

    public void getFullscreen() {
        // TODO Auto-generated method stub
        
    }

    public void getLength() {
        // TODO Auto-generated method stub
        
    }

    public void getTime() {
        // TODO Auto-generated method stub
        
    }

    public void getPosition() {
        // TODO Auto-generated method stub
        
    }

    public void setTime() {
        // TODO Auto-generated method stub
        
    }

    public double getFPS() {
        // TODO Auto-generated method stub
        return 0;
    }

    public long getInstance() {
        return _instance;
    }

}
