/*****************************************************************************
 * JVLC.java: Main Java Class, represents a libvlc_instance_t object
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
public class JVLC implements JLibVLC, Runnable {
    
    static {
        System.load(System.getProperty( "user.dir" ) + "/libjvlc.so" );
    }

    /**
     * These are set as final since they live along the jvlc object
     */
    private final long _instance;
    public  final Playlist playlist;

    
    private boolean beingDestroyed = false;
    private long resolution = 50;
	private boolean inputPlaying = false;
	private boolean inputVout = false;
    
    public JVLC() {
        _instance = createInstance();
        playlist = new Playlist( _instance );
        new Thread(this).start();
    }
    
    public JVLC(String[] args) {
        _instance = createInstance( args );
        playlist = new Playlist( _instance );
        new Thread(this).start();
    }
    
    
    /**
     * Destroys the current instance of jvlc, cleaning up objects.
     * This is unreversible.
     */
    public void destroy() {
    	beingDestroyed = true;
    	_destroy();
    }
 

	/*
     * Core methods
     */
    private native long createInstance();
    private native long createInstance( String[] args );
    private native void _destroy();   
    /*
     * 	Audio native methods
     */
    private native boolean	_getMute();
    private native void		_setMute( boolean value );
    private native void		_toggleMute();
    private native int		_getVolume();
    private native void		_setVolume( int volume );

    /*
     *  Input native methods
     */
    private native long     _getInputLength();
    private native float    _getInputPosition();
    private native long     _getInputTime();
    private native float	_getInputFPS();

    
    /*
     * Video native methods
     */
    private native void     _toggleFullscreen();
    private native void     _setFullscreen( boolean value);
    private native boolean  _getFullscreen();
    private native int      _getVideoHeight();
    private native int      _getVideoWidth();
    private native void		_getSnapshot(String filename);
 
    
    public boolean getMute() {
        return _getMute();
    }

    public void setMute(boolean value) {
        _setMute( value );
        
    }
    
    public void toggleMute() {
    	_toggleMute();
    }

    public int getVolume() {
        return _getVolume();        
    }

    public void setVolume(int volume) {
        _setVolume( volume );
        
    }

    public void toggleFullscreen() {
        _toggleFullscreen();
        
    }

    public void setFullscreen( boolean value ) {
        _setFullscreen( value );
        
    }

    public boolean getFullscreen() {
    	return _getFullscreen();        
    }

    public int getVideoHeight() {
        return _getVideoHeight();
    }
    

    public int getVideoWidth() {
        return _getVideoWidth();        
    }

    
    public long getInputLength() {
        return _getInputLength();        
    }

    public long getInputTime() {
        return _getInputTime();
    }

    public float getInputPosition() {
        return _getInputPosition();
        
    }

    public void setInputTime() {
        // TODO Auto-generated method stub
        
    }

    public double getInputFPS() {
        return _getInputFPS();
    }
    
    public long getInstance() {
        return _instance;
    }

    /*
     * Getters and setters
     */
	public Playlist getPlaylist() {
		return playlist;
	}
    

	public void getSnapshot(String filename) {
		_getSnapshot(filename);
	}
	
	/**
	 * Checks if the input is playing.
	 * @return True if there is a playing input.
	 */
	public boolean isInputPlaying() {
		return inputPlaying;
	}

	/**
	 * Checks if the input has spawned a video window.
	 * @return True if there is a video window.
	 */
	public boolean hasVout() {
		return inputVout;
	}

	/*
	 * (non-Javadoc)
	 * @see java.lang.Runnable#run()
	 * 
	 * In this thread we check the playlist and input status.
	 */
	public void run() {
		while (! beingDestroyed) {
			while (playlist.isRunning()) {
				if (playlist.inputIsPlaying()) {
					inputPlaying = true;
				}
				else {
					inputPlaying = false;
                }
	                    
				if (playlist.inputHasVout()) {
					inputVout = true;
                }
				else {
					inputVout = false;
                }
				try {
					Thread.sleep(resolution);
				} catch (InterruptedException e) {
					e.printStackTrace();
				} 
			} // while playlist running
	           inputPlaying = false;
	           inputVout = false;
			try {
				Thread.sleep(resolution);
			} catch (InterruptedException e) {
				e.printStackTrace();
			} // try
		} // while ! being destroyed
	} // run

}

