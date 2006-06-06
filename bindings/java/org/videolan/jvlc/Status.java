/*****************************************************************************
 * Status.java: VLC status thread 
 *****************************************************************************
 *
 * Copyright (C) 1998-2006 the VideoLAN team
 * 
 * Author: Filippo Carone <filippo@carone.org>
 *
 * Created on 5-jun-2006
 *
 * $Id $
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
 * In this thread we check the playlist status and the input status to see if:
 * - the playlist is running
 * - the input is playing
 * - if the input has a vout child
 *
 */
public class Status implements Runnable {

	final JVLC jvlc;
	public boolean inputPlaying;
	public boolean inputVout;
    private long resolution = 50;
	
	public Status(JVLC jvlc) {
		if (jvlc == null)
			throw new RuntimeException("No jvlc instance.. I die");
		if (jvlc.playlist == null) 
            throw new RuntimeException("No playlist instance.. I die");
		this.jvlc = jvlc;
		new Thread(this).start();
	}
	
	public void run() {
		while (true) {
			while (jvlc.playlist.isRunning()) {
				if (jvlc.playlist.inputIsPlaying()) {
					inputPlaying = true;
                }
				else {
					inputPlaying = false;
                }
                     
				if (jvlc.playlist.inputHasVout()) {
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
            inputPlaying = false;
            inputVout = false;
			try {
				Thread.sleep(resolution);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}
	}

}
