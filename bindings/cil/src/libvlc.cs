/**
 * @file libvlc.cs
 * @brief Bindings to LibVLC for the .NET Common Intermediate Language
 * @ingroup API
 *
 * @defgroup API Managed interface to LibVLC
 * This is the primary class library for .NET applications
 * to embed and control LibVLC.
 *
 * @defgroup Internals LibVLC internals
 * This covers internal marshalling functions to use the native LibVLC.
 * Only VLC developpers should need to read this section.
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
     * @brief InstanceHandle: unmanaged LibVLC instance pointer
     * @ingroup Internals
     */
    internal sealed class InstanceHandle : NonNullHandle
    {
        [DllImport ("libvlc.dll", EntryPoint="libvlc_get_version")]
        public static extern IntPtr GetVersion ();

        [DllImport ("libvlc.dll", EntryPoint="libvlc_get_compiler")]
        public static extern IntPtr GetCompiler ();

        [DllImport ("libvlc.dll", EntryPoint="libvlc_get_changeset")]
        public static extern IntPtr GetChangeSet ();

        [DllImport ("libvlc.dll", EntryPoint="libvlc_new")]
        public static extern
        InstanceHandle Create (int argc, U8String[] argv,
                               NativeException ex);

        /*[DllImport ("libvlc.dll", EntryPoint="libvlc_retain")]
        public static extern void Hold (InstanceHandle h,
                                         NativeException ex);*/

        [DllImport ("libvlc.dll", EntryPoint="libvlc_release")]
        private static extern void Release (IntPtr h,
                                            NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_add_intf")]
        public static extern void AddInterface (InstanceHandle h,
                                                  U8String name,
                                                  NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_wait")]
        public static extern void Run (InstanceHandle h);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_get_vlc_instance")]
        public static extern NonNullHandle GetVLC (InstanceHandle h);

        protected override void Destroy ()
        {
            Release (handle, null);
        }
    };

    /**
     * @brief VLC: VLC media player instance
     * @ingroup API
     *
     * The VLC class provides represent a run-time instance of a media player.
     * An instance can spawn multiple independent medias, however
     * configuration settings, message logging, etc are common to all medias
     * from the same instance.
     */
    public class VLC : BaseObject
    {
        internal InstanceHandle Handle
        {
            get
            {
                return handle as InstanceHandle;
            }
        }

        /**
         * Loads the native LibVLC and creates a LibVLC instance.
         *
         * @param args VLC command line parameters for the LibVLC Instance.
         */
        public VLC (string[] args)
        {
            U8String[] argv = new U8String[args.Length];
            for (int i = 0; i < args.Length; i++)
                argv[i] = new U8String (args[i]);

            handle = InstanceHandle.Create (argv.Length, argv, ex);
            Raise ();
        }

        /**
         * Starts a VLC interface plugin.
         *
         * @param name name of the interface plugin (e.g. "http", "qt4", ...)
         */
        public void AddInterface (string name)
        {
            U8String uname = new U8String (name);

            InstanceHandle.AddInterface (Handle, uname, ex);
            Raise ();
        }

        /**
         * Waits until VLC instance exits. This can happen if a fatal error
         * occurs (e.g. cannot parse the arguments), if the user has quit
         * through an interface, or if the special vlc://quit item was played.
         */
        public void Run ()
        {
            InstanceHandle.Run (Handle);
        }

        /**
         * The human-readable LibVLC version number.
         */
        public static string Version
        {
            get
            {
                return U8String.FromNative (InstanceHandle.GetVersion ());
            }
        }

        /**
         * The human-readable LibVLC C compiler infos.
         */
        public static string Compiler
        {
            get
            {
                return U8String.FromNative (InstanceHandle.GetCompiler ());
            }
        }

        /**
         * The unique commit identifier from the LibVLC source control system,
         * or "exported" if unknown.
         */
        public static string ChangeSet
        {
            get
            {
                return U8String.FromNative (InstanceHandle.GetChangeSet ());
            }
        }

        /**
         * The unmanaged VLC-internal instance object.
         * Do not use this unless you really know what you are doing.
         */
        public SafeHandle Object
        {
            get
            {
                return InstanceHandle.GetVLC (Handle);
            }
        }
    };
};
