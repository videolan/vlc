/*
 * libvlc.cs - libvlc-control CIL bindings
 *
 * $Id$
 */

/**********************************************************************
 *  Copyright (C) 2007 Rémi Denis-Courmont.                           *
 *  This program is free software; you can redistribute and/or modify *
 *  it under the terms of the GNU General Public License as published *
 *  by the Free Software Foundation; version 2 of the license, or (at *
 *  your option) any later version.                                   *
 *                                                                    *
 *  This program is distributed in the hope that it will be useful,   *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *  See the GNU General Public License for more details.              *
 *                                                                    *
 *  You should have received a copy of the GNU General Public License *
 *  along with this program; if not, you can get it from:             *
 *  http://www.gnu.org/copyleft/gpl.html                              *
 **********************************************************************/

using System;
using System.Runtime.InteropServices;

namespace VideoLAN.VLC
{

    public class MediaControl : IDisposable
    {
        /**
         * Possible player status
         */
        enum PlayerStatus
        {
            PlayingStatus,
            PauseStatus,
            ForwardStatus,
            BackwardStatus,
            InitStatus,
            EndStatus,
            UndefinedStatus,
        };

        enum PositionOrigin
        {
            AbsolutePosition,
            RelativePosition,
            ModuloPosition,
        };

        enum PositionKey
        {
            ByteCount,
            SampleCount,
            MediaTime,
        };

        MediaControlHandle self;

        private void CheckDisposed ()
        {
            if (self.IsInvalid)
                throw new ObjectDisposedException ("Media controlled disposed");
        }

        /**
         * Creates a MediaControl with a new LibVLC instance
         */
        public MediaControl (string[] args)
        {
            NativeException e = NativeException.Prepare ();

            U8String[] argv = new U8String[args.Length];
            for (int i = 0; i < args.Length; i++)
                argv[i] = new U8String (args[i]);

            self = MediaControlAPI.New (argv.Length, argv, ref e);
            e.Consume ();
        }

        /**
         * Creates a MediaControl from an existing LibVLC instance
         */
        /*public MediaControl (MediaControl instance)
        {
            NativeException e = NativeException.Prepare ();
            IntPtr libvlc = mediacontrol_get_libvlc_instance (instance.self);

            self = mediacontrol_new_from_instance (libvlc, ref e);
            e.Consume ();
        }*/

        /*public void Play (from)
        {
            CheckDisposed ();
            throw new NotImplementedException ();
        }*/

        public void Resume ()
        {
            CheckDisposed ();
            NativeException e = NativeException.Prepare ();

            MediaControlAPI.Resume (self, IntPtr.Zero, ref e);
            e.Consume ();
        }

        public void Pause ()
        {
            CheckDisposed ();
            NativeException e = NativeException.Prepare ();

            MediaControlAPI.Pause (self, IntPtr.Zero, ref e);
            e.Consume ();
        }

        public void Stop ()
        {
            CheckDisposed ();

            NativeException e = NativeException.Prepare ();
            MediaControlAPI.Stop (self, IntPtr.Zero, ref e);
            e.Consume ();
        }

        public void AddItem (string mrl)
        {
            CheckDisposed ();

            U8String nmrl = new U8String (mrl);
            NativeException e = NativeException.Prepare ();
            MediaControlAPI.PlaylistAddItem (self, nmrl, ref e);
            e.Consume ();
        }

        public void Clear ()
        {
            CheckDisposed ();

            NativeException e = NativeException.Prepare ();
            MediaControlAPI.PlaylistClear (self, ref e);
            e.Consume ();
        }

        public string[] Playlist
        {
            get
            {
                CheckDisposed ();
                throw new NotImplementedException ();
            }
        }

        public void NextItem ()
        {
            CheckDisposed ();

            NativeException e = NativeException.Prepare ();
            MediaControlAPI.PlaylistNextItem (self, ref e);
            e.Consume ();
        }

        public void Dispose ()
        {
            self.Dispose ();
        }
    };
};
