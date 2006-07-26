/*****************************************************************************
 * JVLC.java: Main Java Class, represents a libvlc_instance_t object
 *****************************************************************************
 *
 * Copyright (C) 1998-2006 the VideoLAN team
 * 
 * Author: Filippo Carone <filippo@carone.org>
 *         Philippe Morin <phmorin@free.fr>
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

public class JVLC implements JLibVLC, Runnable {
    
    static {
        System.loadLibrary("jvlc" );
    }

    /**
     * These are set as final since they live along the jvlc object
     */
    private final long _instance;
    public  final Playlist playlist;

    
    private boolean beingDestroyed = false;

    /**
     * This is the time in millis VLC checks for internal status 
     */
    private long resolution = 50;
    
	private boolean inputPlaying = false;
	private boolean inputVout = false;
    
    public JVLC() {
        String[] args = new String[1];
        args[0] = "";
        
        _instance = createInstance(args);
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
 

    /*
     * VLM native methods
     */
    private native void _addBroadcast(String mediaName, String meditInputMRL, String mediaOutputMRL ,
                               String[] additionalOptions, boolean enableBroadcast, boolean isPlayableInLoop);
    private native void _deleteMedia	(String mediaName);
    private native void _setEnabled		(String mediaName,	boolean newStatus);
    private native void _setOutput		(String mediaName,	String mediaOutputMRL);
    private native void _setInput		(String mediaName,	String mediaInputMRL);
    private native void _setLoop		(String mediaName,	boolean isPlayableInLoop);
    private native void _changeMedia	(String newMediaName, String inputMRL, String outputMRL , String[] additionalOptions, boolean enableNewBroadcast, boolean isPlayableInLoop);

    /*
     * Native methods wrappers
     */
       
    
    public boolean getMute() throws VLCException {
        return _getMute();
    }

    public void setMute(boolean value) throws VLCException {
        _setMute( value );
        
    }
    
    public void toggleMute() throws VLCException {
    	_toggleMute();
    }

    public int getVolume() throws VLCException {
        return _getVolume();        
    }

    public void setVolume(int volume) throws VLCException {
        _setVolume( volume );
        
    }

    public void toggleFullscreen() throws VLCException {
        _toggleFullscreen();
        
    }

    public void setFullscreen( boolean value ) throws VLCException {
        _setFullscreen( value );
        
    }

    public boolean getFullscreen() throws VLCException {
    	return _getFullscreen();        
    }

    public int getVideoHeight() throws VLCException {
        return _getVideoHeight();
    }
    

    public int getVideoWidth() throws VLCException {
        return _getVideoWidth();        
    }

    
    public long getInputLength() throws VLCException {
        return _getInputLength();        
    }

    public long getInputTime() throws VLCException {
        return _getInputTime();
    }

    public float getInputPosition() throws VLCException {
        return _getInputPosition();
        
    }

    public void setInputTime() throws VLCException {
        // TODO Auto-generated method stub
        
    }

    public double getInputFPS() throws VLCException {
        return _getInputFPS();
    }
    
    public long getInstance() throws VLCException {
        return _instance;
    }

    /*
     * Getters and setters
     */
	public Playlist getPlaylist() throws VLCException {
		return playlist;
	}
    

	public void getSnapshot(String filename) throws VLCException {
		_getSnapshot(filename);
	}


    public void addBroadcast( String name, String input, String output, String[] options, boolean enabled, boolean loop )
    	throws VLCException {
    	_addBroadcast(name, input, output, options, enabled, loop);
    }
    
    public void deleteMedia( String name ) throws VLCException {
    	_deleteMedia(name);
    }
    
    public void setEnabled( String name, boolean enabled ) throws VLCException {
    	_setEnabled(name, enabled);
    }
    
    public void setOutput( String name, String output ) throws VLCException {
    	_setOutput(name, output);
    }
    
    public void setInput( String name, String input ) throws VLCException {
    	_setInput(name, input);
    }
    
    public void setLoop( String name, boolean loop ) throws VLCException {
    	_setLoop(name, loop);
    }
    
    public void changeMedia( String name, String input, String output, String[] options, boolean enabled, boolean loop )
    	throws VLCException {
    	_changeMedia(name, input, output, options, enabled, loop);
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
			try {
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
				}
			} catch (VLCException e1) { } // while playlist running
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

