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
     * @brief PlayerHandle: unmanaged LibVLC media player pointer
     * @ingroup Internals
     */
    internal sealed class PlayerHandle : NonNullHandle
    {
        protected override void Destroy ()
        {
            LibVLC.PlayerRelease (handle);
        }
    };

    /**
     * @brief MediaPlayer: a simple media player
     * @ingroup API
     * Use this class to play a media.
     */
    public class Player : BaseObject
    {
        internal PlayerHandle Handle
        {
            get
            {
                return handle as PlayerHandle;
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

                LibVLC.PlayerSetMedia (Handle, mh, null);
                media = value;
            }
        }

        /**
         * Creates a player with no medias.
         * An input media will be needed before this media player can be used.
         *
         * @param instance VLC instance
         */
        public Player (VLC instance)
        {
            this.media = null;
            handle = LibVLC.PlayerCreate (instance.Handle, ex);
            Raise ();
        }

        /**
         * Creates a player object for a given a media.
         *
         * @param media media object
         */
        public Player (Media media)
        {
            this.media = media;
            handle = LibVLC.PlayerCreateFromMedia (media.Handle, ex);
            Raise ();
        }

    };
};
