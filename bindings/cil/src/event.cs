/**
 * @file event.cs
 * @brief Unmanaged LibVLC events
 * @ingroup Internals
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
     * @ingroup Internals
     * @{
     */

    /**
     * @brief EventType: LibVLC event types
     */
    internal enum EventType
    {
        MediaMetaChanged,
        MediaSubItemAdded,
        MediaDurationChanged,
        MediaPreparsedChanged,
        MediaFreed,
        MediaStateChanged,

        PlayerNothingSpecial,
        PlayerOpening,
        PlayerBuffering,
        PlayerPlaying,
        PlayerPaused,
        PlayerStopped,
        PlayerForward,
        PlayerBackward,
        PlayerEndReached,
        PlayerEncounteredError,
        PlayerTimeChanged,
        PlayerPositionChanged,
        PlayerSeekableChanged,
        PlayerPausableChanged,

        ListItemAdded,
        ListWillAddItem,
        ListItemDeleted,
        ListWillDeleteItem,

        ListViewItemAdded,
        ListViewWillAddItem,
        ListViewItemDeleted,
        ListViewWillDeleteItem,

        ListPlayerPlayed,
        ListPlayerNextItemSet,
        ListPlayerStopped,

        DiscovererStarted,
        DiscovererEnded,

        PlayerTitleChanged,
    };

    [StructLayout (LayoutKind.Sequential)]
    internal class GenericEvent
    {
        public EventType type;
        public IntPtr    obj;
    };
    internal delegate void GenericCallback (GenericEvent e, IntPtr d);

    /* Player events */
    [StructLayout (LayoutKind.Sequential)]
    internal sealed class PlayerPositionEvent : GenericEvent
    {
        float     position;
    };

    [StructLayout (LayoutKind.Sequential)]
    internal sealed class PlayerTimeEvent : GenericEvent
    {
        long      time;
    };

    [StructLayout (LayoutKind.Sequential)]
    internal sealed class PlayerTitleEvent : GenericEvent
    {
        int       title;
    };

    [StructLayout (LayoutKind.Sequential)]
    internal sealed class PlayerSeekableEvent : GenericEvent
    {
        long      seekable;
    };

    [StructLayout (LayoutKind.Sequential)]
    internal sealed class PlayerPausableChangedEvent : GenericEvent
    {
        long      pausable;
    };
    /** @} */
};