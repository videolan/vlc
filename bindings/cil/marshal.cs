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

namespace VideoLAN.LibVLC
{
    /**
     * Abstract safe handle class for non-NULL pointers
     * (Microsoft.* namespace has a similar class, but lets stick to System.*)
     */
    public abstract class NonNullHandle : SafeHandle
    {
        protected NonNullHandle ()
            : base (IntPtr.Zero, true)
        {
        }

        public override bool IsInvalid
        {
            get
            {
                return handle == IntPtr.Zero;
            }
        }
    };

    public class BaseObject<HandleT> : IDisposable where HandleT : SafeHandle
    {
        protected NativeException ex;
        protected HandleT self;

        internal BaseObject (HandleT self)
        {
            this.self = self;
            ex = new NativeException ();
        }

        public void Dispose ()
        {
            ex.Dispose ();
            self.Close ();
        }
    };
};
