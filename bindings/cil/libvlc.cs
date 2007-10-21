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
    /** Safe handle for unmanaged LibVLC instance pointer */
    internal sealed class InstanceHandle : NonNullHandle
    {
        private InstanceHandle ()
        {
        }

        [DllImport ("libvlc-control.dll", EntryPoint="libvlc_new")]
        public static extern
        InstanceHandle Create (int argc, U8String[] argv, NativeException ex);

        [DllImport ("libvlc-control.dll", EntryPoint="libvlc_destroy")]
        static extern void Destroy (InstanceHandle ptr, NativeException ex);

        protected override bool ReleaseHandle ()
        {
            Destroy (this, null);
            return true;
        }
    };

    public class VLCInstance : IDisposable
    {
        NativeException ex;
        InstanceHandle self;

        public VLCInstance (string[] args)
        {
            U8String[] argv = new U8String[args.Length];
            for (int i = 0; i < args.Length; i++)
                argv[i] = new U8String (args[i]);

            ex = new NativeException ();
            self = InstanceHandle.Create (argv.Length, argv, ex);
            ex.Raise ();
        }

        public void Dispose ()
        {
            ex.Dispose ();
            self.Close ();
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
    internal sealed class DescriptorHandle : NonNullHandle
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
        public static extern void Release (DescriptorHandle ptr);

        protected override bool ReleaseHandle ()
        {
            Release (this);
            return true;
        }
    };

    public class MediaDescriptor
    {
        NativeException ex;
        DescriptorHandle self;

        internal MediaDescriptor (DescriptorHandle self)
        {
            this.self = self;
            ex = new NativeException ();
        }
    };
};
