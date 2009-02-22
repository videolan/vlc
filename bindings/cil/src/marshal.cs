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
using System.Collections;
using System.Collections.Generic;
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
     * @brief BaseObject: generic wrapper around a safe LibVLC handle.
     * @ingroup Internals
     *
     * This is the baseline for all managed LibVLC objects. It wraps:
     *  - an unmanaged LibVLC pointer,
     *  - a native exception structure.
     */
    public class BaseObject : IDisposable
    {
        internal NativeException ex; /**< buffer for LibVLC exceptions */
        internal SafeHandle handle; /**< wrapped safe handle */

        internal BaseObject ()
        {
            ex = new NativeException ();
            this.handle = null;
        }

        /**
         * Checks if the LibVLC run-time raised an exception
         * If so, raises a CIL exception.
         */
        internal void Raise ()
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

        /**
         * Releases unmanaged resources associated with the object.
         * @param disposing true if the disposing the object explicitly,
         *                  false if finalizing the object inside the GC.
         */
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

    internal class EventManagerHandle : NonNullHandle
    {
        protected override void Destroy ()
        {
        }
    };


    /**
     * @brief EventingObject: wrapper around an eventing LibVLC handle.
     * @ingroup Internals
     *
     * This is the base class for all managed LibVLC objects which do have an
     * event manager.
     */
    public abstract class EventingObject : BaseObject
    {
        /**
         * @brief Managed to unmanaged event handler mapping
         * @ingroup Internals
         *
         * The CLR cannot do reference counting for unmanaged callbacks.
         * We keep track of handled events here instead.
         */
        private class Event
        {
            public EventCallback managed;
            public IntPtr        unmanaged;

            public Event (EventCallback managed, IntPtr unmanaged)
            {
                this.managed = managed;
                this.unmanaged = unmanaged;
            }
        };
        private Dictionary<EventType, Event> events;
        /**< references to our unmanaged function pointers */

        internal EventingObject () : base ()
        {
            events = new Dictionary<EventType, Event> ();
        }

        /**
         * Releases unmanaged resources associated with the object.
         * @param disposing true if the disposing the object explicitly,
         *                  false if finalizing the object inside the GC.
         */
        protected override void Dispose (bool disposing)
        {
            events = null;
            base.Dispose (disposing);
        }

        /**
         * @return the unmanaged event manager for this object
         */
        internal abstract EventManagerHandle GetManager ();

        /**
         * Registers an event handler.
         * @param type event type to register to
         * @param callback callback to invoke when the event occurs
         *
         * @note
         * For simplicity, we only allow one handler per type.
         * Multicasting can be implemented higher up with managed code.
         */
        internal void Attach (EventType type, EventCallback callback)
        {
            EventManagerHandle manager;
            IntPtr cb = Marshal.GetFunctionPointerForDelegate (callback);
            Event ev = new Event (callback, cb);
            bool unref = false;

            if (events.ContainsKey (type))
                throw new ArgumentException ("Duplicate event");

            try
            {
                handle.DangerousAddRef (ref unref);
                manager = GetManager ();
                LibVLC.EventAttach (manager, type, cb, IntPtr.Zero, ex);
            }
            finally
            {
                if (unref)
                    handle.DangerousRelease ();
            }
            Raise ();
            events.Add (type, ev);
        }

        private void Detach (EventType type, IntPtr callback)
        {
            EventManagerHandle manager;
            bool unref = false;

            try
            {
                handle.DangerousAddRef (ref unref);
                manager = GetManager ();
                LibVLC.EventDetach (manager, type, callback, IntPtr.Zero, ex);
            }
            finally
            {
                if (unref)
                    handle.DangerousRelease ();
            }
            Raise ();
            events.Remove (type);
        }

        internal void Detach (EventType type)
        {
            Detach(type, events[type].unmanaged);
        }
    };
};
