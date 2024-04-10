/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

import "qrc:///style/"

StateGroup {
    id: root

    state: target?.state ?? ""

    property Item target

    states: [
        State {
            name: "hidden"

            PropertyChanges {
                target: root.target

                visible: false
                opacity: 0.0
            }
        },
        State {
            name: "visible"

            PropertyChanges {
                target: root.target

                visible: true
                opacity: 1.0
            }
        }
    ]

    transitions: [
        Transition {
            to: "hidden"

            SequentialAnimation {
                NumberAnimation {
                    property: "opacity"

                    duration: VLCStyle.duration_long
                    easing.type: Easing.InSine
                }

                PropertyAction {
                    property: "visible"
                }
            }
        },
        Transition {
            to: "visible"

            SequentialAnimation {
                PropertyAction {
                    property: "visible"
                }

                NumberAnimation {
                    property: "opacity"

                    duration: VLCStyle.duration_long
                    easing.type: Easing.OutSine
                }
            }
        }
    ]
}
