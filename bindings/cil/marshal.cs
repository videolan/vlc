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

namespace VideoLAN.VLC
{
    internal class MediaControlAPI
    {
        [DllImport ("libvlc-control.dll", EntryPoint="mediacontrol_new")]
        public static extern MediaControlHandle New (int argc, U8String[] argv, ref NativeException e);
        [DllImport ("libvlc-control.dll", EntryPoint="mediacontrol_exit")]
        public static extern void Exit (IntPtr self);

        [DllImport ("libvlc-control.dll", EntryPoint="mediacontrol_start")]
        public static extern void Start (MediaControlHandle self, IntPtr pos, ref NativeException e);
        [DllImport ("libvlc-control.dll", EntryPoint="mediacontrol_pause")]
        public static extern void Pause (MediaControlHandle self, IntPtr dummy, ref NativeException e);
        [DllImport ("libvlc-control.dll", EntryPoint="mediacontrol_resume")]
        public static extern void Resume (MediaControlHandle self, IntPtr dummy, ref NativeException e);
        [DllImport ("libvlc-control.dll", EntryPoint="mediacontrol_stop")]
        public static extern void Stop (MediaControlHandle self, IntPtr dummy, ref NativeException e);

        [DllImport ("libvlc-control.dll", EntryPoint="mediacontrol_playlist_add_item")]
        public static extern void PlaylistAddItem (MediaControlHandle self, U8String mrl, ref NativeException e);
        [DllImport ("libvlc-control.dll", EntryPoint="mediacontrol_playlist_clear")]
        public static extern void PlaylistClear (MediaControlHandle self, ref NativeException e);
        [DllImport ("libvlc-control.dll", EntryPoint="mediacontrol_playlist_get_list")]
        public static extern IntPtr PlaylistGetList (MediaControlHandle self, ref NativeException e);
        [DllImport ("libvlc-control.dll", EntryPoint="mediacontrol_playlist_next_item")]
        public static extern void PlaylistNextItem (MediaControlHandle self, ref NativeException e);
    }

    /**
     * Abstract safe handle class for non-NULL pointers
     * (Microsoft.* namespace has a similar class, but lets stick to System.*)
     */
    internal abstract class NonNullHandle : SafeHandle
    {
        protected NonNullHandle ()
            : base (IntPtr.Zero, true)
        {
        }

        public override bool IsInvalid
        {
            get
            {
                return IsClosed || (handle == IntPtr.Zero);
            }
        }
    };

    internal sealed class MediaControlHandle : NonNullHandle
    {
        private MediaControlHandle ()
        {
        }

        protected override bool ReleaseHandle ()
        {
            MediaControlAPI.Exit (handle);
            return true;
        }
    };

    /**
     * Wrapper around native UTF-8 nul-terminated character arrays
     */
    [StructLayout (LayoutKind.Sequential)]
    internal sealed struct U8String
    {
        byte[] mb_str;

        public U8String (string value)
        {
            byte[] bytes = System.Text.Encoding.UTF8.GetBytes (value);
            mb_str = new byte[bytes.Length + 1];
            Array.Copy (bytes, mb_str, bytes.Length);
            mb_str[bytes.Length] = 0;
        }

        public U8String (IntPtr ptr)
        {
            if (ptr == IntPtr.Zero)
                return;

            int i = 0;
            while (Marshal.ReadByte (ptr, i) != 0)
                i++;
            i++;

            mb_str = new byte[i];
            Marshal.Copy (ptr, mb_str, 0, i);
        }

        public override string ToString ()
        {
            if (mb_str == null)
                return null;

            byte[] bytes = new byte[mb_str.Length - 1];
            Array.Copy (mb_str, bytes, bytes.Length);

            return System.Text.Encoding.UTF8.GetString (bytes);
        }
    };


    /**
     * LibVLC native exception structure
     */
    [StructLayout (LayoutKind.Sequential)]
    internal sealed struct NativeException
    {
        public int code;
        public IntPtr message;

        public string Message
        {
            get
            {
                return new U8String (message).ToString ();
            }
        }

        [DllImport ("libvlc-control.dll")]
        static extern void mediacontrol_exception_init (ref NativeException e);
        [DllImport ("libvlc-control.dll")]
        static extern void mediacontrol_exception_cleanup (ref NativeException e);

        public static NativeException Prepare ()
        {
            NativeException e = new NativeException ();
            mediacontrol_exception_init (ref e);
            return e;
        }

        public void Consume ()
        {
            try
            {
                Exception e;
                string m = Message;

                switch (this.code)
                {
                    case 0:
                        e = null;
                        break;
                    case 1:
                        e = new PositionKeyNotSupportedException (m);
                        break;
                    case 2:
                        e = new PositionOriginNotSupportedException (m);
                        break;
                    case 3:
                        e = new InvalidPositionException (m);
                        break;
                    case 4:
                        e = new PlaylistException (m);
                        break;
                    case 5:
                        e = new InternalException (m);
                        break;
                    default:
                        e = new MediaException (m);
                        break;
                }
                if (e != null)
                    throw e;
            }
            finally
            {
                mediacontrol_exception_cleanup (ref this);
            }
        }
    };
};
