    /*****************************************************************************
 * LibVlcEventType.java: VLC Java Bindings event types enum
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 *
 * Authors: Filippo Carone <filippo@carone.org>
 *
 *
 * $Id $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

package org.videolan.jvlc.internal;


public enum LibVlcEventType {

    libvlc_MediaMetaChanged, // 0
    libvlc_MediaSubItemAdded, // 1
    libvlc_MediaDurationChanged, // 2
    libvlc_MediaPreparsedChanged, // 3
    libvlc_MediaFreed, // 4
    libvlc_MediaStateChanged, // 5

    libvlc_MediaPlayerNothingSpecial, // 6
    libvlc_MediaPlayerOpening, // 7
    libvlc_MediaPlayerBuffering, // 8
    libvlc_MediaPlayerPlaying, // 9
    libvlc_MediaPlayerPaused, // 10
    libvlc_MediaPlayerStopped, // 11
    libvlc_MediaPlayerForward, // 12
    libvlc_MediaPlayerBackward, // 13
    libvlc_MediaPlayerEndReached, // 14
    libvlc_MediaPlayerEncounteredError, // 15
    libvlc_MediaPlayerTimeChanged, // 16
    libvlc_MediaPlayerPositionChanged, // 17
    libvlc_MediaPlayerSeekableChanged, // 18
    libvlc_MediaPlayerPausableChanged, // 19

    libvlc_MediaListItemAdded, // 20
    libvlc_MediaListWillAddItem, // 21
    libvlc_MediaListItemDeleted, // 22
    libvlc_MediaListWillDeleteItem, // 23

    libvlc_MediaListViewItemAdded, // 24
    libvlc_MediaListViewWillAddItem, // 25
    libvlc_MediaListViewItemDeleted, // 26
    libvlc_MediaListViewWillDeleteItem, // 27

    libvlc_MediaListPlayerPlayed, // 28
    libvlc_MediaListPlayerNextItemSet, // 29
    libvlc_MediaListPlayerStopped, // 30

    libvlc_MediaDiscovererStarted, // 31
    libvlc_MediaDiscovererEnded, // 32

    libvlc_MediaPlayerTitleChanged, // 33
    libvlc_MediaPlayerSnapshotTaken, // 34
    libvlc_MediaPlayerLengthChanged, // 35

    libvlc_VlmMediaAdded, // 36
    libvlc_VlmMediaRemoved, // 37
    libvlc_VlmMediaChanged, // 38
    libvlc_VlmMediaInstanceStarted, // 39
    libvlc_VlmMediaInstanceStopped, // 40
    libvlc_VlmMediaInstanceStatusInit, // 41
    libvlc_VlmMediaInstanceStatusOpening, // 42
    libvlc_VlmMediaInstanceStatusPlaying, // 43
    libvlc_VlmMediaInstanceStatusPause, // 44
    libvlc_VlmMediaInstanceStatusEnd, // 45
    libvlc_VlmMediaInstanceStatusError; // 46
}
