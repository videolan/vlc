/**
 * @file ustring.cs
 * @brief Managed LibVLC strings
 * @ingroup Internals
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
     * @brief U8String: Native UTF-8 characters array
     * @ingroup Internals
     * This supports conversion between native UTF-8 nul-terminated characters
     * arrays (as used by the native LibVLC) and managed strings.
     */
    [StructLayout (LayoutKind.Sequential)]
    internal struct U8String
    {
        public byte[] mb_str; /**< nul-terminated UTF-8 bytes array */

        /**
         * Creates an UTF-8 characters array from a .NET string.
         * @param value string to convert
         */
        public U8String (string value)
        {
            mb_str = null;
            if (value == null)
                return;

            byte[] bytes = System.Text.Encoding.UTF8.GetBytes (value);
            mb_str = new byte[bytes.Length + 1];
            Array.Copy (bytes, mb_str, bytes.Length);
            mb_str[bytes.Length] = 0;
        }

        private U8String (IntPtr ptr)
        {
            mb_str = null;
            if (ptr == IntPtr.Zero)
                return;

            int i = 0;
            while (Marshal.ReadByte (ptr, i) != 0)
                i++;
            i++;

            mb_str = new byte[i];
            Marshal.Copy (ptr, mb_str, 0, i);
        }

        /**
         * Object::ToString.
         */
        public override string ToString ()
        {
            if (mb_str == null)
                return null;

            byte[] bytes = new byte[mb_str.Length - 1];
            Array.Copy (mb_str, bytes, bytes.Length);

            return System.Text.Encoding.UTF8.GetString (bytes);
        }

        /**
         * Converts a pointer to a nul-terminated UTF-8 characters array into
         * a managed string.
         */
        public static string FromNative (IntPtr ptr)
        {
            return new U8String (ptr).ToString ();
        }
    };

    /**
     * @brief MemoryHandle: heap allocation by the C run-time
     * @ingroup Internals
     */
    internal class MemoryHandle : NonNullHandle
    {
        [DllImport ("libvlc.dll", EntryPoint="libvlc_free")]
        private static extern void Free (IntPtr ptr);

        /**
         * NonNullHandle.Destroy
         */
        protected override void Destroy ()
        {
            Free (handle);
        }
    };

    /**
     * @brief StringHandle: heap-allocated characters array
     * @ingroup Internals
     */
    internal sealed class StringHandle : MemoryHandle
    {
        /**
         * Converts an heap-allocated nul-terminated UTF-8 characters array
         * into a managed string.
         * @return the resulting managed string.
         */
        public override string ToString ()
        {
            return U8String.FromNative (handle);
        }

        /**
         * Converts the buffer (as in ToString()) and release it.
         * @return managed string representation of the buffer
         */
        public string Transform ()
        {
            string value = ToString ();
            Close ();
            return value;
        }
    };
};
