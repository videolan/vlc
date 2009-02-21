/**
 * @file exception.cs
 * @brief LibVLC exceptions
 * @ingroup API
 */

/**********************************************************************
 *  Copyright (C) 2007-2009 Rémi Denis-Courmont.                      *
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
     * @brief VLCException: base class for LibVLC exceptions
     * @ingroup API
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
     * @section Internals
     */

    /**
     * libvlc_exception_t: structure for unmanaged LibVLC exceptions
     */
    [StructLayout (LayoutKind.Sequential)]
    public sealed class NativeException : IDisposable
    {
        int raised;
        int code;
        IntPtr message;

        [DllImport ("libvlc.dll", EntryPoint="libvlc_exception_init")]
        private static extern void Init (NativeException e);
        [DllImport ("libvlc.dll", EntryPoint="libvlc_exception_clear")]
        private static extern void Clear (NativeException e);
        /*[DllImport ("libvlc.dll",
                    EntryPoint="libvlc_exception_raised")]
        private static extern int Raised (NativeException e);*/
        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_exception_get_message")]
        private static extern IntPtr GetMessage (NativeException e);

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
