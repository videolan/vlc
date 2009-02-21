/**
 * @file marshal.cs
 * @brief Common LibVLC objects marshalling utilities
 * @ingroup Internals
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
     * @brief NonNullHandle: abstract safe handle class for non-NULL pointers
     * @ingroup Internals
     * Microsoft.* namespace has a similar class. However we want to use the
     * System.* namespace only.
     */
    internal abstract class NonNullHandle : SafeHandle
    {
        protected NonNullHandle ()
            : base (IntPtr.Zero, true)
        {
        }

        /**
         * System.Runtime.InteropServices.SafeHandle::IsInvalid.
         */
        public override bool IsInvalid
        {
            get
            {
                return handle == IntPtr.Zero;
            }
        }

        /**
         * Destroys an handle. Cannot fail.
         */
        protected abstract void Destroy ();

        /**
         * System.Runtime.InteropServices.SafeHandle::ReleaseHandle.
         */
        protected override bool ReleaseHandle ()
        {
            Destroy ();
            return true;
        }

    };

    /**
     * @brief BaseObject: generic wrapper around a safe handle.
     * @ingroup Internals
     * This is the baseline for all managed LibVLC objects which wrap
     * an unmanaged LibVLC pointer.
     */
    public class BaseObject : IDisposable
    {
        protected NativeException ex; /**< buffer for LibVLC exceptions */
        protected SafeHandle handle; /**< wrapped safe handle */

        internal BaseObject ()
        {
            ex = new NativeException ();
            this.handle = null;
        }

        /**
         * Checks if the LibVLC run-time raised an exception
         * If so, raises a CIL exception.
         */
        protected void Raise ()
        {
            ex.Raise ();
        }

        /**
         * IDisposable::Dispose.
         */
        public void Dispose ()
        {
            Dispose (true);
            GC.SuppressFinalize (this);
        }

        protected virtual void Dispose (bool disposing)
        {
            if (disposing)
            {
                ex.Dispose ();
                if (handle != null)
                    handle.Close ();
            }
            ex = null;
            handle = null;
        }
    };
};
