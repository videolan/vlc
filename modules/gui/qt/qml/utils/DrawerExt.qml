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

NavigableFocusScope {
    id: root

    property Component component: undefined

    //readonly property int horizontal: 0
    //readonly property int vertical: 1

    property bool expandHorizontally: true
    width:  (root.expandHorizontally) ? root._size : undefined
    height: (!root.expandHorizontally) ? root._size : undefined
    property int _size: 0

    property alias contentItem: content.item

    Flickable {
        anchors.fill: parent
        Loader {
            focus: true
            id: content
            sourceComponent: root.component
        }
    }

    state: "hidden"
    states: [
        State {
            name: "visible"
            PropertyChanges {
                target: root
                _size: (root.expandHorizontally) ?  content.item.width : content.item.height
                visible: true
            }
        },
        State {
            name: "hidden"
            PropertyChanges {
                target: root
                _size: 0
                visible: false
            }
        }
    ]
    transitions: [
        Transition {
            to: "hidden"
            SequentialAnimation {
                NumberAnimation { target: root; property: "_size"; duration: 200 }
                PropertyAction{ target: root; property: "visible" }
            }
        },
        Transition {
            to: "visible"
            SequentialAnimation {
                PropertyAction{ target: root; property: "visible" }
                NumberAnimation { target: root; property: "_size"; duration: 200 }
            }
        }
    ]
}
