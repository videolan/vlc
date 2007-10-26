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
     * VLCException: managed base class for LibVLC exceptions
     */
    public class VLCException : Exception
    {
        /**
         * Creates a managed VLC exception.
         */
        public VLCException ()
        {
        }

        /**
         * Creates a managed VLC exception.
         * @param message exception error message
         */
        public VLCException (string message)
            : base (message)
        {
        }

        /**
         * Creates a managed VLC exception wrapping another exception.
         * @param message exception error message
         * @param inner inner wrapped exception
         */
        public VLCException (string message, Exception inner)
           : base (message, inner)
        {
        }
    };

    /**
     * libvlc_exception_t: structure for unmanaged LibVLC exceptions
     */
    [StructLayout (LayoutKind.Sequential)]
    public sealed class NativeException : IDisposable
    {
        int raised;
        int code;
        IntPtr message;

        [DllImport ("libvlc-control.dll", EntryPoint="libvlc_exception_init")]
        static extern void Init (NativeException e);
        [DllImport ("libvlc-control.dll", EntryPoint="libvlc_exception_clear")]
        static extern void Clear (NativeException e);
        /*[DllImport ("libvlc-control.dll",
                    EntryPoint="libvlc_exception_raised")]
        static extern int Raised (NativeException e);*/
        [DllImport ("libvlc-control.dll",
                    EntryPoint="libvlc_exception_get_message")]
        static extern IntPtr GetMessage (NativeException e);

        public NativeException ()
        {
            Init (this);
        }

        /**
         * Throws a managed exception if LibVLC has returned a native
         * unmanaged exception. Clears the native exception.
         */
        public void Raise ()
        {
            try
            {
                string msg = U8String.FromNative (GetMessage (this));
                if (msg != null)
                    throw new VLCException (msg);
            }
            finally
            {
                Clear (this);
            }
        }

        /** IDisposable implementation. */
        public void Dispose ()
        {
            Clear (this);
        }
    };
};
