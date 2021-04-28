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
import QtQuick.Templates 2.4 as T

import "qrc:///style/"

T.ProgressBar {
    id: control

    from: 0
    to: 100
    value: medialib.parsingProgress
    indeterminate: medialib.discoveryPending
    visible: !medialib.idle
    height: contentItem.implicitHeight
    width: implicitWidth

    contentItem: Column {
        spacing: VLCStyle.margin_normal
        width: control.width
        height: implicitHeight
        visible: control.visible

        Rectangle {
            id: bg

            width: parent.width
            height: VLCStyle.heightBar_xsmall
            color: VLCStyle.colors.bgAlt

            function fromRGB(r, g, b, a) {
                return Qt.rgba( r / 255, g / 255, b / 255, a / 255)
            }

            Rectangle {
                id: progressBar

                width: parent.height
                height: parent.width * control.visualPosition
                rotation: -90
                y: width
                transformOrigin: Item.TopLeft
                visible: !control.indeterminate
                gradient: Gradient {
                    GradientStop { position: 0.00; color: bg.fromRGB(248, 154, 6, 200) }
                    GradientStop { position: 0.48; color: bg.fromRGB(226, 90, 1, 255) }
                    GradientStop { position: 0.89; color: bg.fromRGB(248, 124, 6, 255) }
                    GradientStop { position: 1.00; color: bg.fromRGB(255, 136, 0, 100) }
                }
            }

            Rectangle {
                id: indeterminateBar

                property real pos: 0

                anchors.centerIn: parent
                anchors.horizontalCenterOffset: ((bg.width - indeterminateBar.height) / 2) * pos
                width: parent.height
                height: parent.width * .24
                rotation: 90
                visible: control.indeterminate
                gradient: Gradient {
                    GradientStop { position: 0.00; color: bg.fromRGB(248, 154, 6, 0) }
                    GradientStop { position: 0.09; color: bg.fromRGB(253, 136, 0, 0) }
                    GradientStop { position: 0.48; color: bg.fromRGB(226, 90, 1, 255) }
                    GradientStop { position: 0.89; color: bg.fromRGB(248, 124, 6, 255) }
                    GradientStop { position: 1.00; color: bg.fromRGB(255, 136, 0, 0) }
                }

                SequentialAnimation on pos {
                    id: loadingAnim

                    loops: Animation.Infinite
                    running: control.indeterminate

                    NumberAnimation {
                        from: - 1
                        to: 1
                        duration: 2000
                        easing.type: Easing.OutBounce
                    }

                    PauseAnimation {
                        duration: 500
                    }

                    NumberAnimation {
                        to: - 1
                        from: 1
                        duration: 2000
                        easing.type: Easing.OutBounce
                    }

                    PauseAnimation {
                        duration: 500
                    }
                }
            }
        }

        SubtitleLabel {
            text:  medialib.discoveryPending ? medialib.discoveryEntryPoint : (medialib.parsingProgress + "%")
            font.weight: Font.Normal
            width: parent.width
        }
    }
}
