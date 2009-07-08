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
 * Only VLC developers should need to read this section.
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
    public class Player : EventingObject
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

        internal override EventManagerHandle GetManager ()
        {
            return LibVLC.PlayerEventManager (Handle, null);
        }

        /**
         * Whether the player is currently active.
         * @version VLC 1.0
         */
        public bool IsPlaying
        {
            get
            {
                int ret = LibVLC.PlayerIsPlaying (Handle, ex);
                Raise ();
                return ret != 0;
            }
        }

        /**
         * Starts playing the selected media.
         */
        public void Play ()
        {
            LibVLC.PlayerPlay (Handle, ex);
            Raise ();
        }

        /**
         * Pauses the playback.
         */
        public void Pause ()
        {
            LibVLC.PlayerPause (Handle, ex);
            Raise ();
        }

        /**
         * Stops the playback.
         */
        public void Stop ()
        {
            LibVLC.PlayerStop (Handle, ex);
            Raise ();
        }

        /**
         * The 32-bits identifier of an X Window System window,
         * or 0 if not specified.
         * Video will be rendered inside that window, if the underlying VLC
         * supports X11. Note that X pixmaps are <b>not</b> supported.
         * Also note that you should set/change/unset the window while
         * playback is not started or stopped; live reparenting might not
         * work.
         *
         * @warning If the identifier is invalid, Xlib might abort the process.
         * @version VLC 1.0
         */
        public int XWindow
        {
            get
            {
                return LibVLC.PlayerGetXWindow (Handle);
            }
            set
            {
                LibVLC.PlayerSetXWindow (Handle, value, ex);
                Raise ();
            }
        }

        /**
         * The handle of a window (HWND) from the Win32 API,
         * or NULL if unspecified.
         * Video will be rendered inside that window, if the underlying VLC
         * supports one of DirectDraw, Direct3D, GDI or OpenGL/Win32.
         * Note that you should set/change/unset the window while playback is
         * not started or stopped; live reparenting might not work.
         * @version VLC 1.0
         */
        public SafeHandle HWND
        {
            get
            {
                return LibVLC.PlayerGetHWND (Handle);
            }
            set
            {
                LibVLC.PlayerSetHWND (Handle, value, ex);
                Raise ();
            }
        }

        /**
         * Total length in milliseconds of the playback (if known).
         */
        public long Length
        {
            get
            {
                long ret = LibVLC.PlayerGetLength (Handle, ex);
                Raise ();
                return ret;
            }
        }

        /**
         * Playback position in milliseconds from the start (if applicable).
         * Setting this value might not work depending on the underlying
         * media capability and file format.
         *
         * Changing the Time will also change the Position.
         */
        public long Time
        {
            get
            {
                long ret = LibVLC.PlayerGetTime (Handle, ex);
                Raise ();
                return ret;
            }
            set
            {
                LibVLC.PlayerSetTime (Handle, value, ex);
                Raise ();
            }
        }

        /**
         * Playback position as a fraction of the total (if applicable).
         * At start, this is 0; at the end, this is 1.
         * Setting this value might not work depending on the underlying
         * media capability and file format.
         *
         * Changing the Position will also change the Time.
         */
        public float Position
        {
            get
            {
                float ret = LibVLC.PlayerGetPosition (Handle, ex);
                Raise ();
                return ret;
            }
            set
            {
                LibVLC.PlayerSetPosition (Handle, value, ex);
                Raise ();
            }
        }

        /**
         * Number of the current chapter (within the current title).
         * This is mostly used for DVDs and the likes.
         */
        public int Chapter
        {
            get
            {
                int ret = LibVLC.PlayerGetChapter (Handle, ex);
                Raise ();
                return ret;
            }
            set
            {
                LibVLC.PlayerSetChapter (Handle, value, ex);
                Raise ();
            }
        }

        /**
         * Number of chapters within the current title,
         */
        public int ChapterCount
        {
            get
            {
                int ret = LibVLC.PlayerGetChapterCount (Handle, ex);
                Raise ();
                return ret;
            }
        }

        /**
         * Gets the number of chapters within a given title.
         * @param title media title number
         * @version VLC 1.0
         */
        public int GetChapterCountByTitle (int title)
        {
            int ret = LibVLC.PlayerGetChapterCountForTitle (Handle, title, ex);
            Raise ();
            return ret;
        }

        /**
         * Number of the current title.
         * @version VLC 1.0
         */
        public int Title
        {
            get
            {
                int ret = LibVLC.PlayerGetTitle (Handle, ex);
                Raise ();
                return ret;
            }
            set
            {
                LibVLC.PlayerSetTitle (Handle, value, ex);
                Raise ();
            }
        }

        /**
         * Total number of titles.
         * @version VLC 1.0
         */
        public int TitleCount
        {
            get
            {
                int ret = LibVLC.PlayerGetTitleCount (Handle, ex);
                Raise ();
                return ret;
            }
        }

        /**
         * Skips to the beginning of the next chapter.
         * @version VLC 1.0
         */
        public void NextChapter ()
        {
            LibVLC.PlayerNextChapter (Handle, ex);
            Raise ();
        }

        /**
         * Rewinds to the previous chapter.
         * @version VLC 1.0
         */
        public void PreviousChapter ()
        {
            LibVLC.PlayerPreviousChapter (Handle, ex);
            Raise ();
        }

        /**
         * Media playback rate.
         * 1.0 is the nominal rate.
         * Less than one is slower than nominal.
         * More than one is faster than nominal.
         */
        public float Rate
        {
            get
            {
                float ret = LibVLC.PlayerGetRate (Handle, ex);
                Raise ();
                return ret;
            }
            set
            {
                LibVLC.PlayerSetRate (Handle, value, ex);
                Raise ();
            }
        }

        /**
         * Current state of the player.
         */
        public State State
        {
            get
            {
                State ret = LibVLC.PlayerGetState (Handle, ex);
                Raise ();
                return ret;
            }
        }

        /**
         * Frame rate in unit/seconds.
         */
        public float FramePerSeconds
        {
            get
            {
                float ret = LibVLC.PlayerGetFPS (Handle, ex);
                Raise ();
                return ret;
            }
        }

        /**
         * Whether a video track is currently active.
         * This is false if there is no video track, or if video is discarded.
         */
        public bool HasVideo
        {
            get
            {
                int ret = LibVLC.PlayerHasVout (Handle, ex);
                Raise ();
                return ret != 0;
            }
        }

        /**
         * Whether the media supports seeking.
         * Note that this tells nothing about the seeking precision.
         */
        public bool CanSeek
        {
            get
            {
                int ret = LibVLC.PlayerIsSeekable (Handle, ex);
                Raise ();
                return ret != 0;
            }
        }

        /**
         * Whether the media supports pausing.
         * Live content cannot be paused, unless timeshifting is enabled.
         */
        public bool CanPause
        {
            get
            {
                int ret = LibVLC.PlayerCanPause (Handle, ex);
                Raise ();
                return ret != 0;
            }
        }
    };
};
