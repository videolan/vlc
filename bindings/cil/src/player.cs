/**
 * @file player.cs
 * @brief Media player class
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
 *  Copyright (C) 2009 Rémi Denis-Courmont.                           *
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
     * @brief MediaPlayerHandle: unmanaged LibVLC media player pointer
     * @ingroup Internals
     */
    internal sealed class MediaPlayerHandle : NonNullHandle
    {
        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_new")]
        internal static extern
        MediaPlayerHandle Create (InstanceHandle inst, NativeException ex);
        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_new_from_media")]
        internal static extern
        MediaPlayerHandle Create (MediaHandle media, NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_release")]
        internal static extern void Release (IntPtr ptr);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_set_media")]
        internal static extern
        MediaPlayerHandle SetMedia (MediaPlayerHandle player,
                                    MediaHandle media,
                                    NativeException ex);

        protected override void Destroy ()
        {
            Release (handle);
        }
    };

    /**
     * @brief MediaPlayer: a simple media player
     * Use this class to play a media.
     */
    public class MediaPlayer : BaseObject
    {
        internal MediaPlayerHandle Handle
        {
            get
            {
                return handle as MediaPlayerHandle;
            }
        }

        Media media; /**< Active media */
        /**
         * The Media object that the MediaPlayer is using,
         * or null if there is none.
         */
        public Media Media
        {
            get
            {
                return media;
            }
            set
            {
                MediaHandle mh = (value != null) ? value.Handle : null;

                MediaPlayerHandle.SetMedia (Handle, mh, null);
                media = value;
            }
        }

        /**
         * Creates an empty MediaPlayer object.
         * An input media will be needed before this media player can be used.
         *
         * @param instance VLC instance
         */
        public MediaPlayer (VLC instance)
        {
            this.media = null;
            handle = MediaPlayerHandle.Create (instance.Handle, ex);
            ex.Raise ();
        }

        /**
         * Creates a MediaPlayer object from a Media object.
         * This allows playing the specified media.
         *
         * @param media media object
         */
        public MediaPlayer (Media media)
        {
            this.media = media;
            handle = MediaPlayerHandle.Create (media.Handle, ex);
            ex.Raise ();
        }

    };
};
