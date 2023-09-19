/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

/*
 * Custom StackView with brief transitions and helper to load view from the history
 */
StackView {
    id: root

    // Functions

    function setCurrentItemFocus(reason) {
        if (reason === Qt.OtherFocusReason)
            return
        focus = true
        focusReason = reason
        if (typeof currentItem.setCurrentItemFocus === "function")
            currentItem.setCurrentItemFocus(reason)

    }

    // Settings

    replaceEnter: null

    replaceExit: null

    //don't report this node
    Accessible.ignored: true

    // Events

    onCurrentItemChanged: {
        if (currentItem === null)
            return

        // NOTE: When the currentItem has a padding defined we propagate the StackView values.

        if (currentItem.leftPadding !== undefined)
        {
            currentItem.leftPadding = Qt.binding(function() {
                return leftPadding
            })
        }

        if (currentItem.rightPadding !== undefined)
        {
            currentItem.rightPadding = Qt.binding(function() {
                return rightPadding
            })
        }
    }
}
