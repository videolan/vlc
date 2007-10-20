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

namespace VideoLAN.VLC
{
    /**
     * Base class for managed LibVLC exceptions
     */
    public class MediaException : Exception
    {
        public MediaException ()
        {
        }

        public MediaException (string message)
            : base (message)
        {
        }

        public MediaException (string message, Exception inner)
           : base (message, inner)
        {
        }
    };

    public class PositionKeyNotSupportedException : MediaException
    {
        public PositionKeyNotSupportedException ()
        {
        }

        public PositionKeyNotSupportedException (string message)
            : base (message)
        {
        }

        public PositionKeyNotSupportedException (string message, Exception inner)
           : base (message, inner)
        {
        }
    };

    public class PositionOriginNotSupportedException : MediaException
    {
        public PositionOriginNotSupportedException ()
        {
        }

        public PositionOriginNotSupportedException (string message)
            : base (message)
        {
        }

        public PositionOriginNotSupportedException (string message, Exception inner)
           : base (message, inner)
        {
        }
    };

    public class InvalidPositionException : MediaException
    {
        public InvalidPositionException ()
        {
        }

        public InvalidPositionException (string message)
            : base (message)
        {
        }

        public InvalidPositionException (string message, Exception inner)
           : base (message, inner)
        {
        }
    };

    public class PlaylistException : MediaException
    {
        public PlaylistException ()
        {
        }

        public PlaylistException (string message)
            : base (message)
        {
        }

        public PlaylistException (string message, Exception inner)
           : base (message, inner)
        {
        }
    };

    public class InternalException : MediaException
    {
        public InternalException ()
        {
        }

        public InternalException (string message)
            : base (message)
        {
        }

        public InternalException (string message, Exception inner)
           : base (message, inner)
        {
        }
    };
};
