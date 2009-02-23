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
            Console.WriteLine ("Media: {0} {1} (duration: {2}, {3}preparsed)",
                               m.State, m.Location, m.Duration,
                               m.IsPreparsed ? "" : "not ");
        }

        private static void WriteMediaState (Media m, State s)
        {
            Console.WriteLine (" -> {0}", s);
        }

        private static void DumpPlayer (Player p)
        {
            if (!p.IsPlaying)
                return;

            int percent = (int)(p.Position * 100);
            Console.Write ("{0}: {1} of {2} ms ({3}%)\r", p.State,
                           p.Time, p.Length, percent);
        }

        private static void Sleep (int msec)
        {
            System.Threading.Thread.Sleep (msec);
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
            foreach (string mrl in args)
            {
                Media media = new Media (vlc, mrl);
                Player player = new Player (media);

                DumpMedia (media);
                DumpMedia ((Media)media.Clone ());
                media.StateChanged += WriteMediaState;

                player.Play ();
                do
                {
                    DumpPlayer (player);
                    Sleep (500);
                }
                while (player.IsPlaying);
                player.Stop ();
                media.Dispose ();
                player.Dispose ();
            }

            vlc.Dispose ();
            return 0;
        }
    };
};
