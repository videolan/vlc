package org.videolan.jvlc;

/*****************************************************************************
 * JVLC.java: global class for vlc Java Bindings
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/**
 * This is the main Java VLC class which represents a vlc_object_t. Through this
 * class it's possible to control media loading/playing. It's possible to have 
 * more than one JVLC object in the same program.
 */
public class JVLC {

    static {
	System.loadLibrary("jvlc");
    }

    private int id;

    /**
     * @param args Arguments for the vlc object. These are the same as the
     * original C version of VLC.
     */
    public JVLC(String[] args) {
        String[] properArgs = new String[args.length + 1];
        properArgs[0] = "jvlc";
        for (int i = 0; i < args.length; i++)
            properArgs[i+1] = args[i];

        this.id = create();
	    init(properArgs);
    }

    public JVLC() {
        new JVLC(new String[]{""});
    }
    
    /**
     * @return Returns the ID of the VLC Object
     */
    public int getID() {
        return this.id;
    }

    
    /**
     * Cleanup the VLC Object. It's the equivalent of
     * calling VLC_Die() and VLC_CleanUp() 
     */
    public void quit() {
        this.die();
        this.cleanUp();
    }

    private native int create();

    private native int init(String[] args);

    /**
     * 
     * Creates the interface of the videolan player
     * 
     * @param moduleName The interface module to display
     * @param blocking True if the interface is blocking, otherwise it runs on its own thread
     * @param startPlay True if the player starts to play as soon as the interface is displayed
     * @return An int which is &lt; 0 on error
     */
    public native int addInterface(String moduleName, boolean blocking, boolean startPlay);

    /**
     * 
     * Creates the interface of the videolan player with the default interface
     * or the interface set by -I
     * 
     * @param blocking True if the interface is blocking, otherwise it runs on its own thread
     * @param startPlay True if the player starts to play as soon as the interface is displayed
     * @return An int which is &lt; 0 on error
     */
    public int addInterface(boolean blocking, boolean startPlay) {
        return addInterface(null, blocking, startPlay);
    }

    /**
     * @return The version of the VideoLan object
     */
    public native String getVersion();

    public native String getError(int errorCode);

    private native int die();

    private native int cleanUp();
    
    public native int setVariable(JVLCVariable jvlcVariable);

    public native JVLCVariable getVariable(String varName); // XXX in progress

    public native int addTarget(String URI, String[] options, int insertMode, int position);

    /**
     * Plays the media
     * 
     * @return An int which is &lt; 0 on error
     */
    public native int play();
   
    /**
     * 
     * Pauses the media. If the media is already paused, pause() restarts it.
     * 
     * @return
     */
    public native int pause();

    /**
     * 
     * Stops the media
     * 
     * @return
     */
    public native int stop();

    /**
     * @return True if the player is actually playing something
     */
    public native boolean isPlaying();

    /**
     * @return The absolute position within the media
     */
    public native float getPosition();

    public native float setPosition(float position);

    public native int getTime();

    public native int setTime(int seconds, boolean relative);

    public native int getLength();

    public native float speedFaster();

    public native float speedSlower();

    public native int getPlaylistIndex();

    public native int getPlaylistItems();

    public native int playlistNext();

    public native int playlistPrev();

    public native int playlistClear();

    public native int getVolume();

    public native int setVolume(int volume);

    public native int muteVolume();

    public native int fullScreen();
}
