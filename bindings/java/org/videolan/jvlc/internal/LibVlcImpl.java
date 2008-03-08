/*****************************************************************************
 * LibVlcImpl.java: VLC Java Bindings, sample LibVlc use.
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
import java.awt.Component;

import javax.swing.JFrame;
import javax.swing.JPanel;

import org.videolan.jvlc.internal.LibVlc.LibVlcCallback;
import org.videolan.jvlc.internal.LibVlc.LibVlcEventManager;
import org.videolan.jvlc.internal.LibVlc.LibVlcInstance;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaDescriptor;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaInstance;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;

import com.sun.jna.Pointer;


public class LibVlcImpl
{

    public static boolean done;

    public static void main(String[] args) throws InterruptedException
    {
        LibVlc libVlc = LibVlc.INSTANCE;
        libvlc_exception_t exception = new libvlc_exception_t();
        libVlc.libvlc_exception_init(exception);

        final Object lock = new Object();

        System.out.println("Starting vlc");
        LibVlcInstance libvlc_instance_t = libVlc.libvlc_new(0, new String[] {"/usr/local/bin/vlc"}, exception);

        LibVlcMediaDescriptor mediaDescriptor = libVlc
            .libvlc_media_descriptor_new(libvlc_instance_t, "/home/carone/a.avi", exception);

        LibVlcMediaInstance mediaInstance = libVlc.libvlc_media_instance_new_from_media_descriptor(mediaDescriptor, exception);

        LibVlcEventManager mediaInstanceEventManager = libVlc.libvlc_media_instance_event_manager(mediaInstance, exception);

        LibVlcCallback played = new LibVlcCallback()
        {

            @Override
            public void callback(int libvlc_event_t, Pointer pointer)
            {
                System.out.println("Playing started.");
            }
        };

        LibVlcCallback endReached = new LibVlcCallback()
        {

            @Override
            public void callback(int libvlc_event_t, Pointer pointer)
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
            LibVlcEventType.libvlc_MediaInstancePlayed.ordinal(),
            played,
            null,
            exception);

        libVlc.libvlc_event_attach(
            mediaInstanceEventManager,
            LibVlcEventType.libvlc_MediaInstanceReachedEnd.ordinal(),
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
        
        long drawable = com.sun.jna.Native.getComponentID(canvas);
        
        libVlc.libvlc_video_set_parent(libvlc_instance_t, drawable, exception);

        libVlc.libvlc_media_instance_play(mediaInstance, exception);
    }
}
