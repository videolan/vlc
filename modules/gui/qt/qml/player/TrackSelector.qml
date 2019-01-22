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
import QtQuick.Layouts 1.3
import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///utils/" as Utils

Utils.NavigableFocusScope {
    id: root

    property bool noAutoHide: true

    Component {
        id: delegateComponent

        Row {
            focus:  true
            spacing: VLCStyle.margin_small
            opacity: 1.0 - Math.abs(Tumbler.displacement) / (Tumbler.tumbler.visibleItemCount / 2)
            height: VLCStyle.fontHeight_large + VLCStyle.margin_small

            Label {
                id: checbock_id
                font.pixelSize: VLCStyle.fontSize_large
                anchors.verticalCenter:  parent.verticalCenter
                visible: model.checked
                color: (Tumbler.tumbler.activeFocus && model.index === Tumbler.tumbler.currentIndex ) ? VLCStyle.colors.accent : VLCStyle.colors.text
                font.bold: true
                text: "☑"
            }
            Label {
                id: label_id
                text: model.display
                anchors.verticalCenter:  parent.verticalCenter
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                font.pixelSize: (model.index === Tumbler.tumbler.currentIndex) ? VLCStyle.fontSize_large * 1.3 : VLCStyle.fontSize_large
                color: (Tumbler.tumbler.activeFocus && model.index === Tumbler.tumbler.currentIndex ) ? VLCStyle.colors.accent : VLCStyle.colors.text
                font.bold: true
            }
            Keys.onPressed:  {
                if (event.key === Qt.Key_Space || event.matches(StandardKey.InsertParagraphSeparator)) { //enter/return/space
                    model.checked  = !model.checked
                }
            }
        }
    }

    Utils.IconToolButton {
        id: back
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        size: VLCStyle.icon_large
        text: VLCIcons.exit
        onClicked: root.actionCancel(0)
        KeyNavigation.right: trackTypeTumbler
    }

    Tumbler {
        id: trackTypeTumbler
        anchors.right: trackTumble.left
        anchors.verticalCenter: parent.verticalCenter
        //fixme hardcoded
        width: 100 * VLCStyle.scale
        height: parent.height

        //Layout.fillHeight: true
        //Layout.preferredWidth: root.width/4
        //Layout.alignment: Qt.AlignCenter
        focus: true
        model: ListModel {
            ListElement { display: qsTr("subtitle") ; checked: false }
            ListElement { display: qsTr("audio")    ; checked: false }
            ListElement { display: qsTr("video")    ; checked: false }
        }
        delegate: delegateComponent
        currentIndex: 0

        //Keys.forwardTo: [trackTypeTumbler.currentItem]
        KeyNavigation.right: trackTumble
    }
    //
    Tumbler {
        id: trackTumble

        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenter:  parent.horizontalCenter
        height: parent.height
        width: parent.width / 2

        //Layout.fillHeight: true
        //Layout.preferredWidth: root.width/4
        //Layout.alignment: Qt.AlignCenter
        model: (trackTypeTumbler.currentIndex === 0)
               ? player.subtitleTracks
               : ((trackTypeTumbler.currentIndex === 1)
                  ? player.audioTracks
                  : player.videoTracks
                  )
        delegate: delegateComponent
        Keys.forwardTo: [trackTumble.currentItem]
    }

    Keys.onPressed: defaultKeyAction(event, 0)
}
