/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
import QtQuick
import QtQuick.Controls
import "qrc:///style/"
import org.videolan.vlc 0.1

// targetWidth: concerned widget's current width
// sourceWidth: target's width is bounded by this value (parent's width?)
// HorizontalResizeHandle actually doesn't resizes target
// you have to assign target's width manually using widthFactor property
// the idea behind using widthFactor is to maintain scale ratio when source itself resizes
// e.g target.width: Helpers.clamp(sourceWidth / resizeHandle.widthFactor, minimumWidth, maximumWidth)
MouseArea {
    id: root

    // provided by parent, this widget don't modify these properties
    property int sourceWidth
    property int targetWidth

    property double widthFactor: 4
    property bool atRight: true

    property int _previousX

    cursorShape: Qt.SplitHCursor
    width: VLCStyle.resizeHandleWidth
    acceptedButtons: Qt.LeftButton

    onPressed: (mouse) => {
        MainCtx.setCursor(cursorShape)
        _previousX = mouseX
    }

    onReleased:(mouse) => {
        MainCtx.restoreCursor()
    }

    onPositionChanged: {
        const f = atRight ? -1 : 1
        const delta = mouseX - _previousX

        root.widthFactor = root.sourceWidth / (root.targetWidth + (delta * - f))
    }

}
