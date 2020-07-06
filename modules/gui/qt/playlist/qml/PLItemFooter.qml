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
import QtQuick 2.11

import "qrc:///style/"

Item {
    id: foot

    property int listCount

    signal dropURLAtEnd(var urlList)
    signal moveAtEnd()

    property bool _dropVisible: false

    width: parent.width
    height: Math.max(VLCStyle.icon_normal, view.height - y)

    Rectangle {
        width: parent.width
        anchors.top: parent.top
        antialiasing: true
        height: 2
        visible: foot._dropVisible
        color: VLCStyle.colors.accent
    }

    DropArea {
        anchors { fill: parent }
        onEntered: {
            if(drag.source.model.index === foot.listCount - 1)
                return

            foot._dropVisible = true
        }
        onExited: {
            if(drag.source.model.index === foot.listCount - 1)
                return

            foot._dropVisible = false
        }
        onDropped: {
            if(drag.source.model.index === foot.listCount - 1)
                return

            if (drop.hasUrls) {
                //force conversion to an actual list
                var urlList = []
                for ( var url in drop.urls)
                    urlList.push(drop.urls[url])
                dropURLAtEnd(urlList)
            } else {
                moveAtEnd()
            }
            drop.accept()
            foot._dropVisible = false
        }
    }
}
