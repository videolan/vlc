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

    /**
     * Managed class for LibVLC instance (including playlist)
     */
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


        [DllImport ("libvlc-control.dll", EntryPoint="libvlc_playlist_loop")]
        static extern void PlaylistLoop (InstanceHandle self, int b,
                                         NativeException ex);
        /** Sets the playlist loop flag */
        public bool Loop
        {
            set
            {
                PlaylistLoop (self, value ? 1 : 0, ex);
                ex.Raise ();
            }
        }

        [DllImport ("libvlc-control.dll", EntryPoint="libvlc_playlist_play")]
        static extern void PlaylistPlay (InstanceHandle self, int id, int optc,
                                         U8String[] optv, NativeException ex);
        /** Plays the next playlist item */
        public void Play ()
        {
            PlaylistPlay (self, -1, 0, new U8String[0], ex);
            ex.Raise ();
        }

        [DllImport ("libvlc-control.dll", EntryPoint="libvlc_playlist_pause")]
        static extern void PlaylistPause (InstanceHandle self,
                                          NativeException ex);
        /** Toggles pause */
        public void TogglePause ()
        {
            PlaylistPause (self, ex);
            ex.Raise ();
        }

        [DllImport ("libvlc-control.dll",
                    EntryPoint="libvlc_playlist_isplaying")]
        static extern int PlaylistIsPlaying (InstanceHandle self,
                                             NativeException ex);
        /** Whether the playlist is running, or in pause/stop */
        public bool IsPlaying
        {
            get
            {
                int ret = PlaylistIsPlaying (self, ex);
                ex.Raise ();
                return ret != 0;
            }
        }

        [DllImport ("libvlc-control.dll", EntryPoint="libvlc_playlist_stop")]
        static extern void PlaylistStop (InstanceHandle self,
                                         NativeException ex);
        /** Stops playing */
        public void Stop ()
        {
            PlaylistStop (self, ex);
            ex.Raise ();
        }

        [DllImport ("libvlc-control.dll", EntryPoint="libvlc_playlist_next")]
        static extern void PlaylistNext (InstanceHandle self,
                                         NativeException ex);
        /** Goes to next playlist item (and start playing it) */
        public void Next ()
        {
            PlaylistNext (self, ex);
            ex.Raise ();
        }

        [DllImport ("libvlc-control.dll", EntryPoint="libvlc_playlist_prev")]
        static extern void PlaylistPrev (InstanceHandle self,
                                         NativeException ex);
        /** Goes to previous playlist item (and start playing it) */
        public void Prev ()
        {
            PlaylistPrev (self, ex);
            ex.Raise ();
        }

        [DllImport ("libvlc-control.dll", EntryPoint="libvlc_playlist_clear")]
        static extern void PlaylistClear (InstanceHandle self,
                                          NativeException ex);
        /** Clears the whole playlist */
        public void Clear ()
        {
            PlaylistClear (self, ex);
            ex.Raise ();
        }

        [DllImport ("libvlc-control.dll", EntryPoint="libvlc_playlist_add")]
        static extern void PlaylistAdd (InstanceHandle self, U8String uri,
                                        U8String name, NativeException e);
        /** Appends an item to the playlist */
        public void Add (string mrl)
        {
            Add (mrl, null);
        }
        /** Appends an item to the playlist */
        public void Add (string mrl, string name)
        {
            U8String umrl = new U8String (mrl);
            U8String uname = new U8String (name);

            PlaylistAdd (self, umrl, uname, ex);
            ex.Raise ();
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
