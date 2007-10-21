/*
 * ustring.cs - Managed LibVLC string
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
     * Managed class for UTF-8 nul-terminated character arrays
     */
    [StructLayout (LayoutKind.Sequential)]
    public sealed struct U8String
    {
        public byte[] mb_str;

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

        public static string FromNative (IntPtr ptr)
        {
            return new U8String (ptr).ToString ();
        }
    };
};
