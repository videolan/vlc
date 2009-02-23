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
        int code;
        /**
         * VLC exception code.
         */
        public int Code
        {
            get
            {
                return code;
            }
        }

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

        /**
         * Creates a VLC exception
         * @param code VLC exception code
         * @param message VLC exception message
         */
        public VLCException (int code, string message) : base (message)
        {
            this.code = code;
        }

        /**
         * Creates a VLC exception
         * @param code VLC exception code
         */
        public VLCException (int code) : base ()
        {
            this.code = code;
        }
    };

    /**
     * @brief NativeException: CIL representation for libvlc_exception_t.
     * @ingroup Internals
     */
    [StructLayout (LayoutKind.Sequential)]
    public sealed class NativeException : IDisposable
    {
        int raised;
        int code;
        IntPtr message;

        public NativeException ()
        {
            LibVLC.ExceptionInit (this);
        }

        /**
         * Throws a managed exception if LibVLC has returned a native
         * unmanaged exception. Clears the native exception.
         */
        public void Raise ()
        {
            if (raised == 0)
                return;

            string msg = U8String.FromNative (message);
            try
            {
                if (msg != null)
                    throw new VLCException (code, msg);
                else
                    throw new VLCException (code);
            }
            finally
            {
                LibVLC.ExceptionClear (this);
            }
        }

        /** IDisposable implementation. */
        public void Dispose ()
        {
            Dispose (true);
            GC.SuppressFinalize (this);
        }

        ~NativeException ()
        {
            Dispose (false);
        }

        private void Dispose (bool disposing)
        {
            LibVLC.ExceptionClear (this);
        }
    };
};
