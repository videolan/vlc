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

public class JVLC {

    private int id;

    public JVLC(String[] args) {
	String[] properArgs = new String[args.length + 1];
	properArgs[0] = "jvlc";
	for (int i = 0; i < args.length; i++)
	    properArgs[i+1] = args[i];

	this.id = create();
	init(properArgs);
    }

    private native int create();

    private native int init(String[] args);

    public native int addInterface(String moduleName, boolean blocking, boolean startPlay);

    public int addInterface(boolean blocking, boolean startPlay) {
	return addInterface(null, blocking, startPlay);
    }

    public native String getVersion();

    public native String getError(int errorCode);

    public native int die();

    public native int cleanUp();
    
    public native int setVariable(JVLCVariable jvlcVariable);

    public native JVLCVariable getVariable(String varName); // XXX in progress

    public native int addTarget(String URI, String[] options, int insertMode, int position);

    public native int play();
   
    public native int pause();

    public native int stop();

    public native boolean isPlaying();

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
