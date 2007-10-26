/*
 * testvlc.cs - tests for libvlc-control CIL bindings
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
using VideoLAN.LibVLC;

namespace VideoLAN.LibVLC.Test
{
    public sealed class Test
    {
        public static int Main (string[] args)
        {
            string[] argv = new string[]{
                "-vvv", "-I", "dummy", "--plugin-path=../../modules"
            };

            Instance vlc = VLC.CreateInstance (argv);
            MediaDescriptor md = vlc.CreateDescriptor ("/dev/null");
            md.Dispose ();

            PlaylistItem item = null;

            foreach (string s in args)
                item = vlc.Add (s);

            vlc.Loop = false;
            vlc.TogglePause ();
            Console.ReadLine ();
            vlc.Stop ();

            if (item != null)
                vlc.Delete (item);
            vlc.Clear ();

            Console.ReadLine ();
            vlc.Dispose ();
            return 0;
        }
    };
};
