/*****************************************************************************
 * ${file_name}: VLC Java Bindings
 *****************************************************************************
 * Copyright (C) 1998-2007 the VideoLAN team
 *
 * Authors: Filippo Carone <filippo@carone.org>
 *
 *
 * $Id $
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

package org.videolan.jvlc.internal;

import java.awt.Canvas;

import javax.swing.JFrame;
import javax.swing.JPanel;

import org.videolan.jvlc.internal.LibVlc.LibVlcCallback;
import org.videolan.jvlc.internal.LibVlc.LibVlcEventManager;
import org.videolan.jvlc.internal.LibVlc.LibVlcInstance;
import org.videolan.jvlc.internal.LibVlc.LibVlcMedia;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaPlayer;
import org.videolan.jvlc.internal.LibVlc.libvlc_event_t;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;

import com.sun.jna.Platform;
import com.sun.jna.Pointer;


public class LibVlcImpl
{

    public static boolean done;

    public static void main(String[] args) throws InterruptedException
    {
        LibVlc libVlc = LibVlc.SYNC_INSTANCE;
        libvlc_exception_t exception = new libvlc_exception_t();
        libVlc.libvlc_exception_init(exception);

        final Object lock = new Object();

        System.out.println("Starting vlc");
        System.out.println("version: " + libVlc.libvlc_get_version());
        System.out.println("changeset: " + libVlc.libvlc_get_changeset());
        System.out.println("compiler: " + libVlc.libvlc_get_compiler());
        
        LibVlcInstance libvlc_instance_t = libVlc.libvlc_new(0, new String[] {"/usr/local/bin/vlc"}, exception);

        LibVlcMedia mediaDescriptor = libVlc
            .libvlc_media_new(libvlc_instance_t, "/home/carone/apps/a.avi", exception);

        LibVlcMediaPlayer mediaPlayer = libVlc.libvlc_media_player_new_from_media(mediaDescriptor, exception);

        LibVlcEventManager mediaInstanceEventManager = libVlc.libvlc_media_player_event_manager(mediaPlayer, exception);

        LibVlcCallback played = new LibVlcCallback()
        {

            public void callback(libvlc_event_t libvlc_event_t, Pointer pointer)
            {
                System.out.println("Playing started.");
            }
        };

        LibVlcCallback endReached = new LibVlcCallback()
        {

            public void callback(libvlc_event_t libvlc_event_t, Pointer pointer)
            {
                synchronized (lock)
                {
                    System.out.println("Playing finished.");
                    LibVlcImpl.done = true;
                }
            }
        };

        libVlc.libvlc_event_attach(
            mediaInstanceEventManager,
            LibVlcEventType.libvlc_MediaPlayerPlaying.ordinal(),
            played,
            null,
            exception);

        libVlc.libvlc_event_attach(
            mediaInstanceEventManager,
            LibVlcEventType.libvlc_MediaPlayerEndReached.ordinal(),
            endReached,
            null,
            exception);

        JFrame frame = new JFrame("title");
        frame.setVisible(true);
        frame.setLocation(100, 100);
        frame.setSize(500, 500);
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        
        JPanel panel = new JPanel();
        Canvas canvas = new Canvas();
        canvas.setSize(500, 500);
        panel.add(canvas);
        frame.getContentPane().add(panel);
        frame.pack();
        
        int drawable = (int) com.sun.jna.Native.getComponentID(canvas);

        if (Platform.isWindows())
        {
            libVlc.libvlc_media_player_set_hwnd(mediaPlayer, drawable, exception);
        }
        else
        {
            libVlc.libvlc_media_player_set_xwindow(mediaPlayer, drawable, exception);
        }
        libVlc.libvlc_media_player_play(mediaPlayer, exception);
    }
}
