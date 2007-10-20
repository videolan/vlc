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

namespace VideoLAN.VLC
{
    public sealed class Test
    {
        public static int Main (string[] args)
        {
            MediaControl mc = new MediaControl (args);

            foreach (string s in args)
                mc.AddItem (s);

            /*mc.Play ();*/
            Console.ReadLine ();

            mc.Stop ();
            mc.Clear ();

            mc.Dispose ();
            return 0;
        }
    };
};
