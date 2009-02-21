/**
 * @file libvlc.cs
 * @brief Unmanaged LibVLC APIs
 * @ingroup Internals
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
     * @brief Native: unmanaged LibVLC APIs
     * @ingroup Internals
     */
    internal static class LibVLC
    {
        /* core.c */
        [DllImport ("libvlc.dll", EntryPoint="libvlc_get_version")]
        public static extern IntPtr GetVersion ();

        [DllImport ("libvlc.dll", EntryPoint="libvlc_get_compiler")]
        public static extern IntPtr GetCompiler ();

        [DllImport ("libvlc.dll", EntryPoint="libvlc_get_changeset")]
        public static extern IntPtr GetChangeset ();

        [DllImport ("libvlc.dll", EntryPoint="libvlc_new")]
        public static extern
        InstanceHandle Create (int argc, U8String[] argv, NativeException ex);

        /*[DllImport ("libvlc.dll", EntryPoint="libvlc_retain")]
        public static extern
        void Retain (InstanceHandle h, NativeException ex);*/

        [DllImport ("libvlc.dll", EntryPoint="libvlc_release")]
        public static extern
        void Release (IntPtr h, NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_add_intf")]
        public static extern
        void AddIntf (InstanceHandle h, U8String name, NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_wait")]
        public static extern
        void Wait (InstanceHandle h);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_get_vlc_instance")]
        public static extern
        SafeHandle GetVLCInstance (InstanceHandle h);

        /* media.c */
        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_new")]
        public static extern
        MediaHandle MediaCreate (InstanceHandle inst, U8String mrl,
                                 NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_new_as_node")]
        public static extern
        MediaHandle MediaCreateAsNode (InstanceHandle inst, U8String name,
                                       NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_add_option")]
        public static extern
        void MediaAddOption (MediaHandle media, U8String options,
                             NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_add_option_untrusted")]
        public static extern
        void MediaAddUntrustedOption (MediaHandle media, U8String options,
                                      NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_release")]
        public static extern
        void MediaRelease (IntPtr ptr);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_get_mrl")]
        public static extern
        void MediaGetMRL (MediaHandle media);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_duplicate")]
        public static extern
        MediaHandle MediaDuplicate (MediaHandle media);

        /*[DllImport ("libvlc.dll", EntryPoint="libvlc_media_read_meta")]
        public static extern
        MediaHandle MediaDuplicate (MediaHandle media, int type,
                                    NativeException ex);*/

        /*[DllImport ("libvlc.dll", EntryPoint="libvlc_media_get_state")]
        public static extern
        int MediaGetState (MediaHandle media, NativeException ex);*/

        /*[DllImport ("libvlc.dll", EntryPoint="libvlc_media_subitems")]
        public static extern
        MediaListHandle MediaSubItems (MediaHandle media, NativeException ex);*/

        /*[DllImport ("libvlc.dll", EntryPoint="libvlc_media_get_state")]
        public static extern
        EventManagerHandle MediaGetEventManager (MediaHandle media,
                                                 NativeException ex);*/

        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_get_duration")]
        public static extern
        long MediaGetDuration (MediaHandle media, NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_is_preparsed")]
        public static extern
        int MediaIsPreparsed (MediaHandle media, NativeException ex);

        /*[DllImport ("libvlc.dll", EntryPoint="libvlc_media_set_user_data")]
        public static extern
        void MediaIsPreparsed (MediaHandle media, IntPtr data,
                               NativeException ex);*/

        /*[DllImport ("libvlc.dll", EntryPoint="libvlc_media_get_user_data")]
        public static extern
        IntPtr MediaIsPreparsed (MediaHandle media, NativeException ex);*/
    };
};
