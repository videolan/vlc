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

import VLC.Style

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
            }
        },
        State {
            name: "visible"

            PropertyChanges {
                target: root.target

                visible: true
            }
        }
    ]

    transitions: [
        Transition {
            to: "hidden"

            SequentialAnimation {
                OpacityAnimator {
                    duration: VLCStyle.duration_long
                    easing.type: Easing.InSine
                    from: 1.0 // QTBUG-66475
                    to: 0.0
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

                OpacityAnimator {
                    duration: VLCStyle.duration_long
                    easing.type: Easing.OutSine
                    from: 0.0 // QTBUG-66475
                    to: 1.0
                }
            }
        }
    ]
}
