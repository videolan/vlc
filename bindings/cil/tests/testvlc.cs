/*
 * testvlc.cs - tests for libvlc CIL bindings
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
        private static void DumpMedia (Media m)
        {
            Console.WriteLine ("Media at    {0}", m.Location);
            Console.WriteLine (" duration:  {0}µs", m.Duration);
            Console.WriteLine (" preparsed: {0}", m.IsPreparsed);
        }

        public static int Main (string[] args)
        {
            string[] argv = new string[]{
                "-vv", "-I", "dummy", "--plugin-path=../../modules"
            };

            Console.WriteLine ("Running on LibVLC {0} ({1})", VLC.Version,
                               VLC.ChangeSet);
            Console.WriteLine (" (compiled with {0})", VLC.Compiler);

            VLC vlc = new VLC (argv);
            Media m = new Media (vlc, "/dev/null");
            DumpMedia (m);

            DumpMedia ((Media)m.Clone ());

            vlc.Dispose ();
            m.Dispose ();
            return 0;
        }
    };
};
