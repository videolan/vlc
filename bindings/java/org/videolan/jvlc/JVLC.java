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


public class JVLC implements Runnable {
    
    static {
        System.loadLibrary("jvlc" );
    }

    /**
     * These are set as final since they live along the jvlc object
     */
    private final long		_instance;
    
    public  final Playlist	playlist;
    public	final Video		video;
    public	final Audio		audio;
    public	final Input		input;
    public	final VLM		vlm;
    
    private boolean beingDestroyed = false;

    /**
     * This is the time in millis VLC checks for internal status 
     */
    private long resolution = 50;
    
	private boolean inputPlaying = false;
	private boolean inputVout = false;
    
    public JVLC() {
        String[] args = new String[1];
        args[0] = "jvlc";
        
        _instance	= createInstance( args );
        playlist	= new Playlist	( _instance );
        video		= new Video		( _instance );
        audio		= new Audio		( _instance );
        input		= new Input		( _instance );
        vlm			= new VLM		( _instance );
        new Thread(this).start();
    }
    
    public JVLC(String[] args) {
        _instance	= createInstance( args );
        playlist	= new Playlist	( _instance );
        video		= new Video		( _instance );
        audio		= new Audio		( _instance );
        input		= new Input		( _instance );
        vlm			= new VLM		( _instance );
        
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
    private native long createInstance( String[] args );
    private native void _destroy();   

    public long getInstance() throws VLCException {
        return _instance;
    }

    /*
     * Getters and setters
     */
	public Playlist getPlaylist() throws VLCException {
		return playlist;
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
					if (input.isPlaying()) {
						inputPlaying = true;
					}
					else {
						inputPlaying = false;
				    }
				            
					if (input.hasVout()) {
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

