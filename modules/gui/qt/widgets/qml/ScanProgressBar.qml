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
    visible: !medialib.idle

    id: control
    from: 0
    to: 100
    height: progressText_id.height
    anchors.topMargin: 10
    anchors.bottomMargin: 10
    value: medialib.parsingProgress
    indeterminate: medialib.discoveryPending

    contentItem: Item {
        implicitHeight: 24
        implicitWidth: 120

        Rectangle {
            width: control.indeterminate ?  parent.width : (control.visualPosition * parent.width)
            height: parent.height
            color: VLCStyle.colors.bgAlt

            Rectangle {
                width: control.width * 0.2
                height: parent.height
                visible: control.indeterminate
                color: VLCStyle.colors.buffer

                property double pos: 0
                x: (pos / 1000) * (parent.width * 0.8)

                SequentialAnimation on pos {
                    id: loadingAnim
                    running: control.indeterminate
                    loops: Animation.Infinite
                    PropertyAnimation {
                        from: 0.0
                        to: 1000
                        duration: 2000
                        easing.type: "OutBounce"
                    }
                    PauseAnimation {
                        duration: 500
                    }
                    PropertyAnimation {
                        from: 1000
                        to: 0.0
                        duration: 2000
                        easing.type: "OutBounce"
                    }
                    PauseAnimation {
                        duration: 500
                    }
                }
            }
        }
    }

    background: Rectangle {
        implicitHeight: 24
        implicitWidth: 120
        radius: 2
        color: VLCStyle.colors.bg
    }

    Text {
        id: progressText_id
        color: VLCStyle.colors.text
        style: Text.Outline
        styleColor: VLCStyle.colors.bg
        text:  medialib.discoveryPending ? medialib.discoveryEntryPoint : (medialib.parsingProgress + "%")
        z: control.z + 1
        anchors.horizontalCenter: parent.horizontalCenter
        visible: true
    }
}
