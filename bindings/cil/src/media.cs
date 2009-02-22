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
     * @brief Media: a source media
     * @ingroup API
     * Each media object represents an input media, such as a file or an URL.
     */
    public class Media : BaseObject, ICloneable
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
        }

        /**
         * Add VLC input item options to the media.
         * @param options VLC options in VLC input item format
         *                (see example below)
         * @param trusted whether the options are set by a trusted agent
         *                (e.g. the local computer configuration) or not
         *                (e.g. a downloaded file).
         * @code
         * Media m = new Media(vlc, "http://www.example.com/music.ogg");
         * m.AddOptions(":http-user-agent=LibVLC.Net "
         *            + ":http-proxy=proxy:8080", true);
         * @endcode
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

        private Media (MediaHandle handle)
        {
            this.handle = handle;
        }

        /**
         * Duplicates a media object.
         */
        public object Clone ()
        {
            return new Media (LibVLC.MediaDuplicate (Handle));
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
    };
};
