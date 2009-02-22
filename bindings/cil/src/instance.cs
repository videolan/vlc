/**
 * @file instance.cs
 * @brief LibVLC instance class
 * @ingroup API
 *
 * @defgroup API Managed interface to LibVLC
 *
 * This is the primary class library for .NET applications
 * to embed and control LibVLC.
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

/**
 * @brief VideoLAN.LibVLC: VideoLAN project's LibVLC .Net bindings
 * @ingroup API
 *
 * This namespace provides a set of managed APIs around the native LibVLC
 * for the .Net Common Language Runtime.
 */
namespace VideoLAN.LibVLC
{
    /**
     * @brief InstanceHandle: unmanaged LibVLC instance pointer
     * @ingroup Internals
     */
    internal sealed class InstanceHandle : NonNullHandle
    {
        /**
         * NonNullHandle.Destroy
         */
        protected override void Destroy ()
        {
            LibVLC.Release (handle, null);
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

            handle = LibVLC.Create (argv.Length, argv, ex);
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

            LibVLC.AddIntf (Handle, uname, ex);
            Raise ();
        }

        /**
         * Waits until VLC instance exits. This can happen if a fatal error
         * occurs (e.g. cannot parse the arguments), if the user has quit
         * through an interface, or if the special vlc://quit item was played.
         */
        public void Run ()
        {
            LibVLC.Wait (Handle);
        }

        /**
         * The human-readable LibVLC version number.
         */
        public static string Version
        {
            get
            {
                return U8String.FromNative (LibVLC.GetVersion ());
            }
        }

        /**
         * The human-readable LibVLC C compiler infos.
         */
        public static string Compiler
        {
            get
            {
                return U8String.FromNative (LibVLC.GetCompiler ());
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
                return U8String.FromNative (LibVLC.GetChangeset ());
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
                return LibVLC.GetVLCInstance (Handle);
            }
        }
    };
};

/**
 * @mainpage libvlc-cil documentation
 *
 * @section Introduction
 *
 * libvlc-cil is a thin API layer around LibVLC,
 * the VideoLAN's project media framework C library.
 * LibVLC comes built-in the VLC media player.
 *
 * With libvlc-cil, you can use LibVLC
 * from within the .Net Common Language Runtime,
 * with the CIL programming language of your preference.
 * However, libvlc-cil and the code sample in this documentation
 * are written is C#.
 *
 * @section Installation
 *
 * libvlc-cil does <b>not</b> include the underlying LibVLC by default.
 * Make sure VLC, or at least LibVLC and the relevant VLC plugins are present.
 * The LibVLC runtime library needs to be in the library search path for the
 * Common Language Runtime. Check the documentation for your CLR for details.
 *
 * On Windows, libvlc.dll needs to be in the current directory, the DLL path
 * of the running process, or the system search path (but the latter is
 * uncommon and not supported by the official LibVLC installer).
 *
 * On Linux, libvlc.so should be in /usr/lib; if it is in another directory,
 * such as /usr/local/lib, you might need to add that directory to the
 * LD_LIBRARY_PATH environment variable.
 *
 * @section Usage
 *
 * First, you need to create a VLC instance. This will load and setup the
 * native VLC runtime, the VLC configuration, the list of available plugins,
 * the platform adaptation and the VLC log messages and objects subsystems
 * @code
 * using System;
 * using VideoLAN.LibVLC;
 * ...
 *
 * try {
 *     Console.WriteLine("Running on VLC version {0}", VLC.Version);
 * }
 * catch (Exception e) {
 *     Console.WriteLine("VLC is not available on your system.");
 *     throw e;
 * }
 *
 * string[] args = new string[]{ "-v", "--ignore-config" };
 * VLC vlc = new VLC(args);
 * @endcode
 * @see VideoLAN::LibVLC::VLC
 *
 * To play media, you need a media and a player.
 * @code
 * Media media = new Media(vlc, "http://www.example.com/video.ogv");
 * Player player = new Player(media);
 * player.Play();
 * @endcode
 * @see VideoLAN::LibVLC::Media @see VideoLAN::LibVLC::Player
 *
 * All these objects use unmanaged resources.
 * They all implement the IDisposeable interface.
 * You should dispose them when you do not need them anymore:
 * @code
 * player.Dispose();
 * media.Dispose();
 * vlc.Dispose();
 * @endcode
 */
