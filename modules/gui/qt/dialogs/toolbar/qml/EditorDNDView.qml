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
import QtQuick.Controls 2.4
import QtQml.Models 2.11

import org.videolan.vlc 0.1

import "qrc:///style/"

ListView {
    id: playerBtnDND
    spacing: VLCStyle.margin_xxsmall
    orientation: Qt.Horizontal
    clip: true
    property bool deleteBtn: false
    property bool addBtn: false
    onDeleteBtnChanged: {
        if(deleteBtn)
            toolbareditor.deleteCursor()
        else
            toolbareditor.restoreCursor()
    }

    ScrollBar.horizontal: ScrollBar {}

    footer: Item {
        height: VLCStyle.icon_medium
        width: height
        anchors.verticalCenter: parent.verticalCenter
        property bool dropVisible: false
        Rectangle {
            z: 2
            width: VLCStyle.dp(2)
            height: parent.height
            anchors {
                left: parent.left
            }
            antialiasing: true
            visible: dropVisible
            color: VLCStyle.colors.accent
        }
        DropArea {
            anchors.fill: parent

            onEntered: {
                dropVisible = true
                playerBtnDND.deleteBtn = false
            }

            onExited: {
                dropVisible = false
                playerBtnDND.deleteBtn = true
            }

            onDropped: {
                if (drag.source.objectName == "buttonsList"){
                    playerBtnDND.model.insert(playerBtnDND.count,
                                             {"id" : drag.source.mIndex,
                                                 "size": PlayerControlBarModel.WIDGET_NORMAL})
                }
                else
                    playerBtnDND.model.move(
                                drag.source.DelegateModel.itemsIndex,
                                playerBtnDND.count-1)
                dropVisible = false
            }
        }

    }

    delegate: EditorDNDDelegate {
        dndView: playerBtnDND
    }
    highlight: Rectangle{
        anchors.verticalCenter: currentIndex > 0 ? parent.verticalCenter : undefined
        color: VLCStyle.colors.bgHover
    }

    highlightMoveDuration: 0 //ms
    highlightResizeDuration: 0 //ms
}
