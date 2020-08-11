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

    enum Edges {
        Top,
        Bottom,
        Left,
        Right
    }

    property int edge: DrawerExt.Edges.Bottom
    property bool expandHorizontally: edge === DrawerExt.Edges.Left || edge === DrawerExt.Edges.Right

    property alias contentItem: content.item

    width:  (root.expandHorizontally) ? root._size : undefined
    height: (!root.expandHorizontally) ? root._size : undefined

    property int _size: (root.expandHorizontally) ?  content.item.width : content.item.height
    property string toChange: expandHorizontally ? "contentX" : "contentY"

    Flickable {
        id: container
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
                target: container
                contentY: 0
                contentX: 0
                visible: true
            }
        },
        State {
            name: "hidden"
            PropertyChanges {
                target: container
                contentY: edgeToOffset(edge)
                contentX: edgeToOffset(edge)
                visible:false
            }
        }
    ]

    function edgeToOffset(edge){
        if(expandHorizontally)
            switch(edge){
            case DrawerExt.Edges.Left: return _size
            case DrawerExt.Edges.Right: return -_size
            default: return 0
            }
        else
            switch(edge){
            case DrawerExt.Edges.Top: return _size
            case DrawerExt.Edges.Bottom: return -_size
            default: return 0
            }
    }

    transitions: [
        Transition {
            to: "hidden"
            SequentialAnimation {
                NumberAnimation { target: container; property: toChange; duration: 150; easing.type: Easing.InSine}
                PropertyAction{ target: container; property: "visible" }
            }
        },
        Transition {
            to: "visible"
            SequentialAnimation {
                PropertyAction{ target: container; property: "visible" }
                NumberAnimation { target: container; property: toChange; duration: 150; easing.type: Easing.OutSine}
            }
        }
    ]
}
