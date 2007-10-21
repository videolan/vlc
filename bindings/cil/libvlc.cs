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
    public sealed class VLC
    {
        public static Instance CreateInstance (string[] args)
        {
            U8String[] argv = new U8String[args.Length];
            for (int i = 0; i < args.Length; i++)
                argv[i] = new U8String (args[i]);

            NativeException ex = new NativeException ();

            InstanceHandle h = InstanceHandle.Create (argv.Length, argv, ex);
            ex.Raise ();

            return new Instance (h);
        }
    };

    /** Safe handle for unmanaged LibVLC instance pointer */
    public sealed class InstanceHandle : NonNullHandle
    {
        private InstanceHandle ()
        {
        }

        [DllImport ("libvlc-control.dll", EntryPoint="libvlc_new")]
        public static extern
        InstanceHandle Create (int argc, U8String[] argv, NativeException ex);

        [DllImport ("libvlc-control.dll", EntryPoint="libvlc_destroy")]
        static extern void Destroy (IntPtr ptr, NativeException ex);

        protected override bool ReleaseHandle ()
        {
            Destroy (handle, null);
            return true;
        }
    };

    public class Instance : BaseObject<InstanceHandle>
    {
        internal Instance (InstanceHandle self) : base (self)
        {
        }

        public MediaDescriptor CreateDescriptor (string mrl)
        {
            U8String umrl = new U8String (mrl);
            DescriptorHandle dh = DescriptorHandle.Create (self, umrl, ex);
            ex.Raise ();

            return new MediaDescriptor (dh);
        }
    };

    /** Safe handle for unmanaged LibVLC media descriptor */
    public sealed class DescriptorHandle : NonNullHandle
    {
        private DescriptorHandle ()
        {
        }

        [DllImport ("libvlc-control.dll",
                    EntryPoint="libvlc_media_descriptor_new")]
        public static extern
        DescriptorHandle Create (InstanceHandle inst, U8String mrl,
                                 NativeException ex);

        [DllImport ("libvlc-control.dll",
                    EntryPoint="libvlc_media_descriptor_release")]
        public static extern void Release (IntPtr ptr);

        protected override bool ReleaseHandle ()
        {
            Release (handle);
            return true;
        }
    };

    public class MediaDescriptor : BaseObject<DescriptorHandle>
    {
        internal MediaDescriptor (DescriptorHandle self) : base (self)
        {
        }
    };
};
