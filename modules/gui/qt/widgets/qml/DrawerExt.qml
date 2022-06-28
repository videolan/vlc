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

import "qrc:///style/"

FocusScope {
    id: root

    enum Edges {
        Top,
        Bottom,
        Left,
        Right
    }

    property int edge: DrawerExt.Edges.Bottom
    property alias contentItem: content.item
    property alias component: content.sourceComponent

    property bool _expandHorizontally: edge === DrawerExt.Edges.Left || edge === DrawerExt.Edges.Right
    property int _size: _expandHorizontally ? content.item.width : content.item.height
    property string _toChange: _expandHorizontally ? "x" : "y"

    width: _expandHorizontally ? root._size : undefined
    height: !_expandHorizontally ? root._size : undefined


    Loader {
        id: content

        anchors.left: !_expandHorizontally ? parent.left : undefined
        anchors.right: !_expandHorizontally ? parent.right : undefined
        anchors.top: _expandHorizontally ? parent.top : undefined
        anchors.bottom: _expandHorizontally ? parent.bottom : undefined

        focus: true
    }

    state: "hidden"
    states: [
        State {
            name: "visible"
            PropertyChanges {
                target: content
                y: 0
                x: 0
                visible: true
            }
        },
        State {
            name: "hidden"
            PropertyChanges {
                target: content
                y: root.edgeToOffset(root.edge)
                x: root.edgeToOffset(root.edge)
                visible: false
            }
        }
    ]

    function edgeToOffset(edge){
        if (root._expandHorizontally) {
            switch (edge) {
            case DrawerExt.Edges.Left:
                return -root._size
            case DrawerExt.Edges.Right:
                return root._size
            default:
                return 0
            }
        }  else {
            switch (edge) {
            case DrawerExt.Edges.Top:
                return -root._size
            case DrawerExt.Edges.Bottom:
                return root._size
            default:
                return 0
            }
        }
    }

    transitions: [
        Transition {
            to: "hidden"
            SequentialAnimation {
                NumberAnimation {
                    target: content
                    property: root._toChange

                    duration: VLCStyle.duration_short
                    easing.type: Easing.InSine
                }

                PropertyAction{
                    target: content
                    property: "visible"
                }
            }
        },
        Transition {
            to: "visible"
            SequentialAnimation {
                PropertyAction {
                    target: content
                    property: "visible"
                }

                NumberAnimation {
                    target: content
                    property: root._toChange

                    duration: VLCStyle.duration_short
                    easing.type: Easing.OutSine
                }
            }
        }
    ]
}
