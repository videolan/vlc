/**
 * @file media.cs
 * @brief Media descriptor class
 * @ingroup API
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
//using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace VideoLAN.LibVLC
{
    /**
     * @brief MediaHandle: unmanaged LibVLC media pointer
     * @ingroup Internals
     */
    internal sealed class MediaHandle : NonNullHandle
    {
        /**
         * NonNullHandle.Destroy
         */
        protected override void Destroy ()
        {
            LibVLC.MediaRelease (handle);
        }
    };

    /**
     * @brief MetaType: type of a media meta-information entry
     */
    public enum MetaType
    {
        Title,
        Artist,
        Genre,
        Copyright,
        Album,
        TrackNumber,
        Description,
        Rating,
        Date,
        Setting,
        URL,
        Language,
        NowPlaying,
        Publisher,
        EncodedBy,
        ArtworkURL,
        TrackID,
    };

    /**
     * @brief State: media/player state
     *
     * Media and Player objects are always in one of these state.
     * @see Media::State and @see Player::State.
     */
    public enum State
    {
        NothingSpecial, /**< Nothing going on */
        Opening, /**< Being opened */
        Buffering, /**< Buffering before play */
        Playing, /**< Playing */
        Paused, /**< Paused */
        Stopped, /**< Stopped */
        Ended, /**< Played until the end */
        Error, /**< Failed */
    };

    /* Media events */
    [StructLayout (LayoutKind.Sequential)]
    internal sealed class MediaMetaEvent : GenericEvent
    {
        public MetaType metaType;
    };
    internal delegate void MediaMetaCallback (MediaMetaEvent e, IntPtr d);

    /*[StructLayout (LayoutKind.Sequential)]
    internal sealed class MediaSubitemEvent : GenericEvent
    {
        public IntPtr child; -- MediaHandle
    };*/

    [StructLayout (LayoutKind.Sequential)]
    internal sealed class MediaDurationEvent : GenericEvent
    {
        public long duration;
    };
    internal delegate void MediaDurationCallback (MediaDurationEvent e,
                                                  IntPtr d);

    [StructLayout (LayoutKind.Sequential)]
    internal sealed class MediaPreparseEvent : GenericEvent
    {
        public int status;
    };
    internal delegate void MediaPreparseCallback (MediaPreparseEvent e,
                                                  IntPtr d);

    /* media_freed -> bad idea w.r.t. the GC */

    [StructLayout (LayoutKind.Sequential)]
    internal sealed class MediaStateEvent : GenericEvent
    {
        public State state;
    };
    internal delegate void MediaStateCallback (MediaStateEvent e, IntPtr d);

    /**
     * @brief Media: a source media
     * @ingroup API
     * Each media object represents an input media, such as a file or an URL.
     */
    public class Media : EventingObject, ICloneable
    {
        internal MediaHandle Handle
        {
            get
            {
                return handle as MediaHandle;
            }
        }

        /**
         * Creates a Media object.
         *
         * @param instance VLC instance
         * @param mrl Media Resource Locator (file path or URL)
         */
        public Media (VLC instance, string mrl)
        {
            U8String umrl = new U8String (mrl);

            handle = LibVLC.MediaCreate (instance.Handle, umrl, ex);
            Raise ();
            Attach ();
        }

        private Media (MediaHandle handle)
        {
            this.handle = handle;
            Attach ();
        }

        /**
         * Duplicates a media object.
         */
        public object Clone ()
        {
            return new Media (LibVLC.MediaDuplicate (Handle));
        }

        private void Attach ()
        {
            Attach (EventType.MediaMetaChanged,
                    new MediaMetaCallback (MetaCallback));
            //Attach (EventType.MediaSubItemAdded, SubItemAdded);
            Attach (EventType.MediaDurationChanged,
                    new MediaDurationCallback (DurationCallback));
            /*Attach (EventType.MediaPreparsedChanged,
                    new MediaPreparseCallback (PreparseCallback));*/
            /* MediaFreed: better not... */
            Attach (EventType.MediaStateChanged,
                    new MediaStateCallback (StateCallback));
        }

        /**
         * Add VLC input item options to the media.
         * @code
         * Media m = new Media(vlc, "http://www.example.com/music.ogg");
         * m.AddOptions(":http-user-agent=LibVLC.Net "
         *            + ":http-proxy=proxy:8080", true);
         * @endcode
         * @param options VLC options in VLC input item format
         *                (see example below)
         * @param trusted whether the options are set by a trusted agent
         *                (e.g. the local computer configuration) or not
         *                (e.g. a downloaded file).
         * @version VLC 0.9.9 if trusted is false
         */
        public void AddOptions (string options, bool trusted)
        {
            U8String uopts = new U8String (options);

            if (trusted)
                LibVLC.MediaAddOption (Handle, uopts, ex);
            else
                LibVLC.MediaAddUntrustedOption (Handle, uopts, ex);
            Raise ();
        }

        /**
         * The media location (file path, URL, ...).
         * @version VLC 1.0
         */
        public string Location
        {
            get
            {
                StringHandle str = LibVLC.MediaGetMRL (Handle, ex);
                Raise ();
                return str.Transform ();
            }
        }

        public override string ToString ()
        {
            return Location;
        }

        /**
         * Current state of the media.
         */
        public State State
        {
            get
            {
                State ret = LibVLC.MediaGetState (Handle, ex);
                Raise ();
                return ret;
            }
        }

        public delegate void StateChange (Media media, State state);
        public event StateChange StateChanged;
        private void StateCallback (MediaStateEvent ev, IntPtr data)
        {
            if (StateChanged != null)
                StateChanged (this, ev.state);
        }

        internal override EventManagerHandle GetManager ()
        {
            return LibVLC.MediaEventManager (Handle, null);
        }

        /**
         * Duration of the media in microseconds. The precision of the result
         * depends on the input stram protocol and file format. The value
         * might be incorrect and unknown (VLC usually returns 0 or -1 then).
         */
        public long Duration
        {
            get
            {
                long duration = LibVLC.MediaGetDuration (Handle, ex);
                Raise ();
                return duration;
            }
        }

        public delegate void DurationChange (Media media, long duration);
        public event DurationChange DurationChanged;
        private void DurationCallback (MediaDurationEvent ev, IntPtr data)
        {
            if (DurationChanged != null)
                DurationChanged (this, ev.duration);
        }

        /**
         * Whether the media was "preparsed". If true, the meta-infos were
         * extracted, even before the media was played. This is normally only
         * available if the input files is stored on a local filesystem.
         */
        public bool IsPreparsed
        {
            get
            {
                int preparsed = LibVLC.MediaIsPreparsed (Handle, ex);
                Raise ();
                return preparsed != 0;
            }
        }

        public delegate void PreparseChange (Media media, bool preparsed);
        public event PreparseChange PreparseChanged;
        private void PreparseCallback (MediaPreparseEvent ev, IntPtr data)
        {
            if (PreparseChanged != null)
                PreparseChanged (this, ev.status != 0);
        }
    };
};
