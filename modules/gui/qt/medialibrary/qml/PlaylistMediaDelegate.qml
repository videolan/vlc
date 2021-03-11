/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

import QtQuick 2.11

import "qrc:///widgets/" as Widgets

Widgets.TableViewDelegate {
    id: delegate

    //---------------------------------------------------------------------------------------------
    // Functions
    //---------------------------------------------------------------------------------------------

    function _applyPosition(drag)
    {
        if (root.isDroppable(drag, index) === false) {

            root.hideLine(delegate);

            return;
        }

        if (index === _getDropIndex(drag.y))
            root.showLine(delegate, true);
        else
            root.showLine(delegate, false);
    }

    //---------------------------------------------------------------------------------------------

    function _getDropIndex(y)
    {
        var size = Math.round(height / 2);

        if (y < size)
            return index;
        else
            return index + 1;
    }

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    // NOTE: We are usng a single DropArea and a single line Rectangle in PlaylistMedia.
    DropArea {
        anchors.fill: parent

        onEntered: _applyPosition(drag)

        onPositionChanged: _applyPosition(drag)

        onExited: root.hideLine(delegate)

        onDropped: {
            if (isDroppable(drop, index) === false) {
                root.hideLine(delegate);

                return;
            }

            root.applyDrop(drop, _getDropIndex(drag.y));
        }
    }
}
